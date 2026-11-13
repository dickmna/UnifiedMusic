#include "SelectorRules.h"
#include <array>
#include <cstdio>
#include <cstdlib>
int main() {
    using namespace UnifiedMusic;
    unsigned checks=0;
    const auto check=[&](bool value){++checks;if(!value) {std::fprintf(stderr,"selector check %u failed\n",checks);std::exit(1);}};
    check(SelectorRank({}).second==0);
    // Exhaust every required/optional, matched/unmatched and actor/geographic
    // combination for three filters, independently count their logical result.
    for(unsigned bits=0;bits<512;++bits) {
        std::array<FilterMatch,3> filters;bool allowed=true,anyOptional=false,optionalMatched=false,actor=false;int requiredCount=0;
        for(unsigned i=0;i<3;++i) {
            const unsigned n=(bits>>(i*3))&7;
            filters[i]={bool(n&1),bool(n&2),bool(n&4)};
            if(n&1) {++requiredCount;if(!(n&2)) allowed=false;}
            else {anyOptional=true;optionalMatched|=bool(n&2);}
            if((n&6)==6) actor=true;
        }
        allowed&=!anyOptional || optionalMatched;
        const auto rank=SelectorRank(filters);
        check(rank==std::pair{allowed && actor,allowed?requiredCount+int(optionalMatched):0});
    }
    check(!SelectMusic({}));
    const std::array geographic{RankedMusic{1,false,6},RankedMusic{2,true,1}};
    check(SelectMusic(geographic)==1); // Explicit Boss target outranks geography.
    const std::array ambiguous{RankedMusic{1,false,2},RankedMusic{2,false,2}};
    check(!SelectMusic(ambiguous));
    const std::array duplicate{RankedMusic{1,false,2},RankedMusic{1,false,2}};
    check(SelectMusic(duplicate)==0);
    const std::array unavailable{RankedMusic{1,true,5,false},RankedMusic{2,false,1}};
    check(!SelectMusic(unavailable)); // A blocked Boss choice does not enable generic regional music.
    const std::array reversed{RankedMusic{2,false,1},RankedMusic{1,true,5,false}};
    check(!SelectMusic(reversed));
    const std::array duplicateBlocked{RankedMusic{1,true,5},RankedMusic{1,true,5,false}};
    check(!SelectMusic(duplicateBlocked));
    const std::array unrelatedBlocked{RankedMusic{1,false,1,false},RankedMusic{2,true,1}};
    check(SelectMusic(unrelatedBlocked)==1);
    const std::array zero{RankedMusic{1,true,0},RankedMusic{2,false,0}};
    check(!SelectMusic(zero));
    const std::array improved{RankedMusic{1,false,1},RankedMusic{2,false,1},RankedMusic{3,true,1}};
    check(SelectMusic(improved)==2);
    std::printf("Selector logic: %u checks passed\n",checks);
}
