#include "PluginScan.h"
#include "AudioAssets.h"
#include <iostream>
int main(int argc,char**argv) {
    try {
        UnifiedMusic::PluginScan scan;
        std::vector<std::filesystem::path> archives;
        std::filesystem::path visible,cache;
        for(int i=1;i<argc;++i) {
            const std::string arg(argv[i]);
            if(arg=="--config-dir") {
                if(++i==argc) throw std::runtime_error("--config-dir requires a directory");
                scan.ReadConfigs(argv[i]);
            } else if(arg=="--config") {
                if(++i==argc) throw std::runtime_error("--config requires a file");
                scan.ReadConfig(argv[i]);
            } else if(arg=="--selector" || arg=="--selector-dir" || arg=="--original-archive" || arg=="--visible-dir" || arg=="--audio-cache") {
                if(++i==argc) throw std::runtime_error(arg+" requires a path");
                if(arg=="--selector") scan.ReadSelector(argv[i]);
                else if(arg=="--selector-dir") scan.ReadSelectors(argv[i]);
                else if(arg=="--original-archive") archives.emplace_back(argv[i]);
                else if(arg=="--visible-dir") visible=argv[i];
                else cache=argv[i];
            } else if(arg.starts_with("--")) throw std::runtime_error("unknown option: "+arg);
            else scan.Read(argv[i]);
        }
        auto plan=scan.Plan();
        for(auto& d:plan.diagnostics) std::cout<<d<<'\n';
        for(const auto& [type,tracks]:plan.distributions) for(const auto& track:tracks)
            std::cout<<"DISTRIBUTE "<<UnifiedMusic::ToString(type)<<" <- "<<UnifiedMusic::ToString(track)<<'\n';
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
        if(!archives.empty()) {
            if(visible.empty()) throw std::runtime_error("audio comparison requires --visible-dir");
            UnifiedMusic::OriginalAudio original;for(const auto& path:archives) original.AddArchive(path);
            const auto reader=[&](const std::string& path)->std::optional<UnifiedMusic::AudioBytes> {
                auto wav=path;wav.replace(wav.size()-4,4,".wav");
                if(std::filesystem::exists(visible/wav)) return {};
                if(std::filesystem::exists(visible/path)) return UnifiedMusic::ReadAudioFile(visible/path);
                return original.Read(path);
            };
            std::cout<<"AUDIO_CANDIDATES "<<plan.audioCandidates.size()<<" (offline loose overlay; not engine BSA precedence)\n";
            std::size_t unchanged=0;
            for(const auto& candidate:plan.audioCandidates) {
                std::string reason;
                auto pair=UnifiedMusic::CompareAudio(original,reader,candidate.audioPath,candidate.finalePath,reason);
                if(!pair && reason=="unchanged audio") {++unchanged;continue;}
                if(pair && !cache.empty()) try {
                    UnifiedMusic::CacheAudio(cache,pair->track);
                    if(!pair->finale.empty()) UnifiedMusic::CacheAudio(cache,pair->finale);
                } catch(const std::exception& e) {reason=e.what();pair.reset();}
                if(pair) std::cout<<"AUDIO_COEXIST "<<UnifiedMusic::ToString(candidate.musicType)<<" <- "<<UnifiedMusic::ToString(candidate.track)
                    <<" main="<<pair->trackHash<<" finale="<<pair->finaleHash<<'\n';
                else std::cout<<"AUDIO_SKIP "<<UnifiedMusic::ToString(candidate.track)<<" "<<reason<<'\n';
            }
            std::cout<<"AUDIO_UNCHANGED "<<unchanged<<'\n';
        } else if(!cache.empty() || !visible.empty()) throw std::runtime_error("audio options require --original-archive");
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
