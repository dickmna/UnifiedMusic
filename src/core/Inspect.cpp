#include "PluginScan.h"
#include <iostream>
int main(int argc,char**argv) {
    try {
        UnifiedMusic::PluginScan scan;
        for(int i=1;i<argc;++i) scan.Read(argv[i]);
        auto plan=scan.Plan();
        for(auto& d:plan.diagnostics) std::cout<<d<<'\n';
        for(auto& [type,tracks]:plan.additions) {
            std::cout<<"ALLOW "<<UnifiedMusic::ToString(type)<<'\n';
            for(auto& track:tracks) std::cout<<"APPEND "<<UnifiedMusic::ToString(type)<<" <- "<<UnifiedMusic::ToString(track)<<'\n';
        }
        if(plan.battleType) std::cout<<"BATTLE_DEFAULT "<<UnifiedMusic::ToString(*plan.battleType)<<'\n';
        for(const auto& repair:plan.conditionRepairs) {
            std::cout<<"CONDITION_UNION "<<UnifiedMusic::ToString(repair.track)<<" <- ";
            for(size_t i=0;i<repair.exclusions.size();++i) {
                if(i) std::cout<<',';
                const auto& area=repair.exclusions[i];
                std::cout<<static_cast<int>(area.kind)<<':'<<UnifiedMusic::ToString(area.form);
            }
            std::cout<<'\n';
        }
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
