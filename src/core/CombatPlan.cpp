#include "PluginScan.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <span>

namespace UnifiedMusic {
namespace {
std::uint32_t Word(const std::vector<std::uint8_t>& b,size_t pos) {
    return b[pos] | (std::uint32_t(b[pos+1])<<8) | (std::uint32_t(b[pos+2])<<16) | (std::uint32_t(b[pos+3])<<24);
}
std::optional<FormKey> ResolveTrack(std::uint32_t id,const TrackRecord& track) {
    if(!id || (id>>24)>track.masters.size()) return {};
    auto index=id>>24;
    return FormKey{index==track.masters.size()?track.source:track.masters[index],id&0xffffff};
}
bool Contains(const std::vector<FormKey>& values,const FormKey& key) {
    return std::find(values.begin(),values.end(),key)!=values.end();
}
struct Gate {
    std::vector<Area> areas;
    std::string conditionIdentity;
};
std::optional<Gate> GeographicGate(const TrackRecord& track) {
    if(!track.singleAudio || track.deleted || track.invalid) return {};
    Gate gate;
    for(auto c:track.conditions) {
        // Do not project OR/alias/global/package/swap conditions into a wider scope.
        if(c.size()!=32 || (c[0]&0x1f)!=0 || (c[0]>>5)>5) return {};
        const auto function=Word(c,8);
        // This intentionally small schema lets us compare every form-valued
        // field across master tables. Unknown conditions remain author-owned.
        if(function!=56 && function!=58 && function!=59 && function!=74 &&
           function!=67 && function!=310 && function!=359) return {};
        float comparison;
        std::memcpy(&comparison,c.data()+4,sizeof(comparison));
        if(!std::isfinite(comparison)) return {};
        const auto runOn=Word(c,20);
        if(runOn!=0 && runOn!=2) return {};
        const auto parameter=ResolveTrack(Word(c,12),track);
        if(!parameter || (function!=59 && Word(c,16)!=0)) return {};
        // All supported functions take a form in parameter 1; GetStageDone
        // additionally takes a numeric stage in parameter 2.
        gate.conditionIdentity+=ToString(*parameter)+";";
        std::fill(c.begin()+12,c.begin()+16,0);
        if(runOn==2) {
            auto ref=ResolveTrack(Word(c,24),track);
            if(!ref || *ref!=FormKey{"skyrim.esm",0x14}) return {};
            gate.conditionIdentity+=ToString(*ref)+";";
            std::fill(c.begin()+24,c.begin()+28,0);
        }
        const auto kind=function==67?AreaKind::Cell:function==310?AreaKind::Worldspace:AreaKind::Location;
        const bool geographic=function==67 || function==310 || function==359;
        if(geographic) {
            const auto compare=Word(c,4);
            if(!((c[0]==0 && compare==0x3f800000) || (c[0]==0x20 && compare==0))) return {};
            gate.areas.push_back({kind,*parameter});
        }
        gate.conditionIdentity.append(reinterpret_cast<const char*>(c.data()),c.size());
    }
    // An unconditional addition to MUSCombat already has an explicit role.
    // Conditional additions need geography so quest-only special music stays
    // outside automatic merging.
    if(gate.areas.empty() && !track.conditions.empty()) return {};
    std::sort(gate.areas.begin(),gate.areas.end());
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
    std::set<FormKey> candidates;
    for(const auto& r:*chain) {
        bool changed=r.deleted || r.oneShot || r.settings!=base->settings;
        for(const auto& track:base->tracks) if(!Contains(r.tracks,track)) changed=true;
        for(const auto& prior:*chain) {
            if(std::find(r.dependencies.begin(),r.dependencies.end(),prior.source)==r.dependencies.end()) continue;
            for(const auto& track:prior.tracks) if(!Contains(r.tracks,track)) changed=true;
        }
        if(changed) { plan.diagnostics.push_back("SKIP_COMBAT default battle metadata change or deliberate removal"); return; }
        for(const auto& track:r.tracks) if(!Contains(base->tracks,track)) candidates.insert(track);
    }
    const auto protectedTracks=ProtectedTracks(base->key,plan.scriptBound);
    std::map<std::vector<Area>,std::vector<FormKey>> groups;
    for(const auto& key:candidates) {
        const auto skip=[&](const char* reason) {plan.diagnostics.push_back("SKIP_COMBAT "+ToString(key)+" "+reason);};
        if(protectedTracks.contains(key) || ScriptReferences(key)) { skip("shared, scripted, deleted or invalid track"); continue; }
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) {skip("missing track record");continue;}
        const auto origin=std::find_if(found->second.begin(),found->second.end(),[&](const auto& t){return t.source==key.plugin;});
        if(origin==found->second.end()) {skip("missing original track");continue;}
        const auto gate=GeographicGate(*origin);
        if(!gate) {skip("unsupported track or ambiguous conditional role");continue;}
        bool valid=true;
        for(const auto& t:found->second) {
            const auto other=GeographicGate(t);
            if(!other || other->areas!=gate->areas || other->conditionIdentity!=gate->conditionIdentity) valid=false;
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
