#pragma once
#include <RE/Skyrim.h>
#include <memory>

namespace UnifiedMusic {
// Steam 1.7.104 stores TESCondition* at 0x10. The pinned SDK declares an
// inline TESCondition of the same size. Keep this ABI correction in one place.
RE::TESCondition* TrackConditions(RE::BGSMusicTrack* track);
const RE::TESCondition* TrackConditions(const RE::BGSMusicTrack* track);
void SetTrackConditions(RE::BGSMusicTrack* track,RE::TESCondition* conditions);
struct SingleDeleter {void operator()(RE::BGSMusicSingleTrack* track) const;};
using PrivateSingle=std::unique_ptr<RE::BGSMusicSingleTrack,SingleDeleter>;
// Fresh native factory instance; only configuration is copied. No TESForm is
// registered, no FormID allocated, no engine vtable or playback hook changed.
PrivateSingle CloneSingle(const RE::BGSMusicTrack* source);
}
