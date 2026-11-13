// Combat entry points adapted from SeaSparrowOG/CombatMusic, AGPL-3.0.
// See PROVENANCE.md for the pinned revision and the deliberately limited changes.
#include "CombatSelector.h"
#include "core/PluginScan.h"
#include "core/SelectorRules.h"
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <utility>

namespace UnifiedMusic {
namespace {
struct Filter { SelectorKind kind; bool required; std::vector<RE::TESForm*> forms; };
struct Rule { bool combat; RE::BGSMusicType* music; std::vector<Filter> filters; };
std::vector<Rule> rules;
RE::BGSMusicType* activeCombat{};
using Default=RE::BGSDefaultObjectManager::DefaultObject;
using GetDefault=RE::BGSMusicType*(*)(RE::DEFAULT_OBJECT);
std::array<GetDefault,6> original{};
std::array<std::uintptr_t,6> sites{};
bool CheckSites() {
    constexpr std::uint64_t ids[]{46897,46870,46870,18369,51653,46895};
    constexpr std::ptrdiff_t offsets[]{0x20,0x1C1,0x22D,0x173,0x29C,0x3B4};
    std::optional<std::uintptr_t> callee;
    for(std::size_t i=0;i<sites.size();++i) {
        sites[i]=REL::Relocation<std::uintptr_t>{REL::ID(ids[i]),offsets[i]}.address();
        if(*reinterpret_cast<const std::uint8_t*>(sites[i])!=0xE8) return false;
        std::int32_t relative;std::memcpy(&relative,reinterpret_cast<const void*>(sites[i]+1),4);
        const auto target=sites[i]+5+relative;
        if(callee && target!=*callee) return false;
        callee=target;
    }
    return true;
}
bool Matches(const Filter& filter,RE::PlayerCharacter* player) {
    const auto contains=[&](RE::TESForm* form){return form && std::ranges::find(filter.forms,form)!=filter.forms.end();};
    switch(filter.kind) {
    case SelectorKind::Worldspace:return contains(player->GetWorldspace());
    case SelectorKind::Cell:return contains(player->GetParentCell());
    case SelectorKind::Location:case SelectorKind::LocationKeyword: {
        std::set<RE::BGSLocation*> visited;
        for(auto* loc=player->GetCurrentLocation();loc && visited.size()<128 && visited.insert(loc).second;loc=loc->parentLoc) {
            if(filter.kind==SelectorKind::Location) {if(contains(loc)) return true;}
            else for(auto* form:filter.forms) if(loc->HasKeyword(static_cast<RE::BGSKeyword*>(form))) return true;
        }
        return false;
    }
    case SelectorKind::Target:case SelectorKind::TargetKeyword: {
        auto target=player->currentCombatTarget.get();if(!target) return false;
        auto* base=target->GetActorBase();if(!base) return false;
        if(filter.kind==SelectorKind::Target) return contains(base);
        auto* race=target->GetRace();
        for(auto* form:filter.forms) {
            auto* keyword=static_cast<RE::BGSKeyword*>(form);
            if(base->HasKeyword(keyword) || (race && race->HasKeyword(keyword))) return true;
        }
        return false;
    }
    }
    return false;
}
RE::BGSMusicType* Choose(bool combat,RE::BGSMusicType* fallback) {
    if(!fallback) return fallback;
    try {
        const auto* objects=RE::BGSDefaultObjectManager::GetSingleton();
        if(!objects || fallback!=objects->GetObject<RE::BGSMusicType>(combat?Default::kBattleMusic:Default::kDungeonClearedMusic)) return fallback;
        auto* player=RE::PlayerCharacter::GetSingleton();if(!player) return fallback;
        std::vector<RankedMusic> ranked;ranked.reserve(rules.size());
        for(const auto& rule:rules) {
            RankedMusic rank{reinterpret_cast<std::size_t>(rule.music),false,0,false};
            if(rule.combat==combat) {
                std::vector<FilterMatch> matches;
                for(const auto& f:rule.filters) matches.push_back({f.required,Matches(f,player),f.kind==SelectorKind::Target || f.kind==SelectorKind::TargetKeyword});
                const auto score=SelectorRank(matches);rank.actor=score.first;rank.score=score.second;
                // Do not take over a type already playing for another owner.
                const auto status=rule.music->typeStatus.underlying();
                const bool idle=status==0 || status==4 || (combat && rule.music==activeCombat) || rule.music==fallback;
                rank.available=idle && rank.score>0 && std::ranges::any_of(rule.music->tracks,[](auto* track){return track && track->TestCanPlay();});
            }
            ranked.push_back(rank);
        }
        const auto choice=SelectMusic(ranked);if(!choice) return fallback;
        return rules[*choice].music;
    } catch(const std::exception& e) {REX::WARN("Music selector preserved engine choice: {}",e.what());return fallback;}
}
RE::BGSMusicType* Stop(unsigned index,RE::DEFAULT_OBJECT object) {
    auto* fallback=original[index](object);
    if(object!=Default::kBattleMusic || !activeCombat) return fallback;
    return std::exchange(activeCombat,nullptr);
}
RE::BGSMusicType* Revert(RE::DEFAULT_OBJECT object) {return Stop(0,object);}
RE::BGSMusicType* Start(RE::DEFAULT_OBJECT object) {
    auto* fallback=original[1](object);
    if(object!=Default::kBattleMusic) return fallback;
    auto* selected=Choose(true,fallback);
    activeCombat=selected!=fallback?selected:nullptr;
    return selected;
}
RE::BGSMusicType* End(RE::DEFAULT_OBJECT object) {return Stop(2,object);}
RE::BGSMusicType* Cleared(RE::DEFAULT_OBJECT object) {
    auto* fallback=original[3](object);
    return object==Default::kDungeonClearedMusic?Choose(false,fallback):fallback;
}
RE::BGSMusicType* Discovery(RE::DEFAULT_OBJECT object) {
    if(object==Default::kBattleMusic && activeCombat) return activeCombat;
    return original[4](object);
}
RE::BGSMusicType* Load(RE::DEFAULT_OBJECT object) {
    auto* fallback=original[5](object);
    if(object!=Default::kBattleMusic) return fallback;
    auto* selected=Choose(true,fallback);
    activeCombat=selected!=fallback?selected:nullptr;
    return selected;
}
}
bool PrepareSelectors(const MergePlan& plan) {
    rules.clear();auto* data=RE::TESDataHandler::GetSingleton();if(!data) return plan.selectors.empty();
    constexpr RE::FormType types[]{RE::FormType::WorldSpace,RE::FormType::Cell,RE::FormType::Location,RE::FormType::Keyword,RE::FormType::NPC,RE::FormType::Keyword};
    std::vector<Rule> pending;
    for(const auto& item:plan.selectors) {
        auto* type=data->LookupForm<RE::BGSMusicType>(item.music.id,item.music.plugin);
        if(!type) return false;
        Rule rule{item.combat,type,{}};
        for(const auto& filter:item.filters) {
            Filter f{filter.kind,filter.required,{}};
            for(const auto& key:filter.forms) {
                auto* form=data->LookupForm<RE::TESForm>(key.id,key.plugin);
                if(!form || form->GetFormType()!=types[static_cast<int>(filter.kind)]) return false;
                f.forms.push_back(form);
            }
            rule.filters.push_back(std::move(f));
        }
        pending.push_back(std::move(rule));
    }
    if(!pending.empty() && !CheckSites()) {REX::WARN("CME hook preflight failed; preserve complete music batch");return false;}
    rules=std::move(pending);return true;
}
bool InstallSelectors() {
    if(rules.empty()) return true;
    constexpr GetDefault hooks[]{Revert,Start,End,Cleared,Discovery,Load};
    if(!CheckSites()) {REX::WARN("CME entries changed during preparation; selectors disabled");return false;}
    auto& trampoline=REL::GetTrampoline();
    for(std::size_t i=0;i<sites.size();++i) original[i]=reinterpret_cast<GetDefault>(trampoline.write_call<5>(sites[i],hooks[i]));
    REX::INFO("CME-derived selector installed: {} authored rules, six original entry points",rules.size());return true;
}
}
