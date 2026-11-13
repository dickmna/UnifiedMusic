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
enum class Role { Ordinary, Dragon, Unknown };
Role BaselineRole(const std::vector<ConditionTerm>& terms) {
    if(terms.empty()) return Role::Ordinary;
    if(terms.size()!=1) return Role::Unknown;
    const auto& term=terms.front();
    if(term.function!=707 || term.parameter!=FormKey{"skyrim.esm",0x35d59}) return Role::Unknown;
    if((term.opcode==0 && term.comparison==0x3f800000) || (term.opcode==1 && term.comparison==0)) return Role::Dragon;
    if((term.opcode==0 && term.comparison==0) || (term.opcode==1 && term.comparison==0x3f800000)) return Role::Ordinary;
    return Role::Unknown;
}
}

void PluginScan::InferConditionRepairs(MergePlan& plan) const {
    if(plan.battleMergeSafe && plan.battleType) InferConditionsFor(plan,*plan.battleType,true);
    for(const auto& type:plan.allowed) InferConditionsFor(plan,type,false);
}

void PluginScan::InferConditionsFor(MergePlan& plan,const FormKey& type,bool battle) const {
    const auto& music=music_.at(type);
    const auto baseMusic=std::find_if(music.begin(),music.end(),[&](const auto& r){return r.source==type.plugin;});
    if(baseMusic==music.end()) return;
    const auto protectedTracks=ProtectedTracks(type,plan);

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
    std::map<FormKey,ConditionRepair> policies;
    for(const auto& key:originalTracks) {
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) continue;
        const auto skip=[&](const char* reason){plan.diagnostics.push_back("SKIP_CONDITIONS "+ToString(key)+" "+reason);};
        const bool protectedTrack=!battle || protectedTracks.contains(key) || ScriptReferences(key);
        if(protectedTrack) skip("shared or protected track; policy evidence only");
        const auto& records=found->second;
        auto origin=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) {skip("missing original track");continue;}
        const auto baseline=Decode(*origin);
        if(!baseline) {skip("unsupported original conditions");continue;}
        ConditionRepair repair;
        repair.track=key;
        repair.musicType=type;
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
        if(protectedTrack) {
            // Reading author policy does not grant ownership of a shared track.
            // Since we cannot repair it, keep only exclusions present in every
            // dependency-maximal version that could win the native load order.
            std::optional<std::set<Area>> guaranteed;
            for(const auto& [source,extras]:contributions) {
                const bool superseded=std::ranges::any_of(parents,[&](const auto& entry){return entry.second.contains(source);});
                if(superseded) continue;
                if(!guaranteed) guaranteed=extras;
                else std::erase_if(*guaranteed,[&](const auto& area){return !extras.contains(area);});
            }
            combined=guaranteed.value_or(std::set<Area>{});
        }
        repair.exclusions.assign(combined.begin(),combined.end());
        std::set<FormKey> required(baseMusic->tracks.begin(),baseMusic->tracks.end());
        for(const auto& [source,extras]:contributions) if(!extras.empty()) {
            for(const auto& type:music) if(type.source==source || parents.at(source).contains(type.source)) {
                required.insert(type.tracks.begin(),type.tracks.end());
            }
        }
        repair.requiredTracks.assign(required.begin(),required.end());
        policies.emplace(key,repair);
        const bool conflict=std::ranges::any_of(contributions,[&](const auto& entry){return entry.first!=key.plugin && entry.second!=combined;});
        if(conflict && !protectedTrack) plan.conditionRepairs.push_back(std::move(repair));
    }

    // Only extend ordinary roles, isolating shared instances. A policy requires
    // unanimity among the original ordinary tracks and a pure positive-area
    // replacement contributed to the same list.
    // Dragon-only tracks are separate; all other original predicates are opaque.
    std::optional<std::set<Area>> common;
    std::vector<ConditionWitness> witnesses;
    for(const auto& key:originalTracks) {
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) return;
        const auto origin=std::ranges::find_if(found->second,[&](const auto& r){return r.source==key.plugin;});
        if(origin==found->second.end()) return;
        const auto baseline=Decode(*origin);
        if(!baseline) return;
        if(!battle && !baseline->empty()) return;
        const auto role=BaselineRole(*baseline);
        if(role==Role::Dragon) continue;
        if(role!=Role::Ordinary) return;
        const auto policy=policies.find(key);
        if(policy==policies.end()) return;
        // Do not equate Self and an explicit reference when inferring a new rule.
        for(const auto& version:policy->second.versions) for(const auto& term:version.conditions)
            if(Exclusion(term) && term.onPlayerReference) return;
        const std::set<Area> areas(policy->second.exclusions.begin(),policy->second.exclusions.end());
        if(!common) common=areas;
        else std::erase_if(*common,[&](const auto& area){return !areas.contains(area);});
        witnesses.push_back({key,policy->second.versions});
    }
    if(!common || common->empty()) return;
    std::set<FormKey> available,candidates;
    for(const auto& record:music) available.insert(record.tracks.begin(),record.tracks.end());
    if(const auto configured=plan.distributions.find(type);configured!=plan.distributions.end()) {
        available.insert(configured->second.begin(),configured->second.end());
        candidates.insert(configured->second.begin(),configured->second.end());
    }
    if(const auto restored=plan.additions.find(type);restored!=plan.additions.end())
        candidates.insert(restored->second.begin(),restored->second.end());
    if(battle) for(const auto& rule:plan.combatRules) {
        available.insert(rule.tracks.begin(),rule.tracks.end());
        if(rule.areas.empty()) candidates.insert(rule.tracks.begin(),rule.tracks.end());
    }
    const auto protectedCandidate=[&](const FormKey& key) {
        if(protectedTracks.contains(key) || ScriptReferences(key)) return true;
        for(const auto& [owner,records]:music_) if(owner!=type)
            for(const auto& record:records) if(std::ranges::find(record.tracks,key)!=record.tracks.end()) return true;
        return false;
    };
    std::map<Area,ConditionWitness> replacements;
    for(const auto& key:available) {
        if(originalTracks.contains(key) || protectedCandidate(key)) continue;
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) continue;
        const auto& records=found->second;
        const auto terms=Decode(records.front());
        if(!terms || terms->size()!=1 || terms->front().onPlayerReference) continue;
        auto negative=terms->front();
        if(!((negative.opcode==0 && negative.comparison==0x3f800000) || (negative.opcode==1 && negative.comparison==0))) continue;
        negative.opcode=0; negative.comparison=0;
        const auto area=Exclusion(negative);
        if(!area || !common->contains(*area)) continue;
        ConditionWitness witness{key,{}};
        bool valid=std::ranges::any_of(records,[&](const auto& r){return r.source==key.plugin;});
        for(const auto& record:records) {
            const auto other=Decode(record);
            if(record.metadata!=records.front().metadata || !other || *other!=*terms) {valid=false;break;}
            witness.versions.push_back({record.source,*other});
        }
        if(valid) replacements.emplace(*area,std::move(witness));
    }
    if(replacements.empty()) return;
    bool guarded=false;
    for(const auto& key:candidates) {
        if(originalTracks.contains(key)) continue;
        if(ScriptReferences(key)) {plan.diagnostics.push_back("SKIP_GUARD "+ToString(key)+" direct script reference");continue;}
        const auto& records=tracks_.at(key);
        ConditionRepair guard;
        guard.track=key; guard.musicType=type; guard.genericGuard=true;
        // A shared single may be guarded in this proven ordinary role without
        // changing the object retained by Boss, Civil War or palette owners.
        guard.isolate=protectedCandidate(key);
        bool valid=std::ranges::any_of(records,[&](const auto& r){return r.source==key.plugin;});
        const auto baseline=Decode(records.front());
        for(const auto& record:records) {
            const auto terms=Decode(record);
            if(record.metadata!=records.front().metadata || !baseline || !terms || *terms!=*baseline ||
               BaselineRole(*terms)!=Role::Ordinary || (!battle && !terms->empty())) {valid=false;break;}
            guard.versions.push_back({record.source,*terms});
        }
        if(!valid) {plan.diagnostics.push_back("SKIP_GUARD "+ToString(key)+" changed or conditional generic track");continue;}
        guard.requiredTracks.assign(originalTracks.begin(),originalTracks.end());
        for(const auto& [area,witness]:replacements) {
            guard.exclusions.push_back(area);
            guard.requiredTracks.push_back(witness.track);
        }
        plan.conditionRepairs.push_back(std::move(guard)); guarded=true;
    }
    if(guarded) {
        plan.conditionWitnesses.insert(plan.conditionWitnesses.end(),witnesses.begin(),witnesses.end());
        for(auto& [area,witness]:replacements) plan.conditionWitnesses.push_back(std::move(witness));
    }
}
}
