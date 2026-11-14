#include "PluginScan.h"
#include <algorithm>
#include "Conditions.h"

namespace UnifiedMusic {
void PluginScan::PlanAudio(MergePlan& plan) const {
    auto types=plan.allowed;
    if(plan.battleMergeSafe && plan.battleType) types.insert(*plan.battleType);
    for(const auto& type:types) {
        const auto& versions=music_.at(type);
        const auto origin=std::ranges::find_if(versions,[&](const auto& r){return r.source==type.plugin;});
        if(origin==versions.end()) continue;
        std::set<FormKey> seen;
        for(const auto& key:origin->tracks) {
            if(!seen.insert(key).second || ScriptReferences(key)) continue;
            // Copies are added only to proven ordinary lists, never to their
            // dedicated owners. Palette children remain in the original tree.
            if(nestedTracks_.contains(key)) continue;
            const auto found=tracks_.find(key);
            if(found==tracks_.end()) continue;
            const auto base=std::ranges::find_if(found->second,[&](const auto& r){return r.source==key.plugin;});
            if(base==found->second.end() || !base->singleAudio || base->deleted || base->invalid) continue;
            if(std::ranges::any_of(found->second,[&](const auto& r){
                return r.deleted || r.invalid || !r.singleAudio || r.metadata!=base->metadata;
            })) continue;
            if(plan.battleType && type==*plan.battleType) {
                // Keep dragon/Boss cues exclusive. Recognize only the exact
                // ordinary baseline, not strings such as "combat" in a filename.
                const auto conditions=DecodeConditions(*base);
                if(!conditions || !OrdinaryConditions(*conditions,true)) continue;
            }
            plan.audioCandidates.push_back({type,key,base->audioPath,base->finalePath});
        }
    }
}
}
