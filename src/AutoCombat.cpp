#include "AutoCombat.h"
#include "NativeTracks.h"
#include "AudioCoexist.h"
#include "core/PluginScan.h"
#include "core/Conditions.h"
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <algorithm>
#include <bit>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace UnifiedMusic {
namespace {
using Track = RE::BGSMusicTrackFormWrapper;
using Node = RE::TESConditionItem;

std::string Lower(std::string_view text)
{
    std::string result(text);
    for (auto& c:result) if (c>='A' && c<='Z') c+=32;
    return result;
}

std::optional<RE::FormType> ParameterType(std::uint32_t function)
{
    switch (function) {
    case 56: case 58: case 59: return RE::FormType::Quest;
    case 74: return RE::FormType::Global;
    case 67: return RE::FormType::Cell;
    case 310: return RE::FormType::WorldSpace;
    case 359: return RE::FormType::Location;
    case 117: return RE::FormType::Region;
    case 562: case 707: return RE::FormType::Keyword;
    default: return {};
    }
}

bool Contains(const RE::BGSMusicType* type, Track* track)
{
    return std::ranges::find(type->tracks,static_cast<RE::BSIMusicTrack*>(track))!=type->tracks.end();
}

bool Matches(const Node* node,const ConditionTerm& expected,RE::TESDataHandler* data)
{
    if (!node || expected.opcode>5) return false;
    const auto& actual=node->data;
    const auto type=ParameterType(expected.function);
    if ((!type && expected.function!=18 && expected.function!=520) || (expected.function!=59 && expected.parameter2)) return false;
    auto* parameter=type?data->LookupForm<RE::TESForm>(expected.parameter.id,expected.parameter.plugin):nullptr;
    if (type && (!parameter || parameter->GetFormType()!=*type)) return false;
    if (actual.flags.isOR!=expected.isOR || actual.flags.usesAliases || actual.flags.global ||
        actual.flags.usePackData || actual.flags.swapTarget ||
        static_cast<std::uint8_t>(actual.flags.opCode)!=expected.opcode ||
        actual.functionData.function.underlying()!=expected.function ||
        std::bit_cast<std::uint32_t>(actual.comparisonValue.f)!=expected.comparison ||
        actual.functionData.params[0]!=parameter ||
        reinterpret_cast<std::uintptr_t>(actual.functionData.params[1])!=expected.parameter2) return false;
    if (expected.onPlayerReference) {
        auto* player=data->LookupForm<RE::TESObjectREFR>(0x14,"skyrim.esm");
        if (!player || actual.object!=RE::CONDITIONITEMOBJECT::kRef || actual.runOnRef.get().get()!=player) return false;
    } else if (actual.object!=RE::CONDITIONITEMOBJECT::kSelf || actual.runOnRef) {
        return false;
    }
    // dataID and padding do not participate in this supported Self/Ref schema.
    return true;
}

bool Matches(const RE::TESCondition* conditions,const ConditionVersion& expected,RE::TESDataHandler* data,Node*& tail)
{
    // Exact length bounds traversal and also rejects cycles without evaluating
    // conditions or interpreting unrelated native memory as serialized CTDA.
    if (expected.conditions.size()>4096) return false;
    auto* node=conditions?conditions->head:nullptr;
    tail=nullptr;
    for (const auto& term:expected.conditions) {
        if (!Matches(node,term,data)) return false;
        tail=node;
        node=node->next;
    }
    return node==nullptr;
}

const ConditionVersion* WinningVersion(Track* track,const std::vector<ConditionVersion>& versions)
{
    auto* source=track->GetFile();
    if (!source) return nullptr;
    const auto name=Lower(source->GetFilename());
    const ConditionVersion* match=nullptr;
    for (const auto& version:versions) if (version.source==name) {
        if (match) return nullptr;
        match=&version;
    }
    return match;
}

struct PreparedRepair {
    RE::BGSMusicTrack* track;
    Node* tail;
    const ConditionVersion* version;
    std::vector<std::unique_ptr<Node>> nodes;
    PrivateSingle isolated;
    Track* original{};
    RE::BGSMusicType* owner{};
    std::unique_ptr<RE::TESCondition> newConditions;
};

bool PlaylistMatches(RE::BGSMusicType* type,const std::vector<MusicRecord>& versions,RE::TESDataHandler* data) {
    if(!type || !type->GetFile()) return false;
    const auto source=Lower(type->GetFile()->GetFilename());
    const MusicRecord* snapshot=nullptr;
    for(const auto& version:versions) if(version.source==source) {
        if(snapshot) return false;
        snapshot=&version;
    }
    if(!snapshot || snapshot->deleted || snapshot->tracks.size()!=type->tracks.size()) return false;
    std::multiset<RE::BSIMusicTrack*> expected;
    for(const auto& key:snapshot->tracks) {
        auto* track=data->LookupForm<Track>(key.id,key.plugin);
        if(!track) return false;
        expected.insert(track);
    }
    return expected==std::multiset<RE::BSIMusicTrack*>(type->tracks.begin(),type->tracks.end());
}
}

static bool ApplyMusicBatch(const MergePlan& plan,const std::map<FormKey,std::vector<MusicRecord>>& records,AudioInputs& audio)
{
    const auto fail=[](std::string_view reason) {
        REX::WARN("Music batch skipped: {}",reason);
        return false;
    };
    std::map<FormKey,std::set<FormKey>> contributions;
    for(const auto* source:{&plan.additions,&plan.distributions}) for(const auto& [type,tracks]:*source)
        if(!tracks.empty()) contributions[type].insert(tracks.begin(),tracks.end());
    for(const auto& type:plan.clearedTypes) contributions.try_emplace(type);
    if(plan.battleType) for(const auto& rule:plan.combatRules)
        contributions[*plan.battleType].insert(rule.tracks.begin(),rule.tracks.end());
    for(const auto& repair:plan.conditionRepairs) contributions.try_emplace(repair.musicType);
    for(const auto& candidate:plan.audioCandidates) contributions.try_emplace(candidate.musicType);
    if(contributions.empty()) return true;
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return fail("data handler unavailable");
    if(!audio.SceneResourcesMatch(plan)) return fail("scene resources unavailable; keeping the entire affected use unchanged");
    const bool battleWork=plan.battleType && contributions.contains(*plan.battleType);
    if(battleWork && plan.battleMergeSafe) {
        auto* objects=RE::BGSDefaultObjectManager::GetSingleton();
        if(!objects || !plan.battleType || !plan.battleMergeSafe) return fail("unavailable or protected default battle list");
        auto* target=data->LookupForm<RE::BGSMusicType>(plan.battleType->id,plan.battleType->plugin);
        if(!target || target!=objects->GetObject<RE::BGSMusicType>(RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic))
            return fail("engine default differs from the original battle type");
    }

    // Prepare all list additions and condition nodes before either is applied.
    struct Playlist {
        RE::BGSMusicType* target;
        std::vector<Track*> pending;
        bool clear{};
        std::vector<RE::BSIMusicTrack*> snapshot;
    };
    const bool automatic=std::ranges::any_of(plan.additions,[](const auto& entry){return !entry.second.empty();}) ||
        !plan.combatRules.empty() || !plan.conditionRepairs.empty() || !plan.audioCandidates.empty();
    std::map<FormKey,Playlist> playlists;
    for(const auto& [type,tracks]:contributions) {
        auto* target=data->LookupForm<RE::BGSMusicType>(type.id,type.plugin);
        const auto found=records.find(type);
        if(!target || found==records.end()) return fail("unresolved target MusicType");
        if(automatic && !PlaylistMatches(target,found->second,data)) return fail("native playlist differs from winning plugin snapshot");
        // Explicit MTD operations work on the loaded list, including changes
        // made by an earlier runtime distributor. Only inference needs the ESP
        // snapshot; all operations still recheck their actual pre-commit state.
        auto& list=playlists.emplace(type,Playlist{target,{},plan.clearedTypes.contains(type),
            {target->tracks.begin(),target->tracks.end()}}).first->second;
        for(const auto& key:tracks) {
            auto* track=data->LookupForm<Track>(key.id,key.plugin);
            // Explicit distributions may contain palettes/silence. Only
            // condition edits below require single tracks.
            if(!track || !track->track) return fail("unresolved/unusable contributed track");
            if(list.clear || !Contains(target,track)) list.pending.push_back(track);
        }
    }
    const auto present=[&](const FormKey& type,Track* track) {
        const auto& list=playlists.at(type);
        return (!list.clear && Contains(list.target,track)) || std::ranges::find(list.pending,track)!=list.pending.end();
    };

    const auto witnessesMatch=[&] {
        for (const auto& witness:plan.conditionWitnesses) {
            auto* wrapper=data->LookupForm<Track>(witness.track.id,witness.track.plugin);
            if (!wrapper || !wrapper->track || wrapper->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack) return false;
            const auto* version=WinningVersion(wrapper,witness.versions);
            Node* tail=nullptr;
            if (!version || !Matches(TrackConditions(wrapper->track),*version,data,tail)) return false;
        }
        return true;
    };
    if (!witnessesMatch()) return fail("regional policy or replacement changed at runtime");

    std::vector<PreparedRepair> repairs;
    std::size_t conditionCount=0;
    for (const auto& repair:plan.conditionRepairs) {
        auto* wrapper=data->LookupForm<Track>(repair.track.id,repair.track.plugin);
        if (!wrapper || !wrapper->track || wrapper->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack ||
            !present(repair.musicType,wrapper))
            return fail("condition target is not a current/contributed single track");
        for (const auto& required:repair.requiredTracks) {
            auto* track=data->LookupForm<Track>(required.id,required.plugin);
            if (!track || !track->track || !present(repair.musicType,track))
                return fail("replacement contribution missing; preserving conditions to avoid silence");
        }
        const auto* version=WinningVersion(wrapper,repair.versions);
        if (!version) return fail("unrecognized winning condition source");
        const auto* originalConditions=TrackConditions(wrapper->track);
        PreparedRepair prepared{wrapper->track,nullptr,version,{}};
        if (!Matches(originalConditions,*version,data,prepared.tail))
            return fail("runtime conditions differ from the scanned winning record");
        prepared.original=wrapper;
        prepared.owner=playlists.at(repair.musicType).target;
        {
            // Every inferred mutation belongs to one playlist use, including
            // original tracks whose other owners were not visible to our scan.
            prepared.isolated=CloneSingle(wrapper->track);
            if(!prepared.isolated) return fail("could not create an independent native single track");
            prepared.track=prepared.isolated.get();
            const auto* copied=TrackConditions(prepared.track);
            if(!Matches(copied,*version,data,prepared.tail)) return fail("private conditions differ from original");
        }
        if (std::ranges::any_of(repairs,[&](const auto& prior) {
            return prior.track==prepared.track || (prepared.tail && prior.tail==prepared.tail);
        })) return fail("condition chains have shared ownership");

        const auto clauses=RepairClauses(repair);
        auto effective=version->conditions;
        for(const auto& clause:clauses) {
            if(clause.empty() || HasClause(effective,clause)) continue;
            for(const auto& term:clause) {
                const auto type=ParameterType(term.function);
                auto* form=type?data->LookupForm<RE::TESForm>(term.parameter.id,term.parameter.plugin):nullptr;
                if((!type && term.function!=18) || (type && (!form || form->GetFormType()!=*type))) return fail("unresolved scene condition parameter");
                auto* memory=RE::malloc(sizeof(Node));
                if(!memory) return fail("condition allocation failed");
                std::unique_ptr<Node> node(std::construct_at(static_cast<Node*>(memory)));
                node->data.functionData.function=static_cast<RE::FUNCTION_DATA::FunctionID>(term.function);
                node->data.functionData.params[0]=form;
                node->data.functionData.params[1]=reinterpret_cast<void*>(static_cast<std::uintptr_t>(term.parameter2));
                node->data.comparisonValue.f=std::bit_cast<float>(term.comparison);
                node->data.flags.opCode=static_cast<RE::CONDITION_ITEM_DATA::OpCode>(term.opcode);
                node->data.flags.isOR=term.isOR;
                if(term.onPlayerReference) {
                    auto* player=data->LookupForm<RE::TESObjectREFR>(0x14,"skyrim.esm");
                    if(!player) return fail("player reference unavailable");
                    node->data.object=RE::CONDITIONITEMOBJECT::kRef;node->data.runOnRef=player->CreateRefHandle();
                }
                prepared.nodes.push_back(std::move(node));
            }
            effective.insert(effective.end(),clause.begin(),clause.end());
        }
        conditionCount+=prepared.nodes.size();
        if(!prepared.nodes.empty() && !TrackConditions(prepared.track)) {
            auto* memory=RE::malloc(sizeof(RE::TESCondition));
            if(!memory) return fail("condition holder allocation failed");
            prepared.newConditions.reset(std::construct_at(static_cast<RE::TESCondition*>(memory)));
        }
        // Attach only to the private instance now. Audio normalization can copy
        // the complete effective conditions before any game list is committed.
        if(!prepared.nodes.empty()) {
            for(std::size_t i=1;i<prepared.nodes.size();++i) prepared.nodes[i-1]->next=prepared.nodes[i].get();
            auto* first=prepared.nodes.front().get();
            if(prepared.newConditions) SetTrackConditions(prepared.track,prepared.newConditions.release());
            if(prepared.tail) prepared.tail->next=first;else TrackConditions(prepared.track)->head=first;
            for(auto& node:prepared.nodes) node.release();
        }
        repairs.push_back(std::move(prepared));
    }

    std::map<FormKey,std::vector<PrivateSingle>> audioUses;
    for(const auto& candidate:plan.audioCandidates) {
        auto* wrapper=data->LookupForm<Track>(candidate.track.id,candidate.track.plugin);
        if(!wrapper || !wrapper->track || !present(candidate.musicType,wrapper)) continue;
        const RE::BGSMusicTrack* effective=wrapper->track;
        for(const auto& repair:repairs) if(repair.original==wrapper && repair.owner==playlists.at(candidate.musicType).target)
            effective=repair.track;
        const auto& list=playlists.at(candidate.musicType);
        std::vector<RE::BSIMusicTrack*> presentTracks;
        if(!list.clear) presentTracks=list.snapshot;
        for(auto* track:list.pending) presentTracks.push_back(track);
        for(const auto& use:audioUses[candidate.musicType]) presentTracks.push_back(use.get());
        auto original=audio.Original(candidate,effective,presentTracks);
        if(original) audioUses[candidate.musicType].push_back(std::move(original));
    }
    for(const auto& [key,list]:playlists) {
        const auto count=list.pending.size()+audioUses[key].size();
        if(count>std::numeric_limits<std::uint32_t>::max()-list.target->tracks.size()) return fail("playlist capacity exceeded");
        list.target->tracks.reserve(static_cast<std::uint32_t>(list.target->tracks.size()+count));
    }
    // No logical list mutation precedes this point. Validate once more before
    // the non-allocating commit; never replace or delete existing condition nodes.
    if (!witnessesMatch()) return fail("regional policy changed during preparation");
    if(!audio.SceneResourcesMatch(plan)) return fail("scene resources changed during preparation");
    for(const auto& [key,list]:playlists)
        if(!std::ranges::equal(list.target->tracks,list.snapshot)) return fail("playlist changed during preparation");
    for (const auto& repair:repairs) {
        Node* tail=nullptr;
        if(!Matches(TrackConditions(repair.original->track),*repair.version,data,tail))
            return fail("shared source conditions changed during preparation");
    }
    std::size_t appended=0,cleared=0;
    for(const auto& [key,list]:playlists) {
        // MTD's established clear-then-append operation, after preparation.
        // Only references in this playlist are removed; shared tracks survive.
        if(list.clear) {list.target->tracks.clear();++cleared;}
        for(auto* track:list.pending) {list.target->tracks.push_back(track);++appended;}
    }
    std::size_t repairedTracks=0,isolatedTracks=0;
    for (auto& repair:repairs) {
        if (repair.nodes.empty()) continue;
        if(repair.isolated) {
            // Scope changes to this list only; other lists, palettes and scripts
            // retain the original wrapper and conditions. No persistent FormID.
            for(auto*& item:repair.owner->tracks)
                if(item==static_cast<RE::BSIMusicTrack*>(repair.original)) item=repair.isolated.get();
            repair.isolated.release(); // bounded startup allocation, engine-lifetime
            ++isolatedTracks;
        }
        ++repairedTracks;
    }
    std::size_t normalized=0;
    for(auto& [key,uses]:audioUses) for(auto& use:uses) {playlists.at(key).target->tracks.push_back(use.release());++normalized;}
    REX::INFO("Music batch: {} cleared lists, {} contributed tracks, {} condition lists, {} condition terms, {} local instances, {} normalized audio inputs",cleared,appended,repairedTracks,conditionCount,isolatedTracks,normalized);
    return true;
}

bool ApplyMusicPlan(const MergePlan& plan,const std::map<FormKey,std::vector<MusicRecord>>& records,const std::map<FormKey,std::vector<TrackRecord>>& tracks,std::set<FormKey>& failed) {
    AudioInputs audio(!plan.audioCandidates.empty(),tracks);
    std::set<FormKey> types=plan.clearedTypes;
    for(const auto* map:{&plan.additions,&plan.distributions}) for(const auto& [type,tracks]:*map) if(!tracks.empty()) types.insert(type);
    if(plan.battleType && !plan.combatRules.empty()) types.insert(*plan.battleType);
    for(const auto& repair:plan.conditionRepairs) types.insert(repair.musicType);
    if(audio.Enabled()) for(const auto& candidate:plan.audioCandidates) types.insert(candidate.musicType);
    bool complete=true;
    for(const auto& type:types) {
        MergePlan batch;batch.battleType=plan.battleType;batch.battleMergeSafe=plan.battleMergeSafe;
        if(plan.clearedTypes.contains(type)) batch.clearedTypes.insert(type);
        if(auto it=plan.additions.find(type);it!=plan.additions.end()) batch.additions.emplace(*it);
        if(auto it=plan.distributions.find(type);it!=plan.distributions.end()) batch.distributions.emplace(*it);
        if(plan.battleType && type==*plan.battleType) batch.combatRules=plan.combatRules;
        for(const auto& r:plan.conditionRepairs) if(r.musicType==type) batch.conditionRepairs.push_back(r);
        for(const auto& r:plan.conditionWitnesses) if(r.musicType==type) batch.conditionWitnesses.push_back(r);
        for(const auto& r:plan.sceneResources) if(r.musicType==type) batch.sceneResources.push_back(r);
        if(audio.Enabled()) for(const auto& r:plan.audioCandidates) if(r.musicType==type) batch.audioCandidates.push_back(r);
        if(const auto it=plan.sceneUses.find(type);it!=plan.sceneUses.end()) batch.sceneUses.emplace(*it);
        try {
            if(!ApplyMusicBatch(batch,records,audio)) {
                complete=false;
                bool recovered=false;
                // Explicit failure returns before commit. Retry only actions
                // independent of the scene, such as dragon/Boss distributions.
                if(batch.sceneUses.contains(type)) {
                    DropSceneUses(batch,type);
                    REX::INFO("Scene preparation failed in {}; retaining independent uses",ToString(type));
                    recovered=ApplyMusicBatch(batch,records,audio);
                }
                if(!recovered && (batch.distributions.contains(type) || batch.clearedTypes.contains(type))) {
                    MergePlan explicitPlan;
                    explicitPlan.distributions=batch.distributions;
                    explicitPlan.clearedTypes=batch.clearedTypes;
                    REX::INFO("Automatic preparation failed in {}; applying only explicit MTD instructions",ToString(type));
                    recovered=ApplyMusicBatch(explicitPlan,records,audio);
                }
                if(!recovered) failed.insert(type);
            }
        }
        catch(const std::exception& e) {REX::WARN("Music use batch {} skipped: {}",ToString(type),e.what());complete=false;failed.insert(type);}
    }
    return complete;
}
}
