# Harmonia — automatic music compatibility

**Harmonia 0.6.0-alpha** combines conservative playlist restoration, additive MTD configuration distribution, proven regional guards, and recovery of overwritten area music assignments in one startup pass. The repository, DLL and inspector retain the technical name `UnifiedMusic` for upgrades.

One DLL; matching SKSE and Address Library for **Steam Skyrim 1.7.99 or 1.7.104**. MTD and CME are not dependencies. No playback hooks, polling, new MusicTypes, generated CME rules, or music assets are included. This release is verified offline; native playback is untested.

The core policy is additive reconciliation of contributions **already assigned to the same playlist**, not automatic assignment of every new MUST record to vanilla music. Audio-file replacement (for example Lorkhan's original paths), MUSC playlist overrides and MUST condition edits are separate layers. Unioning a playlist does not undo an audio replacer or carry an original track's later conditions to a distinct copied form. Deliberate removals and dedicated playlists remain protected.

## Configuration distribution in 0.6

Harmonia reads root `Data/*_MUS.ini` files and supports additive entries targeting an **existing, unambiguous MusicType EditorID**:

```ini
[General]
MUSCombat = 0x800~Example.esp, 0x801~Example.esp
```

- IDs are local hexadecimal FormIDs, not load-order/runtime IDs. `.esm`, `.esp` and `.esl` filenames are case-insensitive; EditorIDs retain exact case. UTF-8 BOM, whitespace, whole-line `;`/`#` comments, repeated keys and multiple files are accepted. The `_MUS.ini` suffix is case-insensitive. Subdirectories are not scanned.
- Contributions are unioned and deduplicated across files and existing native tracks. Existing duplicate entries in a plugin are not removed. This matches MTD's set-based configuration collection rather than treating repeated INI tokens as playback weights.
- Explicit author rules may target custom, story, Boss, palette or silent music. These are distributed with their existing conditions; that explicit destination is not permission to infer new Boss replacements or rewrite shared conditions.
- Missing, deleted and malformed track references, and tracks deliberately removed by a dependent playlist override, produce `CONFIG_SKIP` and are not distributed. Other valid entries continue. No missing record is reconstructed from a filename or EditorID guess.
- `!` clears, runtime type creation, Location/Region sections, unknown sections, malformed syntax and ambiguous/deleted/renamed target types produce `CONFIG_UNSUPPORTED` followed by `CONFIG_HANDOFF`. **All automatic writes pause** rather than partially execute a configuration whose intent is unsupported. Use the original framework or an authored patch for such rules.
- Inline comments, multiline values and UTF-16 are outside this deliberately small parser. Convert those files to the documented form before using this adapter.

The current Music Replacer Reverter v1 configuration uses this additive subset. Local verification found **303 valid type/track pairs across 56 MusicTypes and 121 distinct tracks**, plus **12 absent track IDs affecting 32 unique pairs**. Harmonia reports and skips those missing records, as MTD's lookup does. This does not reconstruct missing palette definitions or certify that every intended vanilla track plays.

## Copied tracks and regional guards

Configured contributions enter the same plan as plugin contributions, before ownership checks and condition inference. Renamed or re-encoded audio does not need fingerprinting: the planner proves the track's **playlist role and regional policy**, not its musical identity.

A new ordinary track can receive negative AND geography conditions only when:

1. Original ordinary tracks in that playlist unanimously exclude the area, with their baseline conditions preserved. Read-only/shared originals must contain the exclusion in every dependency-maximal version that could win. Unknown baseline roles abort inference.
2. An unshared single-audio replacement in the same current or planned list has exactly one positive Self condition covering that area. Quest-gated, multi-area and OR replacements cannot prove full-area coverage.
3. The candidate's metadata and recognized conditions are stable across overrides. It has no direct VMAD reference, palette ownership, or other plugin/configuration playlist owner.
4. The runtime verifies winning condition snapshots and all required original/replacement tracks before committing.

For the default battle list, candidates may be unconditional or retain the vanilla negative `GetCombatTargetHasKeyword(ActorTypeDragon)` predicate. Positive dragon/Boss predicates and arbitrary quest conditions remain unchanged. Eligible original ambient lists can guard configured **unconditional** tracks using the same proof; ambient policy records themselves remain read-only. Shared exploration palettes and day/night or weather predicates are not generalized into unconditional geography.

Only new negative conditions are appended. Existing conditions, audio paths, script calls and dedicated MusicTypes stay author-owned. Shared tracks are deliberately not cloned at runtime or globally restricted on behalf of just one owner.

**Concrete limit:** with real MRR and a synthetic supported new-land policy, three ordinary battle copies get guards. Two other ordinary copies also belong to `MUSCombatCivilWar` and are left unchanged; three dragon/Boss copies are also preserved. Shared ordinary copies can still play in an exclusive new land unless an authored patch separates their use. The synthetic land is not Bruma or Extended Cut verification.

## Existing restoration behavior

- Recover independent additions lost to overrides of original exploration, town, dungeon and tavern types.
- Recover eligible single-audio contributions to original `MUSCombat`: unconditional, or stable supported positive geography with a recognized condition schema.
- Recover independent negative geographic extensions to original Skyrim battle tracks. Every override must retain original non-condition metadata and the baseline condition prefix. Direct/transitive dependency removals, OR expressions, quest edits and shared/scripted/Boss/palette tracks prevent repair.
- Restore one unambiguous independent CELL/WRLD/LCTN music assignment lost beneath an inherited baseline. Only the music field changes. Competing assignments, explicit clears, deleted/malformed records, dependent reversions and missing/cyclic dependencies prevent recovery. REGN sound-data rules and worldspace inheritance flags are not rewritten.

Automatic restoration respects changed type settings, removed base tracks and dependent patches removing earlier additions. Config-only owners participate in shared-track protection. Ordinary ambient playlist restoration may share ordinary tracks, but new inferred condition edits require exclusive ownership. Partial Form and Ignored records do not contribute. Custom MusicTypes are not automatically merged internally; explicit additive configuration is a separate authored instruction.

The recognized typed condition schema is `GetQuestRunning` (56), `GetStage` (58), `GetStageDone` (59), `GetGlobalValue` (74), `GetInCell` (67), `GetInWorldspace` (310), `GetInCurrentLoc` (359), and baseline `GetCombatTargetHasKeyword` (707). Form parameters are normalized against each master table. Unknown functions, aliases, global comparisons, target swapping and unsupported run-on contexts are not inferred. The engine evaluates conditions normally.

## One runtime owner and one commit

At DataLoaded, the scanner reads enabled plugins and configurations, plans all contributions, and checks live winning playlist multisets and typed condition snapshots. All required additions, capacity and condition nodes are prepared before lists or conditions change. A failed precheck skips the complete music batch; dependent area recovery is also skipped. Existing condition nodes are never replaced or deleted.

If **MusicTypeDistributor is loaded**, Harmonia pauses all automatic writes with a log message. This prevents both early and late MTD listeners from competing with integrated distribution. Use one distributor. Harmonia does not disable or delete another DLL.

CME remains necessary for explicit runtime relationships absent from records, such as custom combat-target/keyword or area-clear rules. Harmonia does not read CME JSON. Co-installation with playback frameworks and arbitrary later runtime mutations is not certified; snapshot checks cannot control future writes.

## Installation and boundaries

Install `SKSE/Plugins/UnifiedMusic.dll`. Keep each music pack's own ESP/ESL/BSA/audio and supported `_MUS.ini` files. For packs using only the documented additive subset, Harmonia can perform distribution without MTD. If advanced MTD rules are required, retain MTD; Harmonia's coexistence rule pauses its writes.

Upgrading from 0.5 adds INI reading and single-batch behavior. Versions 0.3–0.5 ignored configuration files; versions 0.1/0.2 embedded broader framework adaptations, still available in historical tags. This is not a complete MTD/CME replacement.

No matching/renaming of two audio replacers, missing-record reconstruction, arbitrary Papyrus interpretation, dynamic script music stack repair, or inference of an absent independent custom battle relationship is performed. Dedicated new-land/story/Boss logic is preserved rather than assigned a guessed replacement. An independent author's deliberate baseline override can be indistinguishable from an accidental overwrite.

**The current 94-plugin installation plus MRR produces distributions but no geographic guards.** Existing Orsinium conditions contain protected sharing/unsupported extensions. Those complex conditions and shared copies can still require a targeted patch. Bruma, Extended Cut, Lorkhan, Chapter II and all Nexus 89099 variants have not been jointly validated. Keep applicable authored patches; this release does not establish universal compatibility for that combination.

Track presence does not prove audio availability, decoding or reachability in every quest state. Offline validation does not execute native allocation, playback, cell transitions or save/reload. No game installation or save was changed during validation. See [VALIDATION.md](VALIDATION.md).

## Build and inspect

Visual Studio 2022 C++ Build Tools, Windows SDK, Git and CMake 3.24+. CommonLibSSE, commonlib-shared, spdlog and zlib are pinned in `cmake/Dependencies.cmake`. No JsonCpp or SimpleIni dependency.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect --parallel 4
$env:UNIFIED_MUSIC_INSPECT = (Resolve-Path build/Release/UnifiedMusicInspect.exe).Path
python -m unittest discover -s tests -p 'test_*.py' -v

build/Release/UnifiedMusicInspect.exe Skyrim.esm Example.esp --config Example_MUS.ini
build/Release/UnifiedMusicInspect.exe Skyrim.esm Example.esp --config-dir Data
```

Supply all enabled plugins, including masters. The inspector does not discover a load order or auto-load INIs without the explicit option. It performs no writes to plugins. Outputs include `APPEND`, `AUTO_COMBAT`, `DISTRIBUTE`, `CONDITION_UNION`, `CONDITION_GUARD`, `RESTORE_MAPPING`, preservation and configuration diagnostics. Values after the colon are decimal local IDs; geographic kinds are `0=Cell`, `1=Worldspace`, `2=Location`. These are proposals, not executed runtime changes. Exit 0 means scanning completed; check `CONFIG_HANDOFF` and skip diagnostics before interpreting coverage.

## License

AGPL-3.0-or-later. Historical MTD/CME attribution and licenses remain in `PROVENANCE.md` and `vendor/`; their execution modules are not compiled. The small 0.6 additive configuration adapter is documented separately. No game, paid Creation, downloaded music mod, or music asset is redistributed.
