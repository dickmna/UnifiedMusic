CME embedded integration

Upstream: SeaSparrowOG/CombatMusic, commit
`302c6df30348ce83cfcf72cbf6ae4e8681fcc7b6`, upstream version 1.0.1.
The original-file SHA-256 manifest is in `vendor/CME/`; source is available at
the pinned upstream revision. The upstream LICENSE
is AGPL-3.0; its vcpkg metadata says GPL-3.0 and must not replace the license.

This adapted module has no SKSE export. The host exports and initializes SKSE
once, using the pinned Steam 1.7 CommonLibSSE fork. It calls `InstallHooks()`
after SKSE initialization with at least 84 trampoline bytes, then calls
`OnDataLoaded()` after the distributor has finished appending tracks.
Call `ResetOwnedMusicState()` for new-game/pre-load bookkeeping; it does not
stop music. Set the optional `ReplacementGuard` before hooks are active.
The guard may veto a proposed replacement based on protected music or context.

The module still reads existing `Data/SKSE/Plugins/CombatMusic/*.json`, with
upstream `combatMusic`, `isCombatMusic`, `newMusic`, and condition schemas.
Plugin-local form references retain `Plugin.esp|0x12345` syntax.
No generated world/Boss rules are guessed from names in this layer.

Changes from upstream:

- Namespaces are nested under CME; all separate SKSE entry points are omitted.
- The ActorUpdate hook, combat event sink, INI silence feature, and forced
  `DoFinish` are omitted. Author-controlled music is never forcibly ended here.
- Only the upstream default battle and dungeon-cleared query call sites are
  hooked. Unsupported runtimes fail without installing hooks. Accepted versions
  are Steam 1.7.99.0 and 1.7.104.0. All six call opcodes are checked before
  patching; this is not a substitute for in-game verification.
- Dungeon-cleared replacements no longer overwrite owned combat bookkeeping.
  Active battle ownership remains tracked until the native end query; repeated
  start queries cannot abandon a still-playing/paused/finishing replacement.
  Already-active candidates that the module did not start are never claimed,
  including music resolved dynamically by scripts without a VMAD property.
- Conditions have a virtual destructor and initialized booleans; player-null
  checks protect matching during loading transitions.
- Invalid JSON is rejected. Explicit empty condition lists and unknown rule
  fields are rejected rather than silently broadening a rule. Optional metadata
  keys must start with `_`. Oversized numeric FormIDs fail resolution.

Build wiring:

```
include(src/cme/Dependencies.cmake)
add_subdirectory(src/cme)
target_link_libraries(YourSingleDLL PRIVATE unified_music_cme)
```

Dependencies.cmake pins CommonLibSSE, commonlib-shared, spdlog, JsonCpp,
SimpleIni and zlib by commit. It provides their namespaced CMake targets.
`UNIFIED_MUSIC_DEPS_DIR` may point at existing Git checkouts; each HEAD is
checked against the pin. FetchContent downloads missing sources otherwise.
Do not install this module alongside the standalone CombatMusic DLL.

Validation completed: configured with CMake 4.3.3 / MSVC 19.44, compiled all
three adapted CME translation units as a static library. The validation did
not launch Skyrim or prove the runtime hook addresses or playback behavior.
