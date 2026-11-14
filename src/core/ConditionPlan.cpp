#include "PluginScan.h"
#include "Conditions.h"
#include <algorithm>
#include <functional>

namespace UnifiedMusic {
namespace {
std::optional<std::vector<ConditionTerm>> Decode(const TrackRecord& record) {
    return DecodeConditions(record);
}
std::optional<Area> Exclusion(const ConditionTerm& term) {
    if(term.isOR) return {};
    if(!((term.opcode==0 && term.comparison==0) ||
         (term.opcode==1 && term.comparison==0x3f800000))) return {};
    if(term.function==67) return Area{AreaKind::Cell,term.parameter};
    if(term.function==310) return Area{AreaKind::Worldspace,term.parameter};
    if(term.function==359) return Area{AreaKind::Location,term.parameter};
    return {};
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
    for(const auto& key:originalTracks) {
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) continue;
        const auto skip=[&](const char* reason){plan.diagnostics.push_back("SKIP_CONDITIONS "+ToString(key)+" "+reason);};
        const bool protectedTrack=ScriptReferences(key);
        if(protectedTrack) skip("direct script reference; policy evidence only");
        const auto& records=found->second;
        auto origin=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) {skip("missing original track");continue;}
        const auto baseline=Decode(*origin);
        if(!baseline) {skip("unsupported original conditions");continue;}
        ConditionRepair repair;
        repair.track=key;
        repair.musicType=type;
        repair.isolate=protectedTracks.contains(key);
        std::map<std::string,std::set<ConditionTerm>> contributions;
        std::map<std::string,std::set<std::string>> parents;
        std::set<ConditionTerm> combined;
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
                auto term=(*conditions)[i];term.opcode=0;term.comparison=0;
                extras.insert(term);
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
        for(const auto& term:combined) {
            repair.clauses.push_back({term});
            const auto area=*Exclusion(term);
            if(std::ranges::find(repair.exclusions,area)==repair.exclusions.end()) repair.exclusions.push_back(area);
        }
        std::set<FormKey> required(baseMusic->tracks.begin(),baseMusic->tracks.end());
        for(const auto& [source,extras]:contributions) if(!extras.empty()) {
            for(const auto& type:music) if(type.source==source || parents.at(source).contains(type.source)) {
                required.insert(type.tracks.begin(),type.tracks.end());
            }
        }
        repair.requiredTracks.assign(required.begin(),required.end());
        const bool conflict=std::ranges::any_of(contributions,[&](const auto& entry){return entry.first!=key.plugin && entry.second!=combined;});
        if(conflict && !protectedTrack) plan.conditionRepairs.push_back(std::move(repair));
    }

    std::set<FormKey> available,candidates;
    for(const auto& record:music) available.insert(record.tracks.begin(),record.tracks.end());
    candidates=available;
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
    struct Scene { FormKey replacement; std::vector<ConditionTerm> scope; ConditionWitness evidence, music; };
    std::vector<Scene> scenes;
    bool unsupportedScene=false;
    for(const auto& key:available) {
        // Shared ownership does not erase this authored use. The scene song
        // itself is read-only; only ordinary uses receive local guard copies.
        if(originalTracks.contains(key) || ScriptReferences(key)) continue;
        const auto found=tracks_.find(key);
        if(found==tracks_.end()) continue;
        const auto& records=found->second;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) continue;
        const auto terms=Decode(*origin);
        // Even an unsupported expression may contain an identifiable regional
        // gate. Use that only to pause additions, never to invent a wider ban.
        std::vector<ConditionTerm> identifiable;
        if(terms) identifiable=*terms;
        else for(auto bytes:origin->conditions) {
            if(bytes.empty()) continue;
            bytes[0]&=~1u;
            auto one=*origin;one.conditions={std::move(bytes)};
            if(const auto decoded=Decode(one)) identifiable.insert(identifiable.end(),decoded->begin(),decoded->end());
        }
        if(identifiable.empty()) continue;
        std::vector<ConditionTerm> scope;
        bool valid=terms.has_value(),geography=false;
        for(const auto& term:identifiable) {
            if(term.isOR) valid=false;
            if(term.function==707) {
                if(!OrdinaryConditions(std::span(&term,1),true)) valid=false;
            }
            else if(term.function==18) {}
            else if(const auto area=ConditionArea(term,true)) {
                const auto definition=areaForms_.find(area->form);
                if(definition==areaForms_.end() || definition->second!=area->kind || deletedAreas_.contains(area->form)) valid=false;
                geography=true;
            } else if(term.function==67 || term.function==310 || term.function==359) valid=false;
            else {
                const auto expected=term.function==74?"GLOB":"QUST";
                bool exists=false;
                for(const auto& d:definitions_) if(d.key==term.parameter) {
                    if(d.deleted || d.type!=expected) valid=false;
                    if(d.source==d.key.plugin && d.type==expected) exists=true;
                }
                if(!exists) valid=false;
            }
            scope.push_back(term);
        }
        if(!geography) continue;
        ConditionWitness witness{key,{}};
        for(const auto& record:records) {
            const auto other=Decode(record);
            if(record.metadata!=origin->metadata || !other || !terms || *other!=*terms) {valid=false;break;}
            witness.versions.push_back({record.source,*other});
        }
        // A matching exclusion from the same source/dependency family is enough
        // for our takeover default. A coincidental song in the same world is not.
        std::optional<ConditionWitness> evidence;
        for(const auto& original:originalTracks) {
            const auto found=tracks_.find(original);if(found==tracks_.end()) continue;
            const auto& versions=found->second;
            const auto base=std::ranges::find_if(versions,[&](const auto& r){return r.source==original.plugin;});
            const auto baseline=base==versions.end()?std::nullopt:Decode(*base);
            if(!baseline || !OrdinaryConditions(*baseline,battle)) continue;
            ConditionWitness witness{original,{}};
            bool readable=true;
            for(const auto& record:versions) {
                const auto terms=Decode(record);
                if(!terms || record.metadata!=base->metadata) {readable=false;break;}
                witness.versions.push_back({record.source,*terms});
            }
            if(!readable) continue;
            for(const auto& version:witness.versions) {
                if(version.source==original.plugin ||
                   !(version.source==key.plugin || DependsOn(version.source,key.plugin) || DependsOn(key.plugin,version.source))) continue;
                for(const auto& term:version.conditions) {
                    const auto area=Exclusion(term);if(!area) continue;
                    // Standalone AND exclusion only. A reordered baseline or
                    // an extra quest condition need not invalidate the source
                    // relationship; neither may an OR term become a wider ban.
                    if(!HasClause(version.conditions,std::span(&term,1))) continue;
                    const bool revoked=std::ranges::any_of(witness.versions,[&](const auto& later) {
                        return DependsOn(later.source,version.source) && !HasClause(later.conditions,std::span(&term,1));
                    });
                    if(revoked) continue;
                    for(const auto& positive:scope) if(ConditionArea(positive,true)==area && positive.onPlayerReference==term.onPlayerReference)
                        evidence=witness;
                }
            }
            if(evidence) break;
        }
        if(evidence && !valid) {
            unsupportedScene=true;
            plan.diagnostics.push_back("SCENE_UNSUPPORTED "+ToString(type)+" <- "+ToString(key)+"; additions for this type paused");
        } else if(evidence) {
            scenes.push_back({key,std::move(scope),*evidence,std::move(witness)});
            plan.sceneResources.push_back({type,key,origin->audioPath,origin->finalePath});
            plan.diagnostics.push_back("SCENE_TAKEOVER "+ToString(type)+" <- "+ToString(key)+" (related exclusion; not a claim of unanimous author intent)");
        }
    }
    if(unsupportedScene) {
        plan.additions.erase(type);plan.distributions.erase(type);
        if(battle) {plan.combatRules.clear();plan.battleMergeSafe=false;}
        std::erase_if(plan.conditionRepairs,[&](const auto& r){return r.musicType==type;});
        std::erase_if(plan.audioCandidates,[&](const auto& r){return r.musicType==type;});
        std::erase_if(plan.sceneResources,[&](const auto& r){return r.musicType==type;});
        return;
    }
    if(scenes.empty()) return;
    const auto blockAddition=[&](const FormKey& key) {
        if(auto it=plan.additions.find(type);it!=plan.additions.end()) std::erase(it->second,key);
        if(auto it=plan.distributions.find(type);it!=plan.distributions.end()) std::erase(it->second,key);
        if(battle) {
            for(auto& rule:plan.combatRules) std::erase(rule.tracks,key);
            std::erase_if(plan.combatRules,[](const auto& rule){return rule.tracks.empty();});
        }
        std::erase_if(plan.audioCandidates,[&](const auto& c){return c.musicType==type && c.track==key;});
    };
    for(const auto& key:candidates) {
        const auto found=tracks_.find(key);if(found==tracks_.end()) continue;
        const auto& records=found->second;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==key.plugin;});
        const auto baseline=origin==records.end()?std::nullopt:Decode(*origin);
        if(!baseline || !OrdinaryConditions(*baseline,battle)) {
            // Do not turn a dedicated/unknown audio slot into generic additions.
            std::erase_if(plan.audioCandidates,[&](const auto& c){return c.musicType==type && c.track==key;});
            continue;
        }
        if(ScriptReferences(key)) {blockAddition(key);plan.diagnostics.push_back("SKIP_GUARD "+ToString(key)+" direct script reference");continue;}
        ConditionRepair guard;
        guard.track=key; guard.musicType=type; guard.genericGuard=true;
        guard.isolate=protectedCandidate(key);
        bool valid=true;
        for(const auto& record:records) {
            const auto terms=Decode(record);
            if(record.metadata!=origin->metadata || !terms || (!originalTracks.contains(key) && *terms!=*baseline)) {valid=false;break;}
            guard.versions.push_back({record.source,*terms});
        }
        if(!valid) {blockAddition(key);plan.diagnostics.push_back("SKIP_GUARD "+ToString(key)+" unsupported ordinary use; related additions paused");continue;}
        for(const auto& scene:scenes) {
            const auto clause=NegateScope(scene.scope);
            bool explicitException=false;
            for(const auto& version:guard.versions) {
                // Positive regional eligibility remains the author's own use.
                for(const auto& term:version.conditions) if(ConditionArea(term,true)) explicitException=true;
                // A dependent patch can deliberately remove an earlier ban.
                for(const auto& prior:guard.versions) if(DependsOn(version.source,prior.source))
                    for(const auto& term:prior.conditions) if(const auto area=Exclusion(term)) {
                        if(!HasClause(prior.conditions,std::span(&term,1))) continue;
                        const bool relevant=std::ranges::any_of(scene.scope,[&](const auto& t){return ConditionArea(t,true)==area;});
                        if(relevant && !HasClause(version.conditions,std::span(&term,1))) explicitException=true;
                    }
            }
            if(explicitException) continue;
            const bool already=std::ranges::all_of(guard.versions,[&](const auto& version) {
                // A master version cannot win over its own dependent override.
                if(std::ranges::any_of(guard.versions,[&](const auto& other){return DependsOn(other.source,version.source);})) return true;
                if(HasClause(version.conditions,clause)) return true;
                return std::ranges::any_of(version.conditions,[&](const auto& term) {
                    const auto area=Exclusion(term);if(!area) return false;
                    if(!HasClause(version.conditions,std::span(&term,1))) return false;
                    return std::ranges::any_of(scene.scope,[&](const auto& t){return ConditionArea(t,true)==area && t.onPlayerReference==term.onPlayerReference;});
                });
            });
            if(already) continue;
            guard.clauses.push_back(clause);
            if(clause.size()==1) if(const auto area=Exclusion(clause.front())) guard.exclusions.push_back(*area);
            guard.requiredTracks.push_back(scene.replacement);
        }
        if(guard.clauses.empty()) continue;
        const auto existing=std::ranges::find_if(plan.conditionRepairs,[&](const auto& r){return r.track==key && r.musicType==type;});
        if(existing==plan.conditionRepairs.end()) plan.conditionRepairs.push_back(std::move(guard));
        else {
            // Legacy authored exclusions and inferred scene clauses share one
            // local instance. Keep the former even if the new scope is narrower.
            for(const auto& clause:guard.clauses)
                if(std::ranges::find(existing->clauses,clause)==existing->clauses.end()) existing->clauses.push_back(clause);
            existing->requiredTracks.insert(existing->requiredTracks.end(),guard.requiredTracks.begin(),guard.requiredTracks.end());
            for(const auto& a:guard.exclusions) if(std::ranges::find(existing->exclusions,a)==existing->exclusions.end()) existing->exclusions.push_back(a);
        }
    }
    for(const auto& scene:scenes) {
        auto evidence=scene.evidence,musicWitness=scene.music;
        evidence.musicType=type;musicWitness.musicType=type;
        plan.conditionWitnesses.push_back(std::move(evidence));plan.conditionWitnesses.push_back(std::move(musicWitness));
    }
}
}
