#pragma once
#include "NativeTracks.h"
#include "core/AudioAssets.h"

namespace UnifiedMusic {
// Resource adapter only. It does not append to any MusicType or install hooks.
class AudioInputs {
public:
    AudioInputs(bool needed,const std::map<FormKey,std::vector<TrackRecord>>& tracks);
    PrivateSingle Original(const AudioCandidate& candidate,const RE::BGSMusicTrack* effectiveUse,const std::vector<RE::BSIMusicTrack*>& present);
    bool SceneResourcesMatch(const MergePlan& plan) const;
    bool Enabled() const { return !pack_.Empty(); }
private:
    OriginalAudioPack pack_;
    std::map<const RE::BSIMusicTrack*,AudioPaths> paths_;
};
}
