#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REX/REX.h>
#include <algorithm>
#include <filesystem>
#include <set>
#include "core/PluginScan.h"
#include "AutoCombat.h"
#include "AreaMusic.h"

namespace {
bool PlaylistMatches(RE::BGSMusicType* type,const std::vector<UnifiedMusic::MusicRecord>& versions,
                     RE::TESDataHandler* data) {
    if(!type || !type->GetFile()) return false;
    std::string source(type->GetFile()->GetFilename());
    for(auto& c:source) if(c>='A' && c<='Z') c+=32;
    const UnifiedMusic::MusicRecord* snapshot=nullptr;
    for(const auto& version:versions) if(version.source==source) {
        if(snapshot) return false;
        snapshot=&version;
    }
    if(!snapshot || snapshot->tracks.size()!=type->tracks.size()) return false;
    std::multiset<RE::BSIMusicTrack*> expected;
    for(const auto& key:snapshot->tracks) {
        auto* track=data->LookupForm<RE::BGSMusicTrackFormWrapper>(key.id,key.plugin);
        if(!track) return false;
        expected.insert(track);
    }
    return expected==std::multiset<RE::BSIMusicTrack*>(type->tracks.begin(),type->tracks.end());
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
    for(const auto& entry:plan.diagnostics) REX::INFO("{}",entry);
    std::size_t appended=0;
    for(const auto& [key,tracks]:plan.additions) {
        if(tracks.empty()) continue;
        auto* type=data->LookupForm<RE::BGSMusicType>(key.id,key.plugin);
        if(!PlaylistMatches(type,scan.Records().at(key),data)) {
            REX::WARN("Preserve changed native playlist {}",UnifiedMusic::ToString(key)); continue;
        }
        for(const auto& track:tracks) {
            auto* form=data->LookupForm<RE::BGSMusicTrackFormWrapper>(track.id,track.plugin);
            if(!form || !form->track) { REX::WARN("Missing/unusable track {}",UnifiedMusic::ToString(track)); continue; }
            auto* music=static_cast<RE::BSIMusicTrack*>(form);
            if(std::find(type->tracks.begin(),type->tracks.end(),music)==type->tracks.end()) {
                type->tracks.push_back(music); ++appended;
                REX::INFO("Append {} -> {}",UnifiedMusic::ToString(track),UnifiedMusic::ToString(key));
            }
        }
    }
    if(plan.battleType && (!plan.combatRules.empty() || !plan.conditionRepairs.empty())) {
        auto* battle=data->LookupForm<RE::BGSMusicType>(plan.battleType->id,plan.battleType->plugin);
        if(!PlaylistMatches(battle,scan.Records().at(*plan.battleType),data) || !UnifiedMusic::ConfigureAutoCombat(plan))
            REX::WARN("Automatic combat batch skipped; native battle playlist preserved");
    }
    UnifiedMusic::RestoreAreaMusic(plan);
    REX::INFO("Automatic merge complete: {} allowed ambient types, {} restored additions",plan.allowed.size(),appended);
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
    version.PluginVersion(REL::Version(0,5,0,0));
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
