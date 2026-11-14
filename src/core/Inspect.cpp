#include "PluginScan.h"
#include "offline/PluginFile.h"
#include "AudioAssets.h"
#include "Conditions.h"
#include <json/json.h>
#include <algorithm>
#include <iostream>
namespace {
Json::Value Clauses(const UnifiedMusic::MergePlan& plan,const UnifiedMusic::FormKey& type,const UnifiedMusic::FormKey& track) {
    Json::Value clauses(Json::arrayValue);
    for(const auto& repair:plan.conditionRepairs) if(repair.musicType==type && repair.track==track)
        for(const auto& clause:UnifiedMusic::RepairClauses(repair)) {
            Json::Value terms(Json::arrayValue);
            for(const auto& term:clause) {
                Json::Value t;
                t["function"]=term.function;t["comparison_bits"]=term.comparison;t["op"]=term.opcode;
                t["parameter"]=UnifiedMusic::ToString(term.parameter);t["parameter2"]=term.parameter2;
                t["player_reference"]=term.onPlayerReference;t["or_next"]=term.isOR;terms.append(t);
            }
            clauses.append(terms);
        }
    return clauses;
}
void Emit(const char* name,const Json::Value& value) {
    Json::StreamWriterBuilder writer;writer["indentation"]="";
    std::cout<<name<<" "<<Json::writeString(writer,value)<<'\n';
}
}
int main(int argc,char**argv) {
    try {
        UnifiedMusic::PluginScan scan;
        std::filesystem::path visible,packRoot;
        for(int i=1;i<argc;++i) {
            const std::string arg(argv[i]);
            if(arg=="--config-dir") {
                if(++i==argc) throw std::runtime_error("--config-dir requires a directory");
                scan.ReadConfigs(argv[i]);
            } else if(arg=="--config") {
                if(++i==argc) throw std::runtime_error("--config requires a file");
                scan.ReadConfig(argv[i]);
            } else if(arg=="--selector" || arg=="--selector-dir" || arg=="--audio-pack" || arg=="--visible-dir") {
                if(++i==argc) throw std::runtime_error(arg+" requires a path");
                if(arg=="--selector") scan.ReadSelector(argv[i]);
                else if(arg=="--selector-dir") scan.ReadSelectors(argv[i]);
                else if(arg=="--audio-pack") packRoot=argv[i];
                else visible=argv[i];
            } else if(arg.starts_with("--")) throw std::runtime_error("unknown option: "+arg);
            else UnifiedMusic::ReadPluginFile(scan,argv[i]);
        }
        auto plan=scan.Plan();
        UnifiedMusic::OriginalAudioPack pack;
        UnifiedMusic::AudioReader reader;
        if(!packRoot.empty()) {
            if(visible.empty()) throw std::runtime_error("--audio-pack requires --visible-dir");
            try {pack.Read(packRoot/UnifiedMusic::AudioManifest);}
            catch(const std::exception& e) {plan.diagnostics.push_back(std::string("AUDIO_PACK_SKIP ")+e.what());}
        }
        if(!visible.empty()) {
            reader=[&](const std::string& path)->std::optional<UnifiedMusic::AudioBytes> {
                auto wav=path;wav.replace(wav.size()-4,4,".wav");
                if(std::filesystem::exists(visible/wav)) return {};
                if(std::filesystem::exists(visible/path)) return UnifiedMusic::ReadAudioFile(visible/path);
                if(!packRoot.empty() && path.starts_with("music/harmonia/original/")) {
                    if(std::filesystem::exists(packRoot/wav)) return {};
                    return UnifiedMusic::ReadAudioFile(packRoot/path);
                }
                return {};
            };
            for(const auto& [type,reason]:UnifiedMusic::CheckSceneResources(plan.sceneResources,reader)) {
                plan.diagnostics.push_back("SCENE_RESOURCE_SKIP "+UnifiedMusic::ToString(type)+" "+reason);
                UnifiedMusic::DropSceneUses(plan,type);
                std::erase_if(plan.mappingRepairs,[&](const auto& r){return r.music==type;});
            }
        }
        for(auto& d:plan.diagnostics) std::cout<<d<<'\n';
        for(const auto& type:plan.clearedTypes) std::cout<<"CLEAR "<<UnifiedMusic::ToString(type)<<'\n';
        for(const auto& [type,tracks]:plan.distributions) for(const auto& track:tracks)
            std::cout<<"DISTRIBUTE "<<UnifiedMusic::ToString(type)<<" <- "<<UnifiedMusic::ToString(track)<<'\n';
        for(const auto& [area,music]:plan.assignments)
            std::cout<<"ASSIGN "<<static_cast<int>(area.kind)<<':'<<UnifiedMusic::ToString(area.form)
                <<" <- "<<UnifiedMusic::ToString(music)<<'\n';
        for(auto& [type,tracks]:plan.additions) {
            std::cout<<"ALLOW "<<UnifiedMusic::ToString(type)<<'\n';
            for(auto& track:tracks) std::cout<<"APPEND "<<UnifiedMusic::ToString(type)<<" <- "<<UnifiedMusic::ToString(track)<<'\n';
        }
        if(plan.battleType) std::cout<<"BATTLE_DEFAULT "<<UnifiedMusic::ToString(*plan.battleType)<<'\n';
        for(const auto& repair:plan.conditionRepairs) {
            std::cout<<(repair.isolate?"ISOLATE_GUARD ":repair.genericGuard?"CONDITION_GUARD ":"CONDITION_UNION ")<<UnifiedMusic::ToString(repair.track);
            if(repair.isolate) std::cout<<" IN "<<UnifiedMusic::ToString(repair.musicType);
            std::cout<<" <- ";
            for(size_t i=0;i<repair.exclusions.size();++i) {
                if(i) std::cout<<',';
                const auto& area=repair.exclusions[i];
                std::cout<<static_cast<int>(area.kind)<<':'<<UnifiedMusic::ToString(area.form);
            }
            std::cout<<'\n';
            Json::Value use;
            use["type"]=UnifiedMusic::ToString(repair.musicType);use["track"]=UnifiedMusic::ToString(repair.track);
            use["clauses"]=Clauses(plan,repair.musicType,repair.track);Emit("USE_RULES",use);
        }
        for(const auto& repair:plan.mappingRepairs)
            std::cout<<"RESTORE_MAPPING "<<static_cast<int>(repair.area.kind)<<':'<<UnifiedMusic::ToString(repair.area.form)
                <<" <- "<<UnifiedMusic::ToString(repair.music)<<'\n';
        for(const auto& rule:plan.combatRules) {
            std::cout<<"AUTO_COMBAT ";
            if(rule.areas.empty()) std::cout<<'*';
            for(size_t i=0;i<rule.areas.size();++i) {
                if(i) std::cout<<',';
                std::cout<<static_cast<int>(rule.areas[i].kind)<<':'<<UnifiedMusic::ToString(rule.areas[i].form);
            }
            std::cout<<" <- ";
            for(size_t i=0;i<rule.tracks.size();++i) {
                if(i) std::cout<<',';
                std::cout<<UnifiedMusic::ToString(rule.tracks[i]);
            }
            std::cout<<'\n';
        }
        for(const auto& rule:plan.selectors) {
            std::cout<<"SELECTOR "<<(rule.combat?"battle":"cleared")<<" -> "<<UnifiedMusic::ToString(rule.music);
            for(const auto& f:rule.filters) {
                std::cout<<" "<<static_cast<int>(f.kind)<<(f.required?" AND":" OR")<<"[";
                for(const auto& key:f.forms) std::cout<<UnifiedMusic::ToString(key)<<",";
                std::cout<<"]";
            }
            std::cout<<'\n';
        }
        if(reader && !pack.Empty()) {
            std::cout<<"AUDIO_CANDIDATES "<<plan.audioCandidates.size()<<" (offline resolved loose overlay; not engine VFS execution)\n";
            std::map<UnifiedMusic::FormKey,std::vector<UnifiedMusic::AudioPaths>> present;
            const auto append=[&](const UnifiedMusic::FormKey& type,const UnifiedMusic::FormKey& key) {
                const auto found=scan.Tracks().find(key);
                if(found==scan.Tracks().end() || found->second.empty()) return;
                const auto& track=found->second.back();
                if(track.singleAudio && !track.deleted && !track.invalid) present[type].emplace_back(track.audioPath,track.finalePath);
            };
            for(const auto& [type,versions]:scan.Records()) if(!plan.clearedTypes.contains(type) && !versions.empty())
                for(const auto& key:versions.back().tracks) append(type,key);
            for(const auto* source:{&plan.additions,&plan.distributions}) for(const auto& [type,keys]:*source)
                for(const auto& key:keys) append(type,key);
            if(plan.battleType) for(const auto& rule:plan.combatRules) for(const auto& key:rule.tracks) append(*plan.battleType,key);
            std::size_t duplicates=0;
            for(const auto& candidate:plan.audioCandidates) {
                std::string reason;
                auto pair=pack.Resolve({candidate.audioPath,candidate.finalePath},present[candidate.musicType],reader,reason);
                if(!pair && reason=="original already present") {++duplicates;continue;}
                if(pair) {
                    present[candidate.musicType].emplace_back(pair->track,pair->finale);
                    std::cout<<"AUDIO_COEXIST "<<UnifiedMusic::ToString(candidate.musicType)<<" <- "<<UnifiedMusic::ToString(candidate.track)
                        <<" main="<<pair->trackHash<<" finale="<<pair->finaleHash<<'\n';
                    Json::Value use;
                    use["type"]=UnifiedMusic::ToString(candidate.musicType);use["track"]=UnifiedMusic::ToString(candidate.track);
                    use["original"]=pair->trackHash;use["replacement"]=pair->replacementHash;
                    use["original_finale"]=pair->finaleHash;use["replacement_finale"]=pair->replacementFinaleHash;
                    use["clauses"]=Clauses(plan,candidate.musicType,candidate.track);Emit("AUDIO_USE",use);
                }
                else std::cout<<"AUDIO_SKIP "<<UnifiedMusic::ToString(candidate.track)<<" "<<reason<<'\n';
            }
            std::cout<<"AUDIO_DUPLICATE "<<duplicates<<'\n';
        } else std::cout<<"AUDIO_DISABLED optional audio pack not installed/valid\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
