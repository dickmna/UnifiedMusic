#include "PCH.h"
#include "Module.h"
#include "hooks/hooks.h"
#include "settings/JSONSettings.h"

namespace CME {
namespace {
ReplacementGuard replacementGuard = nullptr;
bool dataLoaded = false;
}
void SetReplacementGuard(ReplacementGuard guard) { replacementGuard = guard; }
bool AllowsReplacement(RE::BGSMusicType* proposed, bool combat)
{
    return proposed && (!replacementGuard || replacementGuard(proposed, combat));
}
bool InstallHooks() { return Hooks::Install(); }
void OnDataLoaded()
{
    if (dataLoaded) return;
    dataLoaded = true;
    JSONSettings::Read();
    REX::INFO("CME embedded: forced silence and ActorUpdate hooks are disabled.");
}
void ResetOwnedMusicState()
{
    Hooks::CombatMusicCalls::GetSingleton()->SetCurrentCombatMusic(nullptr);
}
}
