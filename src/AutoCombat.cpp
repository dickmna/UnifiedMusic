#include "AutoCombat.h"
#include "core/PluginScan.h"
#include <RE/Skyrim.h>
#include <REX/REX.h>
#include <algorithm>
#include <vector>

namespace UnifiedMusic {
bool ConfigureAutoCombat(const MergePlan& plan)
{
    if (plan.combatRules.empty()) return true;
    auto* data=RE::TESDataHandler::GetSingleton();
    auto* objects=RE::BGSDefaultObjectManager::GetSingleton();
    if (!data || !objects || !plan.battleType) return false;
    auto* target=data->LookupForm<RE::BGSMusicType>(plan.battleType->id,plan.battleType->plugin);
    auto* native=objects->GetObject<RE::BGSMusicType>(RE::BGSDefaultObjectManager::DefaultObject::kBattleMusic);
    if (!target || target!=native) {
        REX::WARN("Auto combat disabled: original battle type differs from the engine default");
        return false;
    }

    // Resolve every contribution before appending any. Reuse the original
    // wrappers so all author conditions remain under the game's control.
    std::vector<RE::BGSMusicTrackFormWrapper*> pending;
    for (const auto& rule:plan.combatRules) {
        for (const auto& key:rule.tracks) {
            auto* track=data->LookupForm<RE::BGSMusicTrackFormWrapper>(key.id,key.plugin);
            if (!track || !track->track || track->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack) {
                REX::WARN("Auto combat disabled: unresolved/non-single track {}",ToString(key));
                return false;
            }
            if (std::ranges::find(pending,track)==pending.end()) pending.push_back(track);
        }
    }
    std::size_t restored=0;
    for (auto* track:pending) {
        if (std::ranges::find(target->tracks,track)==target->tracks.end()) {
            target->tracks.push_back(track);
            ++restored;
        }
    }
    REX::INFO("Auto combat: {} verified contributions, {} restored tracks; native conditions preserved",pending.size(),restored);
    return true;
}
}
