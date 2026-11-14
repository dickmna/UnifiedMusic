#include "PluginScan.h"
#include <algorithm>
#include "Conditions.h"

namespace UnifiedMusic {
namespace {
bool Contains(const std::vector<FormKey>& values,const FormKey& key) {
    return std::ranges::find(values,key)!=values.end();
}
struct Gate {
    std::vector<Area> areas;
    std::vector<ConditionTerm> conditions;
};
std::optional<Gate> GeographicGate(const TrackRecord& track) {
    const auto terms=DecodeConditions(track);
    if(!terms) return {};
    Gate gate;gate.conditions=*terms;
    if(OrdinaryConditions(*terms,true)) return gate;
    for(const auto& term:*terms) {
        if(term.isOR) return {};
        if(term.function==707 && !OrdinaryConditions(std::span(&term,1),true)) return {};
        if(term.function==67 || term.function==310 || term.function==359) {
            const auto area=ConditionArea(term,true);if(!area) return {};
            gate.areas.push_back(*area);
        }
    }
    // Authored geography supplies the role; quest-only special music does not.
    if(gate.areas.empty()) return {};
    std::ranges::sort(gate.areas);
    gate.areas.erase(std::unique(gate.areas.begin(),gate.areas.end()),gate.areas.end());
    return gate;
}
}

void PluginScan::InferCombat(MergePlan& plan) const {
    const std::vector<MusicRecord>* chain=nullptr;
    const MusicRecord* base=nullptr;
    for(const auto& [key,records]:music_) {
        for(const auto& r:records) if(r.source=="skyrim.esm" && r.key.plugin==r.source && r.editorID=="MUSCombat") {
            if(base) { plan.diagnostics.push_back("SKIP_COMBAT ambiguous default battle definition"); return; }
            chain=&records; base=&r;
        }
    }
    if(!base) return;
    plan.battleType=base->key;
    // Public MUSCombat remains public when a mod starts/stops it. Keep its
    // identity and script calls; dedicated/custom types are never inferred here.
    std::set<FormKey> candidates;
    for(const auto& r:*chain) {
        // The engine keeps the winning priority/fade settings. Membership is
        // independent of those fields, but explicit removals still take priority.
        bool changed=r.deleted || r.oneShot;
        for(const auto& track:base->tracks) if(!Contains(r.tracks,track)) changed=true;
        for(const auto& prior:*chain) {
            if(!DependsOn(r.source,prior.source)) continue;
            for(const auto& track:prior.tracks) if(!Contains(r.tracks,track)) changed=true;
        }
        if(changed) { plan.diagnostics.push_back("SKIP_COMBAT default battle deletion, one-shot change or deliberate removal"); return; }
        for(const auto& track:r.tracks) if(!Contains(base->tracks,track)) candidates.insert(track);
    }
    plan.battleMergeSafe=true;
    std::map<std::vector<Area>,std::vector<FormKey>> groups;
    std::set<FormKey> restored;
    for(const auto& key:candidates) {
        const auto skip=[&](const char* reason) {plan.diagnostics.push_back("SKIP_COMBAT "+ToString(key)+" "+reason);};
        if(ScriptReferences(key)) { skip("direct script track reference"); continue; }
        const auto versions=EffectiveTracks(key);
        if(versions.empty()) {skip("missing/deleted/invalid track");continue;}
        const auto* effective=versions.front();
        const auto terms=DecodeConditions(*effective,true);
        bool valid=terms && std::ranges::all_of(*terms,[&](const auto& t){return ValidParameter(t);});
        for(const auto* t:versions)
            if(t->metadata!=effective->metadata || DecodeConditions(*t,true)!=terms) valid=false;
        if(!valid) {skip("unreadable or competing effective track definitions");continue;}
        const auto gate=GeographicGate(*effective);
        if(gate) groups[gate->areas].push_back(key);
        else {
            // Restore the author's exact membership, retaining the native
            // OR/role/quest expression. No selector or inferred area is created.
            plan.additions[base->key].push_back(key);
            plan.diagnostics.push_back("RESTORE_AUTHORED "+ToString(base->key)+" <- "+ToString(key));
        }
        restored.insert(key);
    }
    for(auto& [areas,tracks]:groups) plan.combatRules.push_back({areas,std::move(tracks)});
    // A battle contribution can also have an explicitly authored Boss/Civil War
    // membership. Restore only those existing links, never copy the whole pool.
    for(const auto& [owner,records]:music_) {
        if(owner==base->key || owner.plugin!=base->key.plugin) continue;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==owner.plugin;});
        if(origin==records.end() || std::ranges::any_of(records,[&](const auto& r) {
            return r.deleted || r.oneShot || std::ranges::any_of(origin->tracks,[&](const auto& t){return !Contains(r.tracks,t);});
        })) continue;
        for(const auto& track:restored) {
            if(Contains(origin->tracks,track) || !std::ranges::any_of(records,[&](const auto& r){return Contains(r.tracks,track);})) continue;
            const bool removed=std::ranges::any_of(records,[&](const auto& later) {
                return !Contains(later.tracks,track) && std::ranges::any_of(records,[&](const auto& prior) {
                    return DependsOn(later.source,prior.source) && Contains(prior.tracks,track);
                });
            });
            if(!removed) plan.additions[owner].push_back(track);
        }
    }
}
}
