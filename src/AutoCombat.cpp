#include "AutoCombat.h"
#include "core/PluginScan.h"
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
    case 707: return RE::FormType::Keyword;
    default: return {};
    }
}

std::uint32_t AreaFunction(AreaKind kind)
{
    switch (kind) {
    case AreaKind::Cell: return 67;
    case AreaKind::Worldspace: return 310;
    case AreaKind::Location: return 359;
    default: return 0;
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
    if (!type || (expected.function!=59 && expected.parameter2)) return false;
    auto* parameter=data->LookupForm<RE::TESForm>(expected.parameter.id,expected.parameter.plugin);
    if (!parameter || parameter->GetFormType()!=*type) return false;
    if (actual.flags.isOR || actual.flags.usesAliases || actual.flags.global ||
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

bool Matches(const RE::TESCondition& conditions,const ConditionVersion& expected,RE::TESDataHandler* data,Node*& tail)
{
    // Exact length bounds traversal and also rejects cycles without evaluating
    // conditions or interpreting unrelated native memory as serialized CTDA.
    if (expected.conditions.size()>4096) return false;
    auto* node=conditions.head;
    tail=nullptr;
    for (const auto& term:expected.conditions) {
        if (!Matches(node,term,data)) return false;
        tail=node;
        node=node->next;
    }
    return node==nullptr;
}

bool Excludes(const ConditionVersion& version,const Area& area)
{
    return std::ranges::any_of(version.conditions,[&](const auto& condition) {
        return condition.function==AreaFunction(area.kind) && condition.parameter==area.form &&
            ((condition.opcode==0 && condition.comparison==0) ||
             (condition.opcode==1 && condition.comparison==0x3f800000));
    });
}

struct PreparedRepair {
    RE::BGSMusicTrack* track;
    Node* head;
    Node* tail;
    const ConditionVersion* version;
    std::vector<std::unique_ptr<Node>> nodes;
};
}

bool ConfigureAutoCombat(const MergePlan& plan)
{
    if (plan.combatRules.empty() && plan.conditionRepairs.empty()) return true;
    const auto fail=[](std::string_view reason) {
        REX::WARN("Auto combat batch skipped: {}",reason);
        return false;
    };
    auto* data=RE::TESDataHandler::GetSingleton();
    auto* objects=RE::BGSDefaultObjectManager::GetSingleton();
    if (!data || !objects || !plan.battleType || !plan.battleMergeSafe) return fail("unavailable or protected default battle list");
    auto* target=data->LookupForm<RE::BGSMusicType>(plan.battleType->id,plan.battleType->plugin);
    auto* native=objects->GetObject<RE::BGSMusicType>(RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic);
    if (!target || target!=native) return fail("engine default differs from the original battle type");

    // Prepare all list additions and condition nodes before either is applied.
    std::vector<Track*> pending;
    for (const auto& rule:plan.combatRules) {
        for (const auto& key:rule.tracks) {
            auto* track=data->LookupForm<Track>(key.id,key.plugin);
            if (!track || !track->track || track->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack)
                return fail("unresolved/non-single restored track");
            if (!Contains(target,track) && std::ranges::find(pending,track)==pending.end()) pending.push_back(track);
        }
    }

    std::vector<PreparedRepair> repairs;
    std::size_t conditionCount=0;
    for (const auto& repair:plan.conditionRepairs) {
        auto* wrapper=data->LookupForm<Track>(repair.track.id,repair.track.plugin);
        if (!wrapper || !wrapper->track || wrapper->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack || !Contains(target,wrapper))
            return fail("condition target is not a current single battle track");
        for (const auto& required:repair.requiredBattleTracks) {
            auto* track=data->LookupForm<Track>(required.id,required.plugin);
            if (!track || !track->track || (!Contains(target,track) && std::ranges::find(pending,track)==pending.end()))
                return fail("replacement contribution missing; preserving conditions to avoid silence");
        }
        auto* source=wrapper->GetFile();
        if (!source) return fail("condition target has no source file");
        const auto sourceName=Lower(source->GetFilename());
        const ConditionVersion* version=nullptr;
        for (const auto& candidate:repair.versions) if (candidate.source==sourceName) {
            if (version) return fail("ambiguous source snapshot");
            version=&candidate;
        }
        if (!version) return fail("unrecognized winning condition source");
        PreparedRepair prepared{wrapper->track,wrapper->track->conditions.head,nullptr,version,{}};
        if (!Matches(wrapper->track->conditions,*version,data,prepared.tail))
            return fail("runtime conditions differ from the scanned winning record");
        if (std::ranges::any_of(repairs,[&](const auto& prior) {
            return prior.track==prepared.track || (prepared.tail && prior.tail==prepared.tail);
        })) return fail("condition chains have shared ownership");

        std::vector<Area> missing;
        for (const auto& area:repair.exclusions) {
            if (Excludes(*version,area) || std::ranges::find(missing,area)!=missing.end()) continue;
            const auto function=AreaFunction(area.kind);
            const auto type=ParameterType(function);
            auto* form=data->LookupForm<RE::TESForm>(area.form.id,area.form.plugin);
            if (!type || !form || form->GetFormType()!=*type) return fail("unresolved exclusion area");
            auto* memory=RE::malloc(sizeof(Node));
            if (!memory) return fail("condition allocation failed");
            std::unique_ptr<Node> node(std::construct_at(static_cast<Node*>(memory)));
            node->data.functionData.function=static_cast<RE::FUNCTION_DATA::FunctionID>(function);
            node->data.functionData.params[0]=form;
            // Constructor supplies Self, == 0, AND and all other flags clear.
            prepared.nodes.push_back(std::move(node));
            missing.push_back(area);
        }
        conditionCount+=prepared.nodes.size();
        repairs.push_back(std::move(prepared));
    }

    if (pending.size()>std::numeric_limits<std::uint32_t>::max()-target->tracks.size()) return fail("battle list capacity exceeded");
    target->tracks.reserve(static_cast<std::uint32_t>(target->tracks.size()+pending.size()));
    // No logical list mutation precedes this point. Validate once more before
    // the non-allocating commit; never replace or delete existing condition nodes.
    for (const auto& repair:repairs) {
        Node* tail=nullptr;
        if (repair.track->conditions.head!=repair.head || !Matches(repair.track->conditions,*repair.version,data,tail) || tail!=repair.tail)
            return fail("condition chain changed during preparation");
    }
    for (auto* track:pending) target->tracks.push_back(track);
    std::size_t repairedTracks=0;
    for (auto& repair:repairs) {
        if (repair.nodes.empty()) continue;
        for (std::size_t i=1;i<repair.nodes.size();++i) repair.nodes[i-1]->next=repair.nodes[i].get();
        auto* first=repair.nodes.front().get();
        for (auto& node:repair.nodes) node.release();
        if (repair.tail) repair.tail->next=first;
        else repair.track->conditions.head=first;
        ++repairedTracks;
    }
    REX::INFO("Auto combat: {} restored tracks, {} repaired condition lists, {} restored exclusions",pending.size(),repairedTracks,conditionCount);
    return true;
}
}
