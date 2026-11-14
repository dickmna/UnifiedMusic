#include "AudioCoexist.h"
#include <REX/REX.h>
#include <algorithm>
#include <cstring>

namespace UnifiedMusic {
namespace {
std::optional<AudioBytes> Resource(const std::string& name) {
    RE::BSResourceNiBinaryStream stream(name.c_str());
    if(!stream.good() || !stream.stream || !stream.stream->totalSize || stream.stream->totalSize>MaxAudioBytes) return {};
    AudioBytes bytes(stream.stream->totalSize);
    if(!stream.read(bytes.data(),static_cast<std::uint32_t>(bytes.size()))) return {};
    return bytes;
}
std::optional<AudioBytes> WinningAudio(const std::string& name) {
    auto wav=name;wav.replace(wav.size()-4,4,".wav");
    if(Resource(wav)) return {};
    return Resource(name);
}
bool PathMatches(const RE::BSResource::ID& id,const std::string& path) {
    if(path.empty()) {const RE::BSResource::ID empty{};return std::memcmp(&id,&empty,sizeof(id))==0;}
    const auto normalized=MusicPath(path);if(!normalized) return false;
    auto name=*normalized;std::replace(name.begin(),name.end(),'/','\\');
    RE::BSResource::ID expected;expected.GenerateFromPath(name.c_str());
    if(std::memcmp(&id,&expected,sizeof(id))==0) return true;
    name.replace(name.size()-4,4,".wav");expected.GenerateFromPath(name.c_str());
    return std::memcmp(&id,&expected,sizeof(id))==0;
}
}
AudioInputs::AudioInputs(bool needed) {
    if(!needed) return;
    try {
        for(const auto* name:{"Skyrim - Sounds.bsa","Dawnguard.bsa","HearthFires.bsa","Dragonborn.bsa"}) {
            const auto path=std::filesystem::path("Data")/name;
            if(std::filesystem::exists(path)) original_.AddArchive(path);
        }
        ready_=true;
    } catch(const std::exception& e) {REX::WARN("Original audio input unavailable: {}",e.what());}
}
PrivateSingle AudioInputs::Original(const AudioCandidate& candidate,const RE::BGSMusicTrack* effectiveUse) {
    if(!ready_) return {};
    try {
        if(!effectiveUse || effectiveUse->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack) return {};
        const auto* single=static_cast<const RE::BGSMusicSingleTrack*>(effectiveUse);
        if(!PathMatches(single->trackID,candidate.audioPath) || !PathMatches(single->finaleID,candidate.finalePath)) {
            REX::WARN("AUDIO_SKIP {} runtime audio path differs",ToString(candidate.track));return {};
        }
        std::string reason;
        const auto pair=CompareAudio(original_,WinningAudio,candidate.audioPath,candidate.finalePath,reason);
        if(!pair) {
            if(reason!="unchanged audio") REX::INFO("AUDIO_SKIP {} {}",ToString(candidate.track),reason);
            return {};
        }
        // Both versions inherit the effective use before its single commit.
        auto copy=CloneSingle(single);if(!copy) return {};
        auto main=CacheAudio("Data",pair->track);
        auto finale=pair->finale.empty()?std::string{}:CacheAudio("Data",pair->finale);
        if(Resource(main)!=std::optional<AudioBytes>(pair->track) ||
           (!finale.empty() && Resource(finale)!=std::optional<AudioBytes>(pair->finale))) return {};
        std::replace(main.begin(),main.end(),'/','\\');std::replace(finale.begin(),finale.end(),'/','\\');
        copy->trackID.GenerateFromPath(main.c_str());
        if(!finale.empty()) copy->finaleID.GenerateFromPath(finale.c_str());
        REX::INFO("AUDIO_NORMALIZED {} in {} original SHA256 {}",ToString(candidate.track),ToString(candidate.musicType),pair->trackHash);
        return copy;
    } catch(const std::exception& e) {REX::WARN("AUDIO_SKIP {} {}",ToString(candidate.track),e.what());return {};}
}
bool AudioInputs::SceneResourcesMatch(const MergePlan& plan) const {
    auto* data=RE::TESDataHandler::GetSingleton();if(!data) return false;
    for(const auto& resource:plan.sceneResources) {
        auto* wrapper=data->LookupForm<RE::BGSMusicTrackFormWrapper>(resource.track.id,resource.track.plugin);
        if(!wrapper || !wrapper->track || wrapper->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack) return false;
        const auto* single=static_cast<const RE::BGSMusicSingleTrack*>(wrapper->track);
        if(!PathMatches(single->trackID,resource.audioPath) || !PathMatches(single->finaleID,resource.finalePath)) return false;
    }
    const auto failures=CheckSceneResources(plan.sceneResources,WinningAudio);
    for(const auto& [type,reason]:failures) REX::WARN("SCENE_RESOURCE_SKIP {} {}",ToString(type),reason);
    return failures.empty();
}
}
