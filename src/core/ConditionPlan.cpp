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
    const auto related=[&](const std::string& a,const std::string& b) {
        return a==b || DependsOn(a,b) || DependsOn(b,a);
    };
    const auto validParameter=[&](const ConditionTerm& term) {
        if(term.function==18 || term.function==707) return true;
        const auto expected=term.function==67?"CELL":term.function==310?"WRLD":term.function==359?"LCTN":term.function==74?"GLOB":"QUST";
        bool exists=false;
        for(const auto& d:definitions_) if(d.key==term.parameter) {
            if(d.deleted || d.type!=expected) return false;
            if(d.source==d.key.plugin) exists=true;
        }
        return exists;
    };
    struct Scene {
        FormKey replacement;
        Clauses clauses;
        std::vector<ConditionWitness> witnesses;
    };
    std::vector<Scene> scenes;
    bool unresolvedOrdinaryScene=false;
    for(const auto& key:available) {
        if(originalTracks.contains(key) || ScriptReferences(key)) continue;
        const auto found=tracks_.find(key);if(found==tracks_.end()) continue;
        const auto& records=found->second;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==key.plugin;});
        if(origin==records.end()) continue;
        const auto terms=DecodeConditions(*origin,true);
        std::vector<ConditionTerm> identifiable;
        if(terms) identifiable=*terms;
        else for(auto bytes:origin->conditions) {
            if(bytes.empty()) continue;
            auto one=*origin;one.conditions={std::move(bytes)};
            if(const auto decoded=DecodeConditions(one,true)) identifiable.insert(identifiable.end(),decoded->begin(),decoded->end());
        }
        if(!std::ranges::any_of(identifiable,[](const auto& t){return ConditionArea(t,true).has_value();})) continue;
        // A known dragon-only cue is a different role, even inside MUSCombat.
        if(std::ranges::any_of(identifiable,[](auto t){
            t.isOR=false;return t.function==707 && !OrdinaryConditions(std::span(&t,1),true);
        })) continue;
        const auto linked=[&](const Clauses& clauses) {
            for(const auto& clause:clauses) for(const auto& negative:clause)
                if(const auto area=ConditionArea(negative,false))
                    for(const auto& positive:identifiable)
                        if(ConditionArea(positive,true)==area && positive.onPlayerReference==negative.onPlayerReference) return true;
            return false;
        };
        ConditionWitness musicWitness{key,{},type};
        bool ready=terms.has_value();
        for(const auto& record:records) {
            const auto other=DecodeConditions(record,true);
            if(record.metadata!=origin->metadata || !other || !terms || *other!=*terms ||
               !std::ranges::all_of(*other,validParameter)) {ready=false;break;}
            musicWitness.versions.push_back({record.source,*other});
        }

        // Group by the authoring source, not a magic number of songs. Compare
        // complete added clauses: source day/night and role predicates stay out.
        struct Group { Clauses common; std::vector<ConditionWitness> witnesses; };
        std::map<std::string,Group> groups;
        for(const auto& original:originalTracks) {
            const auto f=tracks_.find(original);if(f==tracks_.end()) continue;
            const auto& versions=f->second;
            const auto base=std::ranges::find_if(versions,[&](const auto& r){return r.source==original.plugin;});
            const auto baseline=base==versions.end()?std::nullopt:Decode(*base);
            if(!baseline || !OrdinaryConditions(*baseline,battle)) continue;
            ConditionWitness witness{original,{},type};
            bool readable=true;
            for(const auto& record:versions) {
                const auto conditions=Decode(record);
                if(!conditions) {readable=false;break;}
                witness.versions.push_back({record.source,*conditions});
            }
            if(!readable) continue;
            for(const auto& version:witness.versions) {
                if(version.source==original.plugin || !related(version.source,key.plugin) || !ancestors(version.source)) continue;
                auto added=AddedClauses(*baseline,version.conditions);
                if(!added || !linked(*added)) continue;
                bool supported=true;
                std::erase_if(*added,[&](const auto& clause) {
                    // A new standalone clock restriction is song eligibility.
                    // Inside a regional/quest OR clause it must remain intact.
                    const bool clockOnly=std::ranges::all_of(clause,[](const auto& t){return t.function==18;});
                    if(clockOnly) return true;
                    if(!std::ranges::all_of(clause,validParameter) ||
                       std::ranges::any_of(clause,[](const auto& t){return t.function==707;}) ||
                       (clause.size()==1 && ConditionArea(clause.front(),true))) supported=false;
                    return false;
                });
                if(!supported || added->empty()) continue;
                const bool revoked=std::ranges::any_of(witness.versions,[&](const auto& later) {
                    return DependsOn(later.source,version.source) && std::ranges::any_of(*added,[&](const auto& clause){return !HasClause(later.conditions,clause);});
                });
                if(revoked) continue;
                auto [g,first]=groups.try_emplace(version.source,Group{*added,{}});
                if(!first) std::erase_if(g->second.common,[&](const auto& clause) {
                    return !std::ranges::any_of(*added,[&](const auto& other){return HasClause(other,clause);});
                });
                g->second.witnesses.push_back(witness);
            }
        }
        for(auto& [source,group]:groups) {
            if(group.common.empty() || !linked(group.common)) continue;
            if(!ready) {
                plan.sceneUses[type].insert(key);
                unresolvedOrdinaryScene=true;
                plan.diagnostics.push_back("SCENE_UNSUPPORTED "+ToString(type)+" <- "+ToString(key)+"; ordinary uses paused, other roles retained");
                continue;
            }
            group.witnesses.push_back(musicWitness);
            scenes.push_back({key,std::move(group.common),std::move(group.witnesses)});
            if(std::ranges::none_of(plan.sceneResources,[&](const auto& r){return r.musicType==type && r.track==key;}))
                plan.sceneResources.push_back({type,key,origin->audioPath,origin->finalePath});
            plan.sceneUses[type].insert(key);
            plan.diagnostics.push_back("SCENE_POLICY "+ToString(type)+" <- "+ToString(key)+" source="+source+"; inherit authored clauses");
        }
    }
    if(scenes.empty() && !unresolvedOrdinaryScene) return;
    const auto blockAddition=[&](const FormKey& key) {
        if(auto it=plan.additions.find(type);it!=plan.additions.end()) std::erase(it->second,key);
        if(auto it=plan.distributions.find(type);it!=plan.distributions.end()) std::erase(it->second,key);
        if(battle) {
            for(auto& rule:plan.combatRules) std::erase(rule.tracks,key);
            std::erase_if(plan.combatRules,[](const auto& rule){return rule.tracks.empty();});
        }
        std::erase_if(plan.audioCandidates,[&](const auto& c){return c.musicType==type && c.track==key;});
        std::erase_if(plan.conditionRepairs,[&](const auto& r){return r.musicType==type && r.track==key;});
    };
    for(const auto& key:candidates) {
        const auto found=tracks_.find(key);if(found==tracks_.end()) continue;
        const auto& records=found->second;
        const auto origin=std::ranges::find_if(records,[&](const auto& r){return r.source==key.plugin;});
        const auto baseline=origin==records.end()?std::nullopt:Decode(*origin);
        if(!baseline || !OrdinaryConditions(*baseline,battle)) {
            std::erase_if(plan.audioCandidates,[&](const auto& c){return c.musicType==type && c.track==key;});
            continue;
        }
        plan.sceneUses[type].insert(key);
        if(unresolvedOrdinaryScene || ScriptReferences(key)) {blockAddition(key);continue;}
        ConditionRepair guard;
        guard.track=key;guard.musicType=type;guard.genericGuard=true;guard.isolate=protectedCandidate(key);
        bool valid=true;
        for(const auto& record:records) {
            const auto conditions=Decode(record);
            if(record.metadata!=origin->metadata || !conditions || (!originalTracks.contains(key) && *conditions!=*baseline)) {valid=false;break;}
            guard.versions.push_back({record.source,*conditions});
        }
        if(!valid) {blockAddition(key);plan.diagnostics.push_back("SKIP_GUARD "+ToString(key)+" unsupported ordinary use; related additions paused");continue;}
        for(const auto& scene:scenes) {
            // Preserve an explicit positive assignment as that song's own use.
            if(std::ranges::any_of(guard.versions,[](const auto& v){
                return std::ranges::any_of(v.conditions,[](const auto& t){return ConditionArea(t,true).has_value();});
            })) continue;
            for(const auto& clause:scene.clauses) {
                bool revoked=false;
                for(const auto& later:guard.versions) for(const auto& prior:guard.versions)
                    if(DependsOn(later.source,prior.source) && HasClause(prior.conditions,clause) && !HasClause(later.conditions,clause)) revoked=true;
                if(revoked) continue;
                const bool already=std::ranges::all_of(guard.versions,[&](const auto& v) {
                    if(std::ranges::any_of(guard.versions,[&](const auto& other){return DependsOn(other.source,v.source);})) return true;
                    return HasClause(v.conditions,clause);
                });
                if(already || std::ranges::any_of(guard.clauses,[&](const auto& c){return HasClause(c,clause);})) continue;
                guard.clauses.push_back(clause);
                if(clause.size()==1) if(const auto a=Exclusion(clause.front())) guard.exclusions.push_back(*a);
                guard.requiredTracks.push_back(scene.replacement);
            }
        }
        if(guard.clauses.empty()) continue;
        const auto existing=std::ranges::find_if(plan.conditionRepairs,[&](const auto& r){return r.track==key && r.musicType==type;});
        if(existing==plan.conditionRepairs.end()) plan.conditionRepairs.push_back(std::move(guard));
        else {
            for(const auto& clause:guard.clauses)
                if(!std::ranges::any_of(existing->clauses,[&](const auto& c){return HasClause(c,clause);})) existing->clauses.push_back(clause);
            existing->requiredTracks.insert(existing->requiredTracks.end(),guard.requiredTracks.begin(),guard.requiredTracks.end());
            for(const auto& a:guard.exclusions) if(std::ranges::find(existing->exclusions,a)==existing->exclusions.end()) existing->exclusions.push_back(a);
        }
    }
    if(unresolvedOrdinaryScene) {
        // No inferred guard survives for this ordinary role; independent dragon
        // or explicitly dedicated distributions remain available.
        DropSceneUses(plan,type);
        return;
    }
    for(const auto& scene:scenes) {
        for(const auto& witness:scene.witnesses)
            if(std::ranges::none_of(plan.conditionWitnesses,[&](const auto& w){return w.musicType==type && w.track==witness.track;}))
                plan.conditionWitnesses.push_back(witness);
        const bool alwaysPresent=std::ranges::all_of(music,[&](const auto& r) {
            if(std::ranges::any_of(music,[&](const auto& later){return DependsOn(later.source,r.source);})) return true;
            return std::ranges::find(r.tracks,scene.replacement)!=r.tracks.end();
        });
        bool planned=false;
        for(const auto* additions:{&plan.additions,&plan.distributions}) if(const auto it=additions->find(type);it!=additions->end())
            planned|=std::ranges::find(it->second,scene.replacement)!=it->second.end();
        if(battle) for(const auto& rule:plan.combatRules) planned|=std::ranges::find(rule.tracks,scene.replacement)!=rule.tracks.end();
        // Re-add an authored scene song without rewriting its OR/quest logic.
        if(!alwaysPresent && !planned) plan.additions[type].push_back(scene.replacement);
    }
}

void DropSceneUses(MergePlan& plan,const FormKey& type) {
    const auto found=plan.sceneUses.find(type);if(found==plan.sceneUses.end()) return;
    const auto& uses=found->second;
    for(auto* additions:{&plan.additions,&plan.distributions}) if(auto it=additions->find(type);it!=additions->end())
        std::erase_if(it->second,[&](const auto& track){return uses.contains(track);});
    if(plan.battleType && type==*plan.battleType) {
        for(auto& rule:plan.combatRules) std::erase_if(rule.tracks,[&](const auto& key){return uses.contains(key);});
        std::erase_if(plan.combatRules,[](const auto& rule){return rule.tracks.empty();});
    }
    std::erase_if(plan.conditionRepairs,[&](const auto& r){return r.musicType==type && uses.contains(r.track);});
    std::erase_if(plan.audioCandidates,[&](const auto& r){return r.musicType==type && uses.contains(r.track);});
    std::erase_if(plan.conditionWitnesses,[&](const auto& r){return r.musicType==type;});
    std::erase_if(plan.sceneResources,[&](const auto& r){return r.musicType==type;});
    plan.sceneUses.erase(found);
}
}
