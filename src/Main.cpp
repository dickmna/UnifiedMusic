#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REX/REX.h>
#include <algorithm>
#include <filesystem>
#include <set>
#include "core/PluginScan.h"
#include "mtd/Integration.h"
#include "cme/Module.h"

namespace {
std::set<RE::FormID> appendAllowed;
std::vector<RE::BGSMusicType*> scriptBoundTypes;
bool analysisReady=false;
bool AllowReplacement(RE::BGSMusicType* proposed, bool) {
    if(!analysisReady) return false;
    for(auto* type:scriptBoundTypes) {
        if(type==proposed) return false;
        auto status=type->typeStatus.underlying();
        if(status==static_cast<std::uint32_t>(RE::BSIMusicType::MUSIC_STATUS::kPlaying) ||
           status==static_cast<std::uint32_t>(RE::BSIMusicType::MUSIC_STATUS::kFinishing)) return false;
    }
    return true;
}
bool AllowAppend(const RE::BGSMusicType* type) noexcept {
    return type && appendAllowed.contains(type->GetFormID());
}
void Distribute() {
    auto* data=RE::TESDataHandler::GetSingleton();
    if(!data) return;
    UnifiedMusic::PluginScan scan;
    // The plan compares origin and explicit dependencies without assuming this
    // list follows override order. Disabled files do not contribute.
    for(auto* file:data->files) {
        if(!file) continue;
        const auto name=file->GetFilename();
        if(!data->LookupLoadedModByName(name) && !data->LookupLoadedLightModByName(name)) continue;
        scan.Read(std::filesystem::path("Data") / name);
    }
    const auto plan=scan.Plan();
    for(const auto& key:plan.scriptBound) {
        if(auto* type=data->LookupForm<RE::BGSMusicType>(key.id,key.plugin)) scriptBoundTypes.push_back(type);
    }
    analysisReady=true;
    for(const auto& entry:plan.diagnostics) REX::INFO("{}",entry);
    for(const auto& key:plan.allowed) {
        if(auto* type=data->LookupForm<RE::BGSMusicType>(key.id,key.plugin)) appendAllowed.insert(type->GetFormID());
    }
    std::size_t appended=0;
    for(const auto& [key,tracks]:plan.additions) {
        auto* type=data->LookupForm<RE::BGSMusicType>(key.id,key.plugin);
        if(!AllowAppend(type)) continue;
        for(const auto& track:tracks) {
            auto* form=data->LookupForm<RE::BGSMusicTrackFormWrapper>(track.id,track.plugin);
            if(!form) { REX::WARN("Missing track {}",UnifiedMusic::ToString(track)); continue; }
            auto* music=static_cast<RE::BSIMusicTrack*>(form);
            if(std::find(type->tracks.begin(),type->tracks.end(),music)==type->tracks.end()) {
                type->tracks.push_back(music); ++appended;
                REX::INFO("Append {} -> {}",UnifiedMusic::ToString(track),UnifiedMusic::ToString(key));
            }
        }
    }
    MTD::SetTypePolicy(AllowAppend);
    if(MTD::PrepareDistribution()) MTD::ApplyDistribution();
    REX::INFO("Automatic merge complete: {} allowed ambient types, {} restored additions",appendAllowed.size(),appended);
}
void OnMessage(SKSE::MessagingInterface::Message* message) {
    if(message->type==SKSE::MessagingInterface::kDataLoaded) {
        try { Distribute(); }
        catch(const std::exception& e) {
            // Parsing finishes before any automatic mutations; an unsupported
            // plugin disables automatic merging rather than guessing records.
            REX::ERROR("Automatic music merge disabled: {}",e.what());
        }
        CME::OnDataLoaded();
    } else if(message->type==SKSE::MessagingInterface::kNewGame || message->type==SKSE::MessagingInterface::kPreLoadGame) {
        CME::ResetOwnedMusicState();
    }
}
}
extern "C" __declspec(dllexport) constinit auto SKSEPlugin_Version=[] {
    SKSE::PluginVersionData version{};
    version.PluginVersion(REL::Version(0,1,0,0));
    version.PluginName("UnifiedMusic");
    version.AuthorName("UnifiedMusic contributors; MTD and CME upstream authors");
    version.UsesAddressLibrary(); version.UsesUpdatedStructs();
    return version;
}();
SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    const auto runtime=skse->RuntimeVersion();
    if(runtime!=SKSE::RUNTIME_SSE_1_7_99 && runtime!=SKSE::RUNTIME_SSE_1_7_104) return false;
    // Double-hooking the standalone distributors is unsupported.
    if(std::filesystem::exists("Data/SKSE/Plugins/CombatMusic.dll") ||
       std::filesystem::exists("Data/SKSE/Plugins/MusicTypeDistributor.dll")) return false;
    SKSE::InitInfo info; info.log=true; info.hook=true; info.trampoline=true; info.trampolineSize=128;
    SKSE::Init(skse,info);
    CME::SetReplacementGuard(AllowReplacement);
    if(!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) return false;
    return CME::InstallHooks();
}
