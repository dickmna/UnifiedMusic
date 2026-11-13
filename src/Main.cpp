#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REX/REX.h>
#include <REX/W32/KERNEL32.h>
#include <filesystem>
#include "core/PluginScan.h"
#include "AutoCombat.h"
#include "AreaMusic.h"

namespace {
void Distribute() {
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return;
    // One owner for MTD configuration. Its listener order is unspecified;
    // snapshot checks alone cannot stop a later MTD listener appending twice.
    if(SKSE::GetPluginInfo("MusicTypeDistributor") || REX::W32::GetModuleHandleW(L"MusicTypeDistributor.dll")) {
        REX::WARN("MTD is loaded: Harmonia automatic writes paused to avoid competing distribution. Use one distributor.");
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
    const auto plan=scan.Plan();
    for(const auto& entry:plan.diagnostics) REX::INFO("{}",entry);
    if(UnifiedMusic::ApplyMusicPlan(plan,scan.Records())) UnifiedMusic::RestoreAreaMusic(plan);
    REX::INFO("Automatic planning complete: {} allowed ambient types, {} configured types",plan.allowed.size(),plan.distributions.size());
}
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if(message->type==SKSE::MessagingInterface::kDataLoaded) {
        try { Distribute(); }
        catch(const std::exception& e) {
            // Parsing finishes before any automatic mutations; an unsupported
            // plugin disables automatic merging rather than guessing records.
            REX::ERROR("Automatic music merge disabled: {}",e.what());
        }
    }
}
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[] {
    SKSE::PluginVersionData version{};
    version.PluginVersion(REL::Version(0,6,0,0));
    version.PluginName("Harmonia");
    version.AuthorName("UnifiedMusic contributors");
    version.UsesAddressLibrary(); version.UsesUpdatedStructs();
    return version;
}();
SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    const auto runtime=skse->RuntimeVersion();
    if(runtime!=SKSE::RUNTIME_SSE_1_7_99 && runtime!=SKSE::RUNTIME_SSE_1_7_104) return false;
    SKSE::InitInfo info; info.log=true; info.hook=false; info.trampoline=false;
    SKSE::Init(skse,info);
    auto* messaging=SKSE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnMessage);
}
