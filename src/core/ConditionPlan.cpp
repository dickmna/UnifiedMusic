#include "PluginScan.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

namespace UnifiedMusic {
namespace {
std::uint32_t Word(const std::vector<std::uint8_t>& data,std::size_t offset) {
    return data[offset] | (std::uint32_t(data[offset+1])<<8) |
        (std::uint32_t(data[offset+2])<<16) | (std::uint32_t(data[offset+3])<<24);
}
std::optional<FormKey> Resolve(std::uint32_t id,const TrackRecord& record) {
    const auto index=id>>24;
    if(!id || index>record.masters.size()) return {};
    return FormKey{index==record.masters.size()?record.source:record.masters[index],id&0xffffff};
}
std::optional<std::vector<ConditionTerm>> Decode(const TrackRecord& record) {
    if(!record.singleAudio || record.deleted || record.invalid || record.conditions.size()>256) return {};
    std::vector<ConditionTerm> result;
    for(const auto& raw:record.conditions) {
        // No OR groups, aliases, globals, package data, or target swapping.
        // Padding bytes 1..3 are deliberately not part of semantic identity.
        if(raw.size()!=32 || (raw[0]&0x1f) || (raw[0]>>5)>5 || Word(raw,28)!=0xffffffff) return {};
        ConditionTerm term;
        term.opcode=raw[0]>>5;
        term.function=Word(raw,8);
        if(term.function!=56 && term.function!=58 && term.function!=59 && term.function!=74 &&
           term.function!=67 && term.function!=310 && term.function!=359 && term.function!=707) return {};
        term.comparison=Word(raw,4);
        float value;
        std::memcpy(&value,&term.comparison,sizeof(value));
        if(!std::isfinite(value)) return {};
        auto parameter=Resolve(Word(raw,12),record);
        if(!parameter) return {};
        term.parameter=*parameter;
        term.parameter2=Word(raw,16);
        if(term.function!=59 && term.parameter2) return {};
        const auto runOn=Word(raw,20);
        if(runOn==2) {
            const auto reference=Resolve(Word(raw,24),record);
            if(!reference || *reference!=FormKey{"skyrim.esm",0x14}) return {};
            term.onPlayerReference=true;
        } else if(runOn!=0 || Word(raw,24)!=0) return {};
        result.push_back(term);
    }
    return result;
}
std::optional<Area> Exclusion(const ConditionTerm& term) {
    if(!((term.opcode==0 && term.comparison==0) ||
         (term.opcode==1 && term.comparison==0x3f800000))) return {};
    if(term.function==67) return Area{AreaKind::Cell,term.parameter};
    if(term.function==310) return Area{AreaKind::Worldspace,term.parameter};
    if(term.function==359) return Area{AreaKind::Location,term.parameter};
    return {};
}
}

void PluginScan::InferConditionRepairs(MergePlan& plan) const {
    if(!plan.battleMergeSafe || !plan.battleType) return;
    const auto& music=music_.at(*plan.battleType);
    const auto baseMusic=std::find_if(music.begin(),music.end(),[&](const auto& r){return r.source==plan.battleType->plugin;});
    if(baseMusic==music.end()) return;
    const auto protectedTracks=ProtectedTracks(*plan.battleType,plan.scriptBound);

    // Resolve transitive dependencies even when an intermediate plugin does not
    // override this track. A dependent patch's deliberate removal wins.
    const auto ancestors=[&](const std::string& source)->std::optional<std::set<std::string>> {
        std::set<std::string> result,active;
        std::function<bool(const std::string&)> visit=[&](const std::string& plugin) {
            if(active.contains(plugin)) return false;
            if(result.contains(plugin)) return true;
            auto found=dependencies_.find(plugin);
            if(found==dependencies_.end()) return false;
            active.insert(plugin);
            for(const auto& master:found->second) if(!visit(master)) return false;
            active.erase(plugin); result.insert(plugin); return true;
        };
        if(!visit(source)) return {};
        result.erase(source);
        return result;
    };

    const std::set<FormKey> originalTracks(baseMusic->tracks.begin(),baseMusic->tracks.end());
    for(const auto& key:originalTracks) {
        if(key.plugin!="skyrim.esm") continue;
        const auto found=tracks_.find(key);
        if(found==tracks_.end() || found->second.size()<2) continue;
        const auto skip=[&](const char* reason){plan.diagnostics.push_back("SKIP_CONDITIONS "+ToString(key)+" "+reason);};
        if(protectedTracks.contains(key) || ScriptReferences(key)) {skip("shared or protected track");continue;}
        const auto& records=found->second;
        auto origin=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) {skip("missing original track");continue;}
        const auto baseline=Decode(*origin);
        if(!baseline) {skip("unsupported original conditions");continue;}
        ConditionRepair repair;
        repair.track=key;
        std::map<std::string,std::set<Area>> contributions;
        std::map<std::string,std::set<std::string>> parents;
        std::set<Area> combined;
        bool valid=true;
        for(const auto& record:records) {
            const auto conditions=Decode(record);
            const auto dependencies=ancestors(record.source);
            if(!conditions || !dependencies || contributions.contains(record.source) ||
               record.metadata!=origin->metadata || conditions->size()<baseline->size() ||
               !std::equal(baseline->begin(),baseline->end(),conditions->begin())) {valid=false;break;}
            auto& extras=contributions[record.source];
            for(std::size_t i=baseline->size();i<conditions->size();++i) {
                const auto area=Exclusion((*conditions)[i]);
                if(!area) {valid=false;break;}
                const auto definition=areaForms_.find(area->form);
                if(definition==areaForms_.end() || definition->second!=area->kind || deletedAreas_.contains(area->form)) {valid=false;break;}
                extras.insert(*area);
            }
            if(!valid) break;
            combined.insert(extras.begin(),extras.end());
            parents.emplace(record.source,*dependencies);
            repair.versions.push_back({record.source,*conditions});
        }
        if(!valid) {skip("not a pure supported exclusion extension");continue;}
        for(const auto& [source,extras]:contributions) {
            for(const auto& [prior,priorExtras]:contributions) {
                if(parents.at(source).contains(prior) &&
                   !std::includes(extras.begin(),extras.end(),priorExtras.begin(),priorExtras.end())) valid=false;
            }
        }
        if(!valid) {skip("dependent patch removes an earlier exclusion");continue;}
        if(combined.empty()) continue;
        // With a single effective extension there is nothing to reconcile.
        const bool conflict=std::ranges::any_of(contributions,[&](const auto& entry){return entry.first!=key.plugin && entry.second!=combined;});
        if(!conflict) continue;
        repair.exclusions.assign(combined.begin(),combined.end());
        std::set<FormKey> required(baseMusic->tracks.begin(),baseMusic->tracks.end());
        for(const auto& [source,extras]:contributions) if(!extras.empty()) {
            for(const auto& type:music) if(type.source==source || parents.at(source).contains(type.source)) {
                required.insert(type.tracks.begin(),type.tracks.end());
            }
        }
        repair.requiredBattleTracks.assign(required.begin(),required.end());
        plan.conditionRepairs.push_back(std::move(repair));
    }
}
}
