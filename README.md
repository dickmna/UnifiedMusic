# UnifiedMusic

An experimental, conservative music compatibility plugin for **Skyrim Steam 1.7.99 / 1.7.104**. One SKSE DLL combines a small MTD-derived INI adapter, CME's conditional default-music selection, and an independent plugin-record scanner. No music, paid Creation assets, ESP copies, or Papyrus replacements are distributed.

**Status: 0.1.0 alpha. Native build and offline tests are reported in VALIDATION.md. In-game playback, quest progression, and save/reload behavior have not been tested. This is not a claim of universal compatibility.**

## What is automatic

At `kDataLoaded`, the plugin reads the loaded ESP/ESM/ESL files through the game's visible Data directory. It resolves their own master tables and reconstructs the contributions to original exploration, town, dungeon and tavern music lists. Independent added tracks that lost a load-order conflict are appended once. Track conditions and final winning track forms are retained.

No third-party mod-name list is used. The same mechanism can recognize music packs that use the usual additive playlist records, including future packs. Separate music types for new lands remain separate. Vanilla audio replacers such as Lorkhan continue to use the original file-resolution mechanism; UnifiedMusic does not copy or rename audio. Chapter II has not been installed in the validation environment, so specific releases are not yet verified.

## Preservation rules

- Custom music types, combat/Boss/discovery/stinger/silence types, and one-shot types are not automatically merged.
- Changed priority/fade/flags, removed base tracks, deleted records, and explicit dependent patches removing earlier music additions cause the entire target type to be preserved.
- Any MusicType found in VMAD script-property bytes is protected. This conservative reference scan can overprotect; it does not decompile Papyrus and cannot discover all dynamically constructed lookups.
- No CELL, worldspace, region, or location music assignment is rewritten. Partial Form placeholders are not treated as deletions.
- CME cannot claim a script-bound MusicType as its own replacement. Active protected music also suppresses replacement. Actor-update silencing and forced `DoFinish` cleanup are omitted.
- The mod does not repair two authors' incompatible conditions, scripts, same-path audio replacements, or intentionally conflicting design choices. Such cases require authored rules or a patch.

## Existing configurations

`Data/*_MUS.ini` `[General]` append rules are accepted through the compact MTD adapter. Track identifiers retain `0xFormID~Plugin.esp` syntax. Empty-list (`!`) directives, new runtime music-type creation, location and region reassignment are deliberately unavailable. A protected target is skipped and logged. This is a supported subset of MTD, not a drop-in replacement for every MTD conversion.

Existing `Data/SKSE/Plugins/CombatMusic/*.json` rules use CME's format. Rules only select a replacement when the engine requests default battle or dungeon-cleared music; the protection guard can decline them. No universal geographical rules are guessed for custom quest/Boss content.

## Installation

For experimental testing, install the packaged `SKSE/Plugins/UnifiedMusic.dll` using a mod manager. Use SKSE and Address Library matching **Steam 1.7.99 or 1.7.104**. Other runtimes are refused, including untested future releases. Keep your music packs and their audio installed.

Disable standalone `MusicTypeDistributor.dll` and `CombatMusic.dll` before testing this combined plugin. Their INI/JSON configurations may remain, subject to the subset above. UnifiedMusic refuses to load when either original DLL is present to prevent double distribution/hooks. A successful compile does not establish runtime safety; test with a separate profile and disposable save first.

## Build

Requires Visual Studio 2022 C++ Build Tools, a Windows SDK, Git and CMake 3.24+. Dependencies are fetched from pinned commits in `src/cme/Dependencies.cmake`.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect --parallel 4
python tests/test_plugin_scan.py --inspect build/Release/UnifiedMusicInspect.exe -v
```

The optional `UNIFIED_MUSIC_DEPS_DIR` CMake setting reuses local dependency Git checkouts after checking their exact revisions. No game path is compiled into the DLL.

`UnifiedMusicInspect.exe plugin1.esm plugin2.esp ...` runs the same scanner offline, in the order supplied. It emits `ALLOW`, `APPEND`, and `PRESERVE` diagnostics. Numeric local IDs in this output are decimal.

## Attribution and licensing

- MTD: ThirdEyeSqueegee, public `stop-sine/MusicTypeDistributor-RE`, commit `9a4cdf112c64bff91bec8f834c62bd00c46d2fe1`; GPL-3.0-or-later plus its original exceptions. The integration is reduced to append-only parsing and distribution.
- CME: SeaSparrowOG/CombatMusic, commit `302c6df30348ce83cfcf72cbf6ae4e8681fcc7b6`; AGPL-3.0. The checked-out LICENSE governs this code, rather than conflicting website summaries.
- The new scanner/integration code is AGPL-3.0-or-later. MTD-derived parts retain their GPL terms and exceptions under the GPL/AGPL combined-work provisions. See the original license texts in `vendor/MTD` and `vendor/CME`.
- CommonLibSSE, commonlib-shared, spdlog, JsonCpp, SimpleIni and zlib retain their upstream licenses. Pinned build sources provide these notices; binary packages include them.

No external service is used at runtime. This repository does not contain the local paid-mod audit data or music assets.
