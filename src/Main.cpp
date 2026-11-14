#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REX/REX.h>
#include <REX/W32/KERNEL32.h>
#include <filesystem>
#include "core/PluginScan.h"
#include "AutoCombat.h"
#include "AreaMusic.h"
#include "CombatSelector.h"

namespace {
void Distribute() {
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return;
    // One owner for MTD configuration. Its listener order is unspecified;
    // snapshot checks alone cannot stop a later MTD listener appending twice.
    if(SKSE::GetPluginInfo("MusicTypeDistributor") || REX::W32::GetModuleHandleW(L"MusicTypeDistributor.dll") ||
       REX::W32::GetModuleHandleW(L"CombatMusic.dll")) {
        REX::WARN("Standalone MTD/CME is loaded: Harmonia writes paused. Use one execution owner.");
        return;
    }
    UnifiedMusic::PluginScan scan;
    // The plan compares origin and explicit dependencies without assuming this
    // list follows override order. Disabled files do not contribute.
    for(auto* file:data->files) {
        if(!file) continue;
        const auto name=file->GetFilename();
        if(!data->LookupLoadedModByName(name) && !data->LookupLoadedLightModByName(name)) continue;
        scan.Read(std::filesystem::path("Data") / name);
    }
    scan.ReadConfigs("Data");
    scan.ReadSelectors("Data/SKSE/Plugins/CombatMusic");
    const auto plan=scan.Plan();
    for(const auto& entry:plan.diagnostics) REX::INFO("{}",entry);
    if(!UnifiedMusic::PrepareSelectors(plan)) {REX::WARN("Unresolved native selector forms; music batch skipped");return;}
    std::set<UnifiedMusic::FormKey> failed;
    const bool complete=UnifiedMusic::ApplyMusicPlan(plan,scan.Records(),scan.Tracks(),failed);
    UnifiedMusic::RestoreAreaMusic(plan,failed,complete);
    UnifiedMusic::InstallSelectors();
    REX::INFO("Automatic planning complete: {} allowed ambient types, {} configured types",plan.allowed.size(),plan.distributions.size());
}
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if(message->type==SKSE::MessagingInterface::kDataLoaded) {
        static bool applied=false;if(std::exchange(applied,true)) return;
        try { Distribute(); }
        catch(const std::exception& e) {
            // Parsing/preparation precedes the first commit. A later exception
            // must not imply previously committed runtime actions were undone.
            REX::ERROR("Music processing stopped: {}; earlier completed stages, if any, remain",e.what());
        }
    }
}
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[] {
    SKSE::PluginVersionData version{};
    version.PluginVersion(REL::Version(1,0,0,0));
    version.PluginName("Harmonia");
    version.AuthorName("UnifiedMusic contributors");
    version.UsesAddressLibrary(); version.UsesUpdatedStructs();
    return version;
}();
SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    const auto runtime=skse->RuntimeVersion();
    // The condition-pointer ABI and native single factory were inspected on
    // 1.7.104. Older executable layouts need separate evidence before enabling.
    if(runtime!=SKSE::RUNTIME_SSE_1_7_104) return false;
    SKSE::InitInfo info; info.log=true; info.hook=true; info.trampoline=true;info.trampolineSize=84;
    SKSE::Init(skse,info);
    auto* messaging=SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
