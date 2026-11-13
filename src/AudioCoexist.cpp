#include "AudioCoexist.h"
#include "NativeTracks.h"
#include "core/AudioAssets.h"
#include "core/PluginScan.h"
#include <REX/REX.h>
#include <algorithm>
#include <cstring>
#include <limits>

namespace UnifiedMusic {
namespace {
std::optional<AudioBytes> Resource(const std::string& name) {
    RE::BSResourceNiBinaryStream stream(name);
    if(!stream.good() || !stream.stream || !stream.stream->totalSize || stream.stream->totalSize>MaxAudioBytes) return {};
    AudioBytes bytes(stream.stream->totalSize);
    if(!stream.read(bytes.data(),static_cast<std::uint32_t>(bytes.size()))) return {};
    return bytes;
}
std::optional<AudioBytes> WinningAudio(const std::string& name) {
    // A simultaneously present WAV has audio-engine precedence outside this
    // reader's contract. Preserve that case instead of comparing the wrong file.
    auto wav=name;wav.replace(wav.size()-4,4,".wav");
    if(Resource(wav)) return {};
    return Resource(name);
}
bool SameID(const RE::BSResource::ID& a,const RE::BSResource::ID& b) {return std::memcmp(&a,&b,sizeof(a))==0;}
bool PathMatches(const RE::BSResource::ID& id,const std::string& path) {
    if(path.empty()) return SameID(id,RE::BSResource::ID{});
    const auto normalized=MusicPath(path);if(!normalized) return false;
    auto name=*normalized;std::replace(name.begin(),name.end(),'/','\\');
    RE::BSResource::ID expected;expected.GenerateFromPath(name.c_str());
    if(SameID(id,expected)) return true;
    name.replace(name.size()-4,4,".wav");expected.GenerateFromPath(name.c_str());
    return SameID(id,expected);
}
}
void ApplyAudioCoexist(const MergePlan& plan) {
    if(plan.audioCandidates.empty()) return;
    OriginalAudio original;
    // Only official archives are original-audio witnesses. Never select an
    // arbitrary losing mod archive by filename or mod-author heuristics.
    for(const auto* name:{"Skyrim - Sounds.bsa","Dawnguard.bsa","HearthFires.bsa","Dragonborn.bsa"}) {
        const auto path=std::filesystem::path("Data")/name;
        if(std::filesystem::exists(path)) original.AddArchive(path);
    }
    auto* data=RE::TESDataHandler::GetSingleton();if(!data) return;
    struct List { RE::BGSMusicType* type; std::vector<RE::BSIMusicTrack*> snapshot; std::vector<PrivateSingle> copies; };
    std::map<FormKey,List> lists;
    std::size_t unchanged=0,skipped=0;
    for(const auto& candidate:plan.audioCandidates) {
        const auto skip=[&](const std::string& reason) {++skipped;REX::INFO("AUDIO_SKIP {} in {}: {}",ToString(candidate.track),ToString(candidate.musicType),reason);};
        try {
            auto* type=data->LookupForm<RE::BGSMusicType>(candidate.musicType.id,candidate.musicType.plugin);
            auto* wrapper=data->LookupForm<RE::BGSMusicTrackFormWrapper>(candidate.track.id,candidate.track.plugin);
            if(!type || !wrapper || !wrapper->track || wrapper->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack ||
                std::ranges::find(type->tracks,static_cast<RE::BSIMusicTrack*>(wrapper))==type->tracks.end()) {skip("source no longer in ordinary list");continue;}
            const auto* single=static_cast<const RE::BGSMusicSingleTrack*>(wrapper->track);
            if(!PathMatches(single->trackID,candidate.audioPath) || !PathMatches(single->finaleID,candidate.finalePath)) {skip("runtime audio path differs");continue;}
            std::string reason;
            auto pair=CompareAudio(original,WinningAudio,candidate.audioPath,candidate.finalePath,reason);
            if(!pair) {if(reason=="unchanged audio") ++unchanged;else skip(reason);continue;}
            auto copy=CloneSingle(single);if(!copy) {skip("native single track could not be isolated");continue;}
            auto main=CacheAudio("Data",pair->track);
            auto finale=pair->finale.empty()?std::string{}:CacheAudio("Data",pair->finale);
            if(Resource(main)!=std::optional<AudioBytes>(pair->track) ||
                (!finale.empty() && Resource(finale)!=std::optional<AudioBytes>(pair->finale))) {skip("cache is not visible to game resources");continue;}
            std::replace(main.begin(),main.end(),'/','\\');
            std::replace(finale.begin(),finale.end(),'/','\\');
            copy->trackID.GenerateFromPath(main.c_str());
            if(!finale.empty()) copy->finaleID.GenerateFromPath(finale.c_str());
            auto [it,added]=lists.try_emplace(candidate.musicType,List{type,{type->tracks.begin(),type->tracks.end()},{}});
            it->second.copies.push_back(std::move(copy));
            REX::INFO("AUDIO_PREPARED {} in {} original SHA256 {}",ToString(candidate.track),ToString(candidate.musicType),pair->trackHash);
        } catch(const std::exception& e) {skip(e.what());}
    }
    for(auto& [key,list]:lists) {
        if(list.copies.size()>std::numeric_limits<std::uint32_t>::max()-list.type->tracks.size()) throw std::runtime_error("audio playlist capacity exceeded");
        list.type->tracks.reserve(static_cast<std::uint32_t>(list.type->tracks.size()+list.copies.size()));
    }
    for(const auto& [key,list]:lists) if(!std::ranges::equal(list.snapshot,list.type->tracks)) {
        REX::WARN("Original audio batch skipped: a playlist changed during preparation");return;
    }
    std::size_t count=0;
    for(auto& [key,list]:lists) for(auto& copy:list.copies) {list.type->tracks.push_back(copy.release());++count;}
    REX::INFO("Original audio coexistence: {} independent instances, {} unchanged candidates, {} skips",count,unchanged,skipped);
}
}
