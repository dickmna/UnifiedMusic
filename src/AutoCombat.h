#pragma once
#include "core/PluginScan.h"

namespace UnifiedMusic {
// Prepares one playlist/distribution/condition batch, checks native winning records
// and required replacement tracks, then appends contributions and AND exclusions.
// Existing wrappers and condition nodes are not replaced or evaluated.
bool ApplyMusicPlan(const MergePlan& plan,const std::map<FormKey,std::vector<MusicRecord>>& records,std::set<FormKey>& failed);
}
