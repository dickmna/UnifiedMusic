# UnifiedMusic

A conservative playlist-restoration plugin for **Skyrim Steam 1.7.99 / 1.7.104**. One SKSE DLL reconstructs eligible music contributions lost when plugins override the same original playlist.

**Status: 0.4.0 alpha. Builds and offline tests are recorded in VALIDATION.md. In-game playback, quests, transitions, and save/reload have not been tested.**

## Runtime

At SKSE `kDataLoaded`, UnifiedMusic reads loaded ESP/ESM/ESL files through the visible Data directory, resolves their master tables, and builds a restoration plan. It appends missing track references to eligible original music lists once, retaining the final native track forms. Version 0.4 also restores a restricted class of overwritten geographic exclusions on original battle tracks while retaining their existing condition nodes.

Since version 0.3, the project has removed the embedded MTD and CME implementations, INI/JSON parsing, playback selection hooks, and playback ownership state. SKSE hook and trampoline initialization are explicitly disabled. There are no forced play/stop calls, temporary MusicTypes, or polling loops. The runtime consists of the scanner, protection checks, and startup list/condition adapter; an offline inspector is optional.

No third-party mod-name list is used. Audio replacement packs retain the game's file-resolution behavior. No audio, game plugins, paid Creation assets, or Papyrus replacements are distributed.

## Restoration and protection

- Original exploration, town, dungeon and tavern types can receive independent additions lost to another override.
- Original `MUSCombat` can receive proven single-audio additions that are unconditional or have supported positive Cell/Worldspace/Location conditions on the player.
- Changed type settings, removed base tracks, deleted types, and explicit dependent patches removing earlier contributions protect the entire target list.
- Custom MusicTypes and all CELL/worldspace/location/region assignments remain untouched.
- Directly script-referenced tracks, palette subtracks, tracks shared with another custom/scripted/one-shot/nonambient type, and deleted or malformed tracks are excluded. Ordinary ambient types may share tracks; mapped ambient types additionally protect shared tracks against combat restoration.
- Ambient targets referenced by VMAD scripts are protected. The original global `MUSCombat` is a deliberate exception to whole-type script-reference protection: its own contributions may be restored even when a cleanup script refers to that type. Direct references to candidate tracks and sharing with other protected types still exclude those tracks.
- Partial Form and Ignored records do not contribute. The scanner completes structural parsing before any automatic list mutation.

Combat conditions use a small known schema: `GetInCell` (67), `GetInWorldspace` (310), `GetInCurrentLoc` (359), `GetQuestRunning` (56), `GetStage` (58), `GetStageDone` (59), and `GetGlobalValue` (74). A geographic condition must be AND, equivalent to `== 1` or `!= 0`, on Self/player. Form parameters are normalized against each override's master table. Changed conditions, OR, aliases, global comparisons, quest-only conditions, unknown functions and deleted/unresolved areas are skipped. The scanner never executes conditions; the engine evaluates them normally.

## Geographic exclusion recovery in 0.4

Independent mods can preserve an original track's conditions but respectively append `not in area A` and `not in area B`. The winning record may retain only one exclusion. Version 0.4 can recover the missing AND exclusions using the original record as a baseline.

- Only original Skyrim tracks in the original default `MUSCombat` list are candidates. Existing custom/Boss sharing, script-reference and palette protections still apply.
- Every override must preserve the complete non-condition payload, including audio paths, and retain the original condition sequence as an unchanged semantic prefix. Form parameters are normalized against master tables; unused padding is ignored.
- Every condition must use the supported typed AND schema. The original-condition schema additionally supports `GetCombatTargetHasKeyword` (707), used by vanilla battle tracks. All OR expressions are excluded, even if unchanged.
- Added conditions must exclusively be `GetInCell`, `GetInWorldspace`, or `GetInCurrentLoc` exclusions equivalent to `== 0` or `!= 1`, on Self/player. Quest edits, positive-area additions, aliases, unknown functions and deleted/unresolved areas prevent repair.
- Direct or transitive dependency patches that remove earlier exclusions or battle-track contributions are respected. Transitive-removal protection also applies to playlist restoration.
- Before applying the battle batch, the runtime checks the actual winning source and typed native conditions against the scanned snapshot. Required battle tracks from contributor sources and their ancestors must already exist or be included in the prepared restoration batch. A failed precheck skips the battle batch.
- Missing condition nodes are allocated on the game heap before commit. Only missing negative AND nodes are appended; existing nodes are not copied, deleted, or evaluated. Already-present exclusions are not duplicated. This runs at startup, without playback hooks.

Track-presence checks reduce the risk of excluding vanilla music without restoring its alternatives. They do not prove audio-file availability or that an alternative is playable in every quest state. Runtime allocation, playback and save/reload require game testing.

**This is not a verified replacement for Nexus 89099 (Music Patch for New Lands).** Its complete ESP variants have not been compared against generated results. Keep authored compatibility patches for unsupported logic and unverified combinations; passing a synthetic AND case does not demonstrate replacement of every new-land patch.

## Independent music and limits

If a new land's quest already starts its own Boss MusicType, that relationship stays in the author's script. UnifiedMusic does not need to recreate it and does not intercept it. Asking a different music pack to replace that Boss theme creates a new choice that a record scan cannot infer; it needs an explicit authored rule or patch.

VMAD reference scanning is conservative and can overprotect. It cannot discover every dynamic script lookup or another DLL's runtime changes. Avoiding playback hooks reduces intervention; it does not certify arbitrary mod combinations. Same-path audio replacements and incompatible script/condition edits remain outside scope.

## Installation and migration

Install `SKSE/Plugins/UnifiedMusic.dll` with a mod manager. Use matching SKSE and Address Library for **Steam 1.7.99 or 1.7.104**; other runtimes are refused. MTD and CME are not dependencies. Keep your music packs' own plugins and audio installed.

**0.3 and later do not read `*_MUS.ini` or `SKSE/Plugins/CombatMusic/*.json`.** It is not a drop-in replacement for 0.1/0.2 configurations that relied on those adapters. A pack distributing tracks only through configuration files does not gain a playlist contribution from file presence alone. Keep the required original framework or the older combined version when those features are needed; legacy rules are not migrated. Standalone frameworks are no longer blocked by filename, but co-installation and initialization order remain unverified.

Test this alpha in a separate profile with a disposable save. This project has not installed or launched the game for validation.

## Build

Requires Visual Studio 2022 C++ Build Tools, a Windows SDK, Git and CMake 3.24+. CommonLibSSE, commonlib-shared, spdlog and zlib are pinned in `cmake/Dependencies.cmake`. JsonCpp and SimpleIni are no longer used.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect --parallel 4
python tests/test_plugin_scan.py --inspect build/Release/UnifiedMusicInspect.exe -v
python tests/test_auto_combat_scan.py --inspect build/Release/UnifiedMusicInspect.exe -v
python tests/test_condition_repair.py --inspect build/Release/UnifiedMusicInspect.exe -v
```

`UNIFIED_MUSIC_DEPS_DIR` reuses local dependency Git checkouts after revision checks. No game path is compiled into the DLL.

`UnifiedMusicInspect.exe plugin1.esm plugin2.esp ...` runs the same scanner offline. Planning compares original definitions and explicit dependencies without assuming input order. It emits `ALLOW`, `APPEND`, `PRESERVE`, `BATTLE_DEFAULT`, `AUTO_COMBAT`, `CONDITION_UNION`, and skip diagnostics. `AUTO_COMBAT *` means unconditional contributions; other scopes use `0=Cell`, `1=Worldspace`, `2=Location`. These are proposed reconciliations, not counts of tracks or exclusions actually missing at runtime. Local IDs are decimal.

## License

The project remains AGPL-3.0-or-later. Historical MTD/CME attribution and license notices remain in `PROVENANCE.md` and `vendor/`; their execution code is absent from the current source tree. Remaining dependencies retain their original licenses, included in binary packages. Earlier combined implementations remain in Git history and prior releases.
