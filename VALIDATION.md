# Validation — Harmonia 0.6.0-alpha, 2026-11-14

## Build and synthetic checks

- Release x64 DLL and inspector built with VS 2022 / MSVC 19.44 and pinned CommonLibSSE, commonlib-shared, spdlog and zlib. Supported executables remain Steam 1.7.99 and 1.7.104. No SimpleIni, JsonCpp, CME/MTD execution modules, playback hooks or trampoline initialization.
- **204 synthetic test methods passed**: 170 existing tests and 34 configuration/copied-track tests, including repeated-key and argument-order subcases. Full output: `tests/latest-results.txt`.
- New coverage: configuration-only contributions; exact EditorID and local FormID resolution; UTF-8 BOM/comments/case/whitespace; multiple-file/key deduplication; root-only discovery; custom/Boss/silent/palette distributions; unsupported clears/new types/sections; malformed values/UTF-16; optional/stale/deleted/invalid references; dependent removals; ordinary negative-dragon copied predicates; positive dragon/quest/OR preservation; plugin/config-only Boss and Civil War ownership; script and palette protection; absent/quest-gated replacements; changed audio/conditions; ambient scopes/sharing; read-only original ambient policy; configured replacement witnesses; input-order independence.
- Existing regression coverage includes structural ESP parsing, compression/XXXX, original/dependency comparison, script and shared ownership, combat geography, typed condition normalization, independent exclusion unions, dependent reversions and unique area music assignments.

## Actual installation and downloaded pack

Music Replacer Reverter v1 was downloaded from [the author's Nexus page](https://www.nexusmods.com/skyrimspecialedition/mods/137806), file 578246. The archive is 447,902,545 bytes with SHA-256 `7a4209dd4d9a4d58f832f8cafffd1a4b3fd1e39d45f8ea3ad9a810c1812c6420`, matching the page's file scan hash. Only its ESP (42,656 bytes) and INI (15,963 bytes) were extracted. No BSA/music was installed or republished.

| Input | Result |
| --- | --- |
| All 94 currently enabled plugins | Zero additions, battle groups, condition unions/guards, area repairs and distributions; approximately 2.6 seconds |
| Same 94 plus actual MRR ESP/INI, outside game Data | 303 valid unique distribution pairs, 56 target MusicTypes, 121 distinct tracks; zero inferred condition or area repairs; approximately 2.5 seconds |
| Five official base plugins, actual MRR, synthetic supported land | Same 303 distributions, one regional battle contribution group and three copied-track guards; approximately 2.1 seconds |

An independent Python config/record comparison matched every valid `DISTRIBUTE` pair. MRR's ESP contains 251 MUST records. Its INI references 133 distinct IDs, of which **12 are absent**, affecting **32 unique destination/track pairs**: `083D, 084F, 0850, 0853, 0854, 0855, 0856, 0857, 0858, 0859, 085A, 085B`. Harmonia emits `CONFIG_SKIP`, matching MTD's behavior of ignoring unresolved track lookups. It does not guess replacement IDs or reconstruct palette records.

The real-MRR/synthetic-land case appends a pure negative worldspace condition to five original ordinary battle tracks and supplies a pure positive-area replacement. It generates guards for actual MRR IDs `080E, 081F, 0820`. IDs `081D, 081E` also belong to Civil War music and are preserved. Dragon/Boss IDs `0821, 0822, 08FA` are preserved. This proves the planner handles actual renamed-copy records under the supported policy; **the synthetic land is not Bruma, ECSS or an actual quest simulation**.

In the installed-plus-MRR case, Orsinium's original-track extensions and protected sharing remain outside inferred guard eligibility. There are no guard proposals. This means distribution works, **not that copied ordinary music has been made region-safe for all installed story content**. Shared copies and unsupported quest-dependent exclusions still require authored decisions or patches.

Hashes of all 14 previously audited Creation plugins matched prior evidence. `plugins.txt` and `loadorder.txt` hashes were unchanged. Neither Data, saves nor load order was modified. Local detailed records, downloaded assets, private plugin evidence and generated synthetic plugin files are excluded from this repository.

## Runtime implementation review

Configuration contributions participate in protection and planning before writes. Explicit destinations retain author conditions; inferred condition mutations require exclusive ownership across plugin TNAMs and configuration destinations. Missing tracks are skipped; unsupported configuration operations pause the whole automatic plan.

The runtime detects a loaded MTD by SKSE plugin identity/canonical module name and pauses its own writes. Otherwise it prepares all ambient, combat and configured additions together, checks current winning playlist multisets, validates typed condition witnesses and required tracks, allocates condition nodes and reserves list capacity, rechecks snapshots, then commits. A failed music batch also prevents area-assignment recovery. No earlier ambient mutation invalidates a later combat snapshot. Existing nodes are not copied/deleted; only missing AND exclusions are appended.

These native checks were reviewed and compiled. They have **not** run in Skyrim; offline tests exercise the shared scanner/planner, not SKSE listener timing, native pointers or the audio manager.

## Remaining validation limits

- No game process was launched. Native allocation, winning-source resolution, MTD detection, atomic prechecks, audio decoding, selection probabilities, cell transitions, combat end and save/reload are unverified.
- A shared ordinary MRR copy can still play in a region whose originals were excluded. Preserving its Civil War/Boss ownership is not a complete conflict repair. Runtime clones or guessed ownership rewrites were deliberately not added.
- Full Bruma/ECSS/Lorkhan/Chapter II combinations and complete Nexus 89099 patch variants were not tested. This is not certification that one tool replaces every authored patch.
- MTD runtime type creation, list clears, Location/Region assignment and CME JSON runtime targeting remain unsupported. Loaded MTD pauses Harmonia writes; arbitrary renamed/forked frameworks and later runtime changes are not controlled.
- Ambient regional inference is limited to unconditional, unshared configured tracks and unanimous proven policies. Shared exploration palettes, weather/day-night/quest expressions and unrecognized conditions remain author-owned.
- Explicit configuration distribution can intentionally add music to story/Boss types. Preservation means their authored conditions are retained, not that explicit author additions are ignored. Configuration cannot infer a missing custom battle relationship.
- No audio extraction/playback validation occurred. Missing record IDs are reported, not repaired. Track presence does not prove an available file or playback in every reachable quest state.

Earlier 0.4 work separately inspected 94 plugins and rehearsed 396 supplied condition scenarios plus 48 scripted gate boundary inputs. That historical exercise was not rerun as a Papyrus VM or expanded into a claim about the new MRR combination. Version 0.6 reran actual plugin/config planning and checked Creation hashes.
