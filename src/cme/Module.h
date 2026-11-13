#pragma once
#include <RE/Skyrim.h>

namespace CME {
// Only callback queries initiated for the default battle/cleared object reach
// this guard. true permits the replacement, false leaves the engine response.
// The callback must not throw and must remain valid throughout the plugin life.
using ReplacementGuard = bool (*)(RE::BGSMusicType* proposed, bool combat);
void SetReplacementGuard(ReplacementGuard guard);
bool AllowsReplacement(RE::BGSMusicType* proposed, bool combat);

// Call during SKSE load, after SKSE::Init with at least 84 trampoline bytes.
// Validates all call sites before installing any of the six hooks.
bool InstallHooks();
// Call once on kDataLoaded, after music distribution creates runtime forms.
void OnDataLoaded();
// Clear ownership bookkeeping on new-game/pre-load. Does not stop any track.
void ResetOwnedMusicState();
}
