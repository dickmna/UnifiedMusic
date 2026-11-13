# UnifiedMusic

A conservative playlist-restoration plugin for **Skyrim Steam 1.7.99 / 1.7.104**. One SKSE DLL reconstructs eligible music contributions lost when plugins override the same original playlist.

**Status: 0.3.0 alpha. Builds and offline tests are recorded in VALIDATION.md. In-game playback, quests, transitions, and save/reload have not been tested.**

## Runtime

At SKSE `kDataLoaded`, UnifiedMusic reads loaded ESP/ESM/ESL files through the visible Data directory, resolves their master tables, and builds a restoration plan. It appends missing track references to eligible original music lists once, retaining the final native track forms and their playback conditions.

Version 0.3 removes the embedded MTD and CME implementations, INI/JSON parsing, playback selection hooks, and playback ownership state. SKSE hook and trampoline initialization are explicitly disabled. There are no forced play/stop calls, temporary MusicTypes, or polling loops. The runtime consists of the scanner, protection checks, and startup list adapter; an offline inspector is optional.

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

## Independent music and limits

If a new land's quest already starts its own Boss MusicType, that relationship stays in the author's script. UnifiedMusic does not need to recreate it and does not intercept it. Asking a different music pack to replace that Boss theme creates a new choice that a record scan cannot infer; it needs an explicit authored rule or patch.

VMAD reference scanning is conservative and can overprotect. It cannot discover every dynamic script lookup or another DLL's runtime changes. Avoiding playback hooks reduces intervention; it does not certify arbitrary mod combinations. Same-path audio replacements and incompatible script/condition edits remain outside scope.

## Installation and migration

Install `SKSE/Plugins/UnifiedMusic.dll` with a mod manager. Use matching SKSE and Address Library for **Steam 1.7.99 or 1.7.104**; other runtimes are refused. MTD and CME are not dependencies. Keep your music packs' own plugins and audio installed.

**0.3 does not read `*_MUS.ini` or `SKSE/Plugins/CombatMusic/*.json`.** It is not a drop-in replacement for 0.1/0.2 configurations that relied on those adapters. A pack distributing tracks only through configuration files does not gain a playlist contribution from file presence alone. Keep the required original framework or the older combined version when those features are needed; legacy rules are not migrated. Standalone frameworks are no longer blocked by filename, but co-installation and initialization order remain unverified.

Test this alpha in a separate profile with a disposable save. This project has not installed or launched the game for validation.

## Build

Requires Visual Studio 2022 C++ Build Tools, a Windows SDK, Git and CMake 3.24+. CommonLibSSE, commonlib-shared, spdlog and zlib are pinned in `cmake/Dependencies.cmake`. JsonCpp and SimpleIni are no longer used.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect --parallel 4
python tests/test_plugin_scan.py --inspect build/Release/UnifiedMusicInspect.exe -v
python tests/test_auto_combat_scan.py --inspect build/Release/UnifiedMusicInspect.exe -v
```

`UNIFIED_MUSIC_DEPS_DIR` reuses local dependency Git checkouts after revision checks. No game path is compiled into the DLL.

`UnifiedMusicInspect.exe plugin1.esm plugin2.esp ...` runs the same scanner offline. Planning compares original definitions and explicit dependencies without assuming input order. It emits `ALLOW`, `APPEND`, `PRESERVE`, `BATTLE_DEFAULT`, `AUTO_COMBAT`, and skip diagnostics. `AUTO_COMBAT *` means unconditional contributions; other scopes use `0=Cell`, `1=Worldspace`, `2=Location`. These are proposed restorations, not counts of tracks actually missing at runtime. Local IDs are decimal.

## License

The project remains AGPL-3.0-or-later. Historical MTD/CME attribution and license notices remain in `PROVENANCE.md` and `vendor/`; their execution code is absent from the 0.3 source tree. Remaining dependencies retain their original licenses, included in binary packages. Earlier combined implementations remain in Git history and prior releases.
