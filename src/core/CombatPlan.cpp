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
        bool changed=r.deleted || r.oneShot || r.settings!=base->settings;
        for(const auto& track:base->tracks) if(!Contains(r.tracks,track)) changed=true;
        for(const auto& prior:*chain) {
            if(!DependsOn(r.source,prior.source)) continue;
            for(const auto& track:prior.tracks) if(!Contains(r.tracks,track)) changed=true;
        }
        if(changed) { plan.diagnostics.push_back("SKIP_COMBAT default battle metadata change or deliberate removal"); return; }
        for(const auto& track:r.tracks) if(!Contains(base->tracks,track)) candidates.insert(track);
    }
    plan.battleMergeSafe=true;
    std::map<std::vector<Area>,std::vector<FormKey>> groups;
    for(const auto& key:candidates) {
        const auto skip=[&](const char* reason) {plan.diagnostics.push_back("SKIP_COMBAT "+ToString(key)+" "+reason);};
        if(ScriptReferences(key)) { skip("direct script track reference"); continue; }
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) {skip("missing track record");continue;}
        const auto origin=std::find_if(found->second.begin(),found->second.end(),[&](const auto& t){return t.source==key.plugin;});
        if(origin==found->second.end()) {skip("missing original track");continue;}
        const auto gate=GeographicGate(*origin);
        if(!gate) {skip("unsupported track or ambiguous conditional role");continue;}
        bool valid=true;
        for(const auto& t:found->second) {
            const auto other=GeographicGate(t);
            if(!other || other->areas!=gate->areas || other->conditions!=gate->conditions) valid=false;
        }
        for(const auto& area:gate->areas) {
            auto form=areaForms_.find(area.form);
            if(form==areaForms_.end() || form->second!=area.kind || deletedAreas_.contains(area.form)) valid=false;
        }
        if(!valid) {skip("changed conditions or unresolved geography");continue;}
        groups[gate->areas].push_back(key);
    }
    for(auto& [areas,tracks]:groups) plan.combatRules.push_back({areas,std::move(tracks)});
}
}
