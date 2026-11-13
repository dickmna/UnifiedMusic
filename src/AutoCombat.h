#pragma once

namespace UnifiedMusic {
struct MergePlan;

// Prepares a single battle-list/condition batch, checks native winning records
// and required replacement tracks, then appends contributions and AND exclusions.
// Existing wrappers and condition nodes are not replaced or evaluated.
bool ConfigureAutoCombat(const MergePlan& plan);
}
