#pragma once

namespace UnifiedMusic {
struct MergePlan;

// Restores verified contributions after resolving all tracks and the engine's
// default battle type. Original wrappers and native conditions remain intact.
bool ConfigureAutoCombat(const MergePlan& plan);
}
