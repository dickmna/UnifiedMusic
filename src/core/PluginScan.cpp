#include "PluginScan.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace UnifiedMusic {
namespace {
std::string Lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
bool IsBase(const std::string& name) {
    static const std::set<std::string> names{"skyrim.esm","update.esm","dawnguard.esm","hearthfires.esm","dragonborn.esm"};
    return names.contains(name);
}
bool Contains(const std::vector<FormKey>& v,const FormKey& key) { return std::find(v.begin(),v.end(),key)!=v.end(); }
}

bool PluginScan::Ambient(const MusicRecord& r) const {
    if(!IsBase(r.key.plugin) || r.oneShot || r.source!=r.key.plugin) return false;
    auto name=Lower(r.editorID);
    for (auto term : {"combat","battle","boss","silent","silence","blank","reveal","discov","stinger","finale","event","reward"})
        if(name.find(term)!=std::string::npos) return false;
    if(name.find("test")!=std::string::npos && !std::ranges::any_of(mappings_,[&](const auto& entry) {
        return std::ranges::any_of(entry.second,[&](const auto& m) {
            return IsBase(m.source) && m.source==m.area.form.plugin && !m.deleted && !m.invalid && m.music==r.key;
        });
    })) return false;
    for (auto term : {"explore","town","dungeon","tavern"})
        if(name.find(term)!=std::string::npos) return true;
    return false;
}

std::string ToString(const FormKey& key) { return key.plugin + ":" + std::to_string(key.id); }

bool PluginScan::ScriptReferences(const FormKey& key,bool modsOnly) const {
    for(const auto& script:scripts_) {
        if(modsOnly && IsBase(script.source)) continue;
        if(script.parsed) {if(script.references.contains(key)) return true;continue;}
        auto it=std::find(script.masters.begin(),script.masters.end(),key.plugin);
        size_t index=it-script.masters.begin();
        if(it==script.masters.end() && script.source!=key.plugin) continue;
        auto raw=key.id | (static_cast<std::uint32_t>(index)<<24);
        std::array<std::uint8_t,4> needle{std::uint8_t(raw),std::uint8_t(raw>>8),std::uint8_t(raw>>16),std::uint8_t(raw>>24)};
        if(std::search(script.bytes.begin(),script.bytes.end(),needle.begin(),needle.end())!=script.bytes.end()) return true;
    }
    return false;
}

bool PluginScan::DependsOn(const std::string& source,const std::string& dependency) const {
    std::set<std::string> visited{source};
    auto found=dependencies_.find(source);
    if(found==dependencies_.end()) return false;
    auto pending=found->second;
    while(!pending.empty()) {
        const auto name=pending.back(); pending.pop_back();
        if(name==dependency) return true;
        if(!visited.insert(name).second) continue;
        found=dependencies_.find(name);
        if(found!=dependencies_.end()) pending.insert(pending.end(),found->second.begin(),found->second.end());
    }
    return false;
}

std::set<FormKey> PluginScan::ProtectedTracks(const FormKey& excludedType,const MergePlan& plan) const {
    auto protectedTracks=nestedTracks_;
    const auto target=music_.find(excludedType);
    const bool ambientTarget=target!=music_.end() && std::ranges::any_of(target->second,[&](const auto& r){return Ambient(r);});
    for(const auto& [key,records]:music_) {
        if(key==excludedType) continue;
        const auto origin=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.source==key.plugin;});
        const bool ordinaryAmbient=origin!=records.end() && Ambient(*origin);
        // Ordinary ambient lists can share tracks across biomes. A map binding
        // alone is not exclusive ownership, but must not leak into combat.
        for(const auto& r:records) if(!ordinaryAmbient || plan.scriptBound.contains(key) || (!ambientTarget && mappedTypes_.contains(key)) || r.oneShot) {
            protectedTracks.insert(r.tracks.begin(),r.tracks.end());
        }
    }
    // Configuration-only Boss/custom owners count just as plugin TNAM owners.
    // For inferred condition edits, even sharing between ambient lists is
    // protected: a restriction for one biome must not change another list.
    for(const auto& [key,tracks]:plan.distributions) if(key!=excludedType)
        protectedTracks.insert(tracks.begin(),tracks.end());
    for(const auto& [key,records]:tracks_) {
        if(std::ranges::any_of(records,[](const auto& r){return r.deleted || r.invalid;})) protectedTracks.insert(key);
    }
    return protectedTracks;
}

std::vector<const TrackRecord*> PluginScan::EffectiveTracks(const FormKey& key) const {
    const auto found=tracks_.find(key);
    if(found==tracks_.end() || !std::ranges::any_of(found->second,[&](const auto& r){return r.source==key.plugin;}) ||
       std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;})) return {};
    std::vector<const TrackRecord*> result;
    for(const auto& record:found->second)
        if(!std::ranges::any_of(found->second,[&](const auto& later){return DependsOn(later.source,record.source);})) result.push_back(&record);
    std::ranges::sort(result,{},[](const auto* r){return r->source;});
    return result;
}

bool PluginScan::ValidParameter(const ConditionTerm& term) const {
    if(term.function==18 || term.function==520 ||
       (term.function==707 && term.parameter==FormKey{"skyrim.esm",0x35d59})) return true;
    const auto expected=term.function==67?"CELL":term.function==310?"WRLD":term.function==359?"LCTN":
        term.function==74?"GLOB":term.function==117?"REGN":(term.function==562 || term.function==707)?"KYWD":"QUST";
    bool exists=false;
    for(const auto& d:definitions_) if(d.key==term.parameter) {
        if(d.deleted || d.type!=expected) return false;
        if(d.source==d.key.plugin) exists=true;
    }
    return exists;
}

bool PluginScan::Within(const Area& child,const Area& parent) const {
    if(child.kind!=parent.kind) return false;
    auto current=child.form;
    std::set<FormKey> seen;
    for(unsigned depth=0;depth<64;++depth) {
        if(!seen.insert(current).second || deletedAreas_.contains(current)) return false;
        if(current==parent.form) return true;
        if(child.kind!=AreaKind::Location) return false;
        const auto it=locationParents_.find(current);
        if(it==locationParents_.end() || it->second.size()!=1 || !*it->second.begin()) return false;
        current=**it->second.begin();
    }
    return false;
}

MergePlan PluginScan::Plan() const {
    MergePlan plan;
    if(!PlanDistribution(plan) || !PlanSelectors(plan)) return plan;
    const auto unknown=std::ranges::count_if(scripts_,[](const auto& s){return !s.parsed;});
    if(unknown) plan.diagnostics.push_back("VMAD_FALLBACK "+std::to_string(unknown)+" unsupported property blocks retain conservative reference protection");
    // Complete this set before checking tracks shared with any other type.
    for(const auto& [key,chain]:music_) if(ScriptReferences(key)) plan.scriptBound.insert(key);
    for(const auto& rule:plan.selectors) plan.scriptBound.insert(rule.music);
    for(const auto& [key,chain]:music_) {
        const auto origin=std::find_if(chain.begin(),chain.end(),[&](const auto& r){return r.source==key.plugin;});
        if(origin==chain.end() || !Ambient(*origin)) continue;
        const auto& base=*origin;
        // Known public ambient lists retain their public role when referenced.
        bool protectedType=false;
        for(size_t i=0;i<chain.size();++i) {
            const auto& r=chain[i];
            if(r.deleted || r.settings!=base.settings) protectedType=true;
            for(const auto& track:base.tracks) if(!Contains(r.tracks,track)) protectedType=true;
            for(size_t j=0;j<chain.size();++j) {
                if(i==j) continue;
                const auto& prior=chain[j];
                if(!DependsOn(r.source,prior.source)) continue;
                for(const auto& track:prior.tracks) if(!Contains(r.tracks,track)) protectedType=true;
            }
        }
        if(protectedType) {
            plan.diagnostics.push_back("PRESERVE "+ToString(key)+" "+base.editorID+" (metadata change, deletion or track removal)");
            continue;
        }
        plan.allowed.insert(key);
        auto& additions=plan.additions[key];
        for(const auto& r:chain) {
            if(IsBase(r.source)) continue;
            for(const auto& track:r.tracks) {
                if(Contains(base.tracks,track) || Contains(additions,track)) continue;
                const auto found=tracks_.find(track);
                const bool invalid=found!=tracks_.end() && std::ranges::any_of(found->second,[](const auto& r){return r.deleted || r.invalid;});
                if(invalid || ScriptReferences(track)) {
                    plan.diagnostics.push_back("SKIP_TRACK "+ToString(track)+" -> "+ToString(key)+" (scripted, deleted or invalid track)");
                    continue;
                }
                additions.push_back(track);
            }
        }
    }
    InferCombat(plan);
    // A clear is an explicit complete playlist, not a missing contribution.
    // Resolve ordinary/Boss links first so clearing one owner cannot suppress
    // independent recovery in another; then exclude this owner from inference.
    for(const auto& type:plan.clearedTypes) {
        plan.additions.erase(type);
        plan.allowed.erase(type);
        if(plan.battleType && type==*plan.battleType) {
            plan.combatRules.clear();plan.battleMergeSafe=false;
        }
    }
    PlanAudio(plan);
    InferConditionRepairs(plan);
    InferMappings(plan);
    return plan;
}
}
