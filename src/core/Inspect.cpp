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
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
