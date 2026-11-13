#pragma once

namespace RE { class BGSMusicType; }
namespace MTD {
// The caller may substitute an audited allowlist derived from original records.
// nullptr restores the conservative default; passing a policy does not enable clear.
using TypePolicy = bool (*)(const RE::BGSMusicType*);
void SetTypePolicy(TypePolicy policy) noexcept;
bool CanAppendTo(const RE::BGSMusicType* type) noexcept;
bool PrepareDistribution() noexcept;
void ApplyDistribution() noexcept;
bool RunDistribution() noexcept;
}
