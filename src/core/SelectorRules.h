#pragma once
#include <optional>
#include <span>
#include <utility>

namespace UnifiedMusic {
struct FilterMatch { bool required{}, matched{}, actor{}; };
// CME scoring: every required filter must match, at least one optional filter
// must match when any exist; optional filters collectively contribute one point.
inline std::pair<bool,int> SelectorRank(std::span<const FilterMatch> filters) {
    bool hasOptional=false,optional=false,actor=false;int score=0;
    for(const auto& f:filters) {
        if(f.required && !f.matched) return {false,0};
        if(!f.required) {hasOptional=true;optional|=f.matched;}
        else ++score;
        actor|=f.matched && f.actor;
    }
    if(hasOptional && !optional) return {false,0};
    return {actor,score+int(optional)};
}
struct RankedMusic { std::size_t identity{}; bool actor{}; int score{}; bool available{true}; };
inline std::optional<std::size_t> SelectMusic(std::span<const RankedMusic> candidates) {
    std::optional<std::size_t> best;bool ambiguous=false,unavailable=false;
    for(std::size_t i=0;i<candidates.size();++i) {
        const auto& c=candidates[i];if(c.score<=0) continue;
        if(!best || std::pair{c.actor,c.score}>std::pair{candidates[*best].actor,candidates[*best].score}) {
            best=i;ambiguous=false;unavailable=!c.available;
        } else if(std::pair{c.actor,c.score}==std::pair{candidates[*best].actor,candidates[*best].score}) {
            ambiguous|=c.identity!=candidates[*best].identity;
            unavailable|=!c.available;
        }
    }
    // An unavailable Boss/quest choice must not silently turn into a broader
    // geographic rule. Keep the engine's original choice in that situation.
    return ambiguous || unavailable?std::nullopt:best;
}
}
