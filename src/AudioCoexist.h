#pragma once
#include "NativeTracks.h"
#include "core/AudioAssets.h"

namespace UnifiedMusic {
// Resource adapter only. It does not append to any MusicType or install hooks.
class AudioInputs {
public:
    explicit AudioInputs(bool needed);
    PrivateSingle Original(const AudioCandidate& candidate,const RE::BGSMusicTrack* effectiveUse);
    bool SceneResourcesMatch(const MergePlan& plan) const;
private:
    OriginalAudio original_;
    bool ready_{};
};
}
