# Harmonia — Automatic Music Compatibility

A conservative music compatibility plugin for **Skyrim Steam 1.7.99 / 1.7.104**, previously called UnifiedMusic. One SKSE DLL restores eligible playlist contributions, reconciles proven geographic exclusions, and recovers music assignments lost to independent area overrides. The repository, DLL and inspector retain their technical UnifiedMusic names for upgrades.

**Status: 0.5.0 alpha. Builds and offline tests are recorded in VALIDATION.md. In-game playback, quests, transitions, and save/reload have not been tested.**

## Runtime

At SKSE `kDataLoaded`, Harmonia reads loaded ESP/ESM/ESL files through the visible Data directory, resolves their master tables, and builds a restoration plan. It appends missing track references to eligible original music lists once, retaining the final native track forms. Existing condition nodes and author-controlled playback remain in place.

Since version 0.3, the project has removed the embedded MTD and CME implementations, INI/JSON parsing, playback selection hooks, and playback ownership state. SKSE hook and trampoline initialization are explicitly disabled. There are no forced play/stop calls, temporary MusicTypes, or polling loops. The runtime consists of the scanner, protection checks, and startup list/condition/assignment adapters; an offline inspector is optional.

No third-party mod-name list is used. Audio replacement packs retain the game's file-resolution behavior. No audio, game plugins, paid Creation assets, or Papyrus replacements are distributed.

## Restoration and protection

- Original exploration, town, dungeon and tavern types can receive independent additions lost to another override.
- Original `MUSCombat` can receive proven single-audio additions that are unconditional or have supported positive Cell/Worldspace/Location conditions on the player.
- Changed type settings, removed base tracks, deleted types, and explicit dependent patches removing earlier contributions protect the entire target list.
- Custom MusicTypes are not merged internally. CELL/worldspace/location music fields can recover a single proven authored assignment under the rules below; region sound-data assignments remain untouched.
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

## New in 0.5: regional guards and area assignments

**Regional guards.** A new unconditional battle track can otherwise play in a land whose authors excluded the original battle music. Harmonia can append the same exclusions to that new track when all of the following evidence is available:

- Every original ordinary battle track agrees on the exclusion after the conservative reconciliation above. The only accepted ordinary baselines are no conditions or the vanilla negative `GetCombatTargetHasKeyword(ActorTypeDragon)` condition; dragon-only baselines are separate. Unknown baseline roles abort this inference.
- Shared original tracks can supply read-only evidence, but are never repaired. An exclusion must exist in every dependency-maximal version of such a track; competing versions cannot establish a common policy.
- The same default battle list contains an eligible unshared single-audio replacement with exactly one positive Self condition for that area. Quest-gated replacements and conjunctions of several areas cannot prove coverage of the whole area.
- The new generic track is unconditional, not scripted/shared/palette-owned, and has unchanged audio/metadata across its overrides. Existing authored conditional tracks are not broadened or rewritten.
- Original policy tracks, the replacement, and the target are checked against their winning native condition snapshots before commit. A changed witness or missing track skips the complete battle batch. This guards record eligibility, not audio decoding or every native selection behavior.

Only the proven area's negative AND conditions are added. A second independent land can contribute another exclusion. The regional and Boss tracks retain their own conditions and ownership.

**Area assignments.** If the original CELL/WRLD/LCTN assigns music A, one mod explicitly assigns B, and an independent override still carries A, Harmonia can restore B. B may be a new land's custom MusicType; the tool restores the assignment without merging that custom list. The original A may be null. Only the music pointer changes; CELL uses its existing or newly created music extra-data node.

Two different new assignments, an explicit clear, a deleted/malformed record, missing/cyclic dependencies, or a dependent patch reverting B all prevent recovery. An unchanged ancestor cannot overwrite its own dependent author and creates no proposal. Runtime source and pointer checks preserve unexpected changes from other systems. REGN sound-data entries and worldspace inheritance flags are not rewritten. This field merge follows declared dependencies; an independent author deliberately carrying the baseline is indistinguishable from an accidental overwrite.

Both features reuse startup scanning. They do not require generating duplicate CME JSON or MTD INI rules. A custom playback relationship absent from the records still requires an authored decision; configuration syntax cannot supply that missing intent.

Before restoring an ambient or battle playlist, the runtime compares its current track multiset with the winning plugin snapshot. Earlier framework additions, replacements or explicit clears cause that playlist to be skipped. This does not prevent a later framework from changing it, and does not validate priority fields or scripted playback state.

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
$env:UNIFIED_MUSIC_INSPECT = (Resolve-Path build/Release/UnifiedMusicInspect.exe).Path
python -m unittest discover -s tests -p 'test_*.py' -v
```

`UNIFIED_MUSIC_DEPS_DIR` reuses local dependency Git checkouts after revision checks. No game path is compiled into the DLL.

`UnifiedMusicInspect.exe plugin1.esm plugin2.esp ...` runs the same scanner offline. Planning compares original definitions and explicit dependencies without assuming input order. It emits `ALLOW`, `APPEND`, `PRESERVE`, `BATTLE_DEFAULT`, `AUTO_COMBAT`, `CONDITION_UNION`, `CONDITION_GUARD`, `RESTORE_MAPPING`, and skip diagnostics. `AUTO_COMBAT *` means unconditional contributions; other scopes use `0=Cell`, `1=Worldspace`, `2=Location`. These are proposed reconciliations, not counts of changes actually missing at runtime. Local IDs are decimal.

## License

The project remains AGPL-3.0-or-later. Historical MTD/CME attribution and license notices remain in `PROVENANCE.md` and `vendor/`; their execution code is absent from the current source tree. Remaining dependencies retain their original licenses, included in binary packages. Earlier combined implementations remain in Git history and prior releases.
