#include "PluginScan.h"
#include <algorithm>
#include <cstring>

namespace UnifiedMusic {
void PluginScan::PlanAudio(MergePlan& plan) const {
    auto types=plan.allowed;
    if(plan.battleMergeSafe && plan.battleType) types.insert(*plan.battleType);
    for(const auto& type:types) {
        const auto& versions=music_.at(type);
        const auto origin=std::ranges::find_if(versions,[&](const auto& r){return r.source==type.plugin;});
        const bool defaultBattle=plan.battleMergeSafe && plan.battleType && type==*plan.battleType;
        if(origin==versions.end() || (!defaultBattle && plan.scriptBound.contains(type))) continue;
        const auto protectedTracks=ProtectedTracks(type,plan);
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
                if(!base->conditions.empty()) {
                    if(base->conditions.size()!=1) continue;
                    const auto& c=base->conditions.front();
                    if(c.size()!=32) continue;
                    std::uint32_t words[7]; std::memcpy(words,c.data()+4,28);
                    if(!((c[0]==0 && words[0]==0) || (c[0]==0x20 && words[0]==0x3f800000)) ||
                       words[1]!=707 || words[2]!=0x35d59 || words[3]!=0 ||
                       !((words[4]==0 && words[5]==0) || (words[4]==2 && words[5]==0x14)) || words[6]!=0xffffffff) continue;
                }
            } else if(protectedTracks.contains(key)) continue;
            plan.audioCandidates.push_back({type,key,base->audioPath,base->finalePath});
        }
    }
}
}
