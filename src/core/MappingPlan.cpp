#include "PluginScan.h"
#include <algorithm>
#include <functional>

namespace UnifiedMusic {
void PluginScan::InferMappings(MergePlan& plan) const {
    std::map<std::string,bool> complete;
    std::set<std::string> active;
    std::function<bool(const std::string&)> dependenciesComplete=[&](const std::string& source) {
        if(complete.contains(source)) return complete.at(source);
        if(!active.insert(source).second) return false;
        const auto found=dependencies_.find(source);
        bool valid=found!=dependencies_.end();
        if(valid) for(const auto& master:found->second) if(!dependenciesComplete(master)) valid=false;
        active.erase(source);
        complete[source]=valid;
        return valid;
    };
    for(const auto& [key,records]:mappings_) {
        if(!records.empty() && plan.assignments.contains(records.front().area)) continue;
        if(records.size()<2) continue;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) continue;
        std::set<std::optional<FormKey>> changes;
        bool inheritedOverride=false,valid=true;
        std::set<std::string> sources;
        for(const auto& r:records) {
            if(r.music!=origin->music) changes.insert(r.music);
            else if(r.source!=key.plugin) {
                for(const auto& changed:records) if(changed.music!=origin->music && !DependsOn(changed.source,r.source))
                    inheritedOverride=true;
            }
            if(r.deleted || r.invalid || r.area!=origin->area || !sources.insert(r.source).second ||
               !dependenciesComplete(r.source)) valid=false;
        }
        if(changes.empty() || !inheritedOverride) continue;
        const auto skip=[&](const char* reason){plan.diagnostics.push_back("SKIP_MAPPING "+ToString(key)+" "+reason);};
        if(!valid || changes.size()!=1 || !*changes.begin()) {skip("invalid dependencies, deleted, malformed, cleared or competing assignments");continue;}
        const auto desired=**changes.begin();
        // A dependent patch can knowingly undo the assignment; independent
        // overrides carrying the original field cannot describe that intent.
        for(const auto& r:records) if(r.music==origin->music) {
            for(const auto& prior:records) if(prior.music==desired && DependsOn(r.source,prior.source)) valid=false;
        }
        if(!valid) {skip("dependent patch restores the original assignment");continue;}
        const auto type=music_.find(desired);
        if(type==music_.end() ||
           !std::ranges::any_of(type->second,[&](const auto& r){return r.source==desired.plugin;}) ||
           std::ranges::any_of(type->second,[](const auto& r){return r.deleted;})) {
            skip("missing or deleted assigned music type");continue;
        }
        plan.mappingRepairs.push_back({origin->area,desired,records});
    }
}
}
