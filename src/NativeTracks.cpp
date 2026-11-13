#include "NativeTracks.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

namespace UnifiedMusic {
namespace {
// BSTDerivedCreator has no instance data beyond its vptr (IBSTCreator, 8 bytes).
// Dispatch through the game's existing stateless factory; this invokes its
// constructor instead of manufacturing an object with memcpy/vtable patching.
struct Factory { std::uintptr_t table; };
const Factory& SingleFactory() {
    static const Factory factory{REL::Relocation<std::uintptr_t>{RE::VTABLE_BSTDerivedCreator_BGSMusicSingleTrack_BSIMusicTrack_[0]}.address()};
    return factory;
}
template<class Function> Function Method(std::size_t slot) {
    return reinterpret_cast<Function>(reinterpret_cast<const std::uintptr_t*>(SingleFactory().table)[slot]);
}
bool Nodes(const RE::TESCondition* condition,std::set<const RE::TESConditionItem*>& nodes) {
    for(auto* node=condition?condition->head:nullptr;node;node=node->next)
        if(nodes.size()>=4096 || !nodes.insert(node).second) return false;
    return true;
}
}
const RE::TESCondition* TrackConditions(const RE::BGSMusicTrack* track) {
    RE::TESCondition* value{};
    if(track) std::memcpy(&value,&track->conditions,sizeof(value));
    return value;
}
RE::TESCondition* TrackConditions(RE::BGSMusicTrack* track) {
    return const_cast<RE::TESCondition*>(TrackConditions(static_cast<const RE::BGSMusicTrack*>(track)));
}
void SetTrackConditions(RE::BGSMusicTrack* track,RE::TESCondition* conditions) {
    std::memcpy(&track->conditions,&conditions,sizeof(conditions));
}
void SingleDeleter::operator()(RE::BGSMusicSingleTrack* track) const {
    if(track) Method<void(*)(const Factory*,const RE::BSIMusicTrack*)>(2)(&SingleFactory(),track);
}
PrivateSingle CloneSingle(const RE::BGSMusicTrack* source) {
    if(!source || source->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack ||
       source->GetMusicStatus()!=RE::BSIMusicTrack::MUSIC_STATUS::kInactive) return {};
    std::set<const RE::TESConditionItem*> original;
    const auto* sourceConditions=TrackConditions(source);
    if(!Nodes(sourceConditions,original)) return {};
    const auto* single=static_cast<const RE::BGSMusicSingleTrack*>(source);
    if(single->cuePoints.size()>4096 || std::ranges::any_of(single->cuePoints,[](float x){return !std::isfinite(x)||x<0;})) return {};
    if(single->loopData && (!std::isfinite(single->loopData->loopBegin)||!std::isfinite(single->loopData->loopEnd)||
        single->loopData->loopBegin<0 || single->loopData->loopEnd<single->loopData->loopBegin)) return {};
    auto* created=Method<RE::BSIMusicTrack*(*)(const Factory*)>(1)(&SingleFactory());
    PrivateSingle result(static_cast<RE::BGSMusicSingleTrack*>(created));
    if(!result || result->GetType()!=RE::BSIMusicTrack::TrackType::kSingleTrack ||
        result->GetMusicStatus()!=RE::BSIMusicTrack::MUSIC_STATUS::kInactive || TrackConditions(result.get()) || result->loopData ||
        !result->cuePoints.empty() || result->trackHandle.soundID!=RE::BSSoundHandle::kInvalidID ||
        result->finaleHandle.soundID!=RE::BSSoundHandle::kInvalidID) return {};
    result->trackID=single->trackID;result->finaleID=single->finaleID;
    result->cuePoints.reserve(single->cuePoints.size());
    for(float point:single->cuePoints) result->cuePoints.push_back(point);
    if(single->loopData) {
        result->loopData=static_cast<RE::BGSMusicSingleTrack::LoopData*>(RE::malloc(sizeof(*result->loopData)));
        if(!result->loopData) return {};
        *result->loopData=*single->loopData;
    }
    if(sourceConditions) {
        auto* memory=RE::malloc(sizeof(RE::TESCondition));
        if(!memory) return {};
        auto* condition=std::construct_at(static_cast<RE::TESCondition*>(memory));
        SetTrackConditions(result.get(),condition);
        condition->Copy(sourceConditions);
    }
    std::set<const RE::TESConditionItem*> copied;
    if(!Nodes(TrackConditions(result.get()),copied) || copied.size()!=original.size() ||
        std::ranges::any_of(copied,[&](auto* node){return original.contains(node);})) {
        // A broken native copy must never destroy an original chain.
        if(auto* condition=TrackConditions(result.get())) condition->head=nullptr;
        return {};
    }
    return result;
}
}
