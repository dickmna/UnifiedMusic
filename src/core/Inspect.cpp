#include "PluginScan.h"
#include <iostream>
int main(int argc,char**argv) {
    try {
        UnifiedMusic::PluginScan scan;
        for(int i=1;i<argc;++i) {
            const std::string arg(argv[i]);
            if(arg=="--config-dir") {
                if(++i==argc) throw std::runtime_error("--config-dir requires a directory");
                scan.ReadConfigs(argv[i]);
            } else if(arg=="--config") {
                if(++i==argc) throw std::runtime_error("--config requires a file");
                scan.ReadConfig(argv[i]);
            } else scan.Read(argv[i]);
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
            std::cout<<(repair.genericGuard?"CONDITION_GUARD ":"CONDITION_UNION ")<<UnifiedMusic::ToString(repair.track)<<" <- ";
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
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
