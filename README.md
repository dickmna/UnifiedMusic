# Harmonia — composer-aware music compatibility

[Nexus Mods — downloads and description](https://www.nexusmods.com/skyrimspecialedition/mods/199219)

**Harmonia 1.0.1** applies explicit music configurations first, then restores supported contributions, inherits authored scene restrictions for ordinary uses, isolates shared tracks, and optionally lets supported original/replacement audio coexist with the separate HQ audio pack. One SKSE DLL; no patch-generation interface or generated ESP. [中文说明](使用说明.md).

The DLL remains `UnifiedMusic.dll` for upgrades. This build supports **Steam Skyrim 1.7.104 only**, with matching SKSE and Address Library. It does not require separate MTD or CME DLLs. Offline validation is complete; native playback has not been tested. See [VALIDATION.md](VALIDATION.md).

Version 1.0.1 moves gameplay plugin reading to Skyrim's native `TESFile` interfaces, following the API pattern used by Modex / Help Extender. Portable ESP parsing and zlib remain in the offline inspector only. Both readers feed the same record decoder and music rules. The optional HQ pack stays at 1.0.0; its audited mappings are unchanged. See [AUDIT.md](AUDIT.md) for the implementation and validation boundaries.

The Nexus installer combines the unchanged 1.0.1 core and 1.0.0 HQ pack in one FOMOD: the core is required and **Original Audio HQ** is an optional checkbox, off by default. Both selections passed the installed Vortex FOMOD engine with exact destination checks (16 or 258 files). MO2 compatibility is structural, not a claimed MO2 UI test. See [packaging instructions](installer/README.md) and the [Nexus description](docs/NEXUS.md).

## Composer policy

The unit of compatibility is **a track's use in one playlist**. Music A and B enrich their authored ordinary playlists. When scene C supplies scoped music and a related plugin excludes ordinary music in the matching scope, Harmonia treats that as a scene arrangement. The author's existing ordinary-track restrictions supply the inherited rule. There is no song-count threshold. This is an explicit player policy, not a claim to prove every author's intent.

Classification uses original MUSC membership, MUST conditions, dependencies, explicit configurations and typed script properties. An official area assignment can establish ordinary use for an otherwise test-named town/exploration list; the word “Test” alone is not proof that a playlist is unused. Dedicated, silent and one-shot roles remain protected from automatic enrichment. Audio filenames do not establish a role. Automatic recovery respects dependent removals, positive eligibility and per-song reversions; explicit configuration can deliberately change membership or area assignment.

The small rule set is:

1. Restore independent authored contributions to eligible ordinary lists, including original tracks deliberately reused by another mod; retain current order and multiplicity. A reference to public MUSCombat/ambient lists does not by itself establish exclusive script ownership. Battle priority/fade settings remain the winning plugin's fields and do not suppress membership restoration.
2. Pair a scoped song with a matching ordinary exclusion from the same source/dependency family. One matching donor can establish a policy; multiple unrelated exclusions cannot. Sharing the scene song with a dedicated playlist does not erase its authored ordinary use. The scene song is never modified.
3. Extract **complete added condition clauses** from ordinary donors, removing their original eligibility. Within each authoring source, inherit only clauses common to its matching donors. Keep AND/OR grouping, quest/global comparisons and Self/explicit-player context. A donor's original night-only or negative-dragon condition is not copied; newly added standalone clock restrictions also remain song-specific. Time terms inside a scene/quest OR clause remain intact. Do not negate or reconstruct the scene song's playback expression.
4. Apply these rules to stable ordinary uses: unconditional/time-only ambient tracks, or unconditional/default-negative-dragon combat tracks. For example, an authored ordinary rule `outside C OR quest stage < 40 OR stage >= 50` is inherited as written. If the author instead excludes ordinary music from all of C, that broader authored restriction is retained even if a Boss cue has narrower gates. Explicit positive assignments and dependent per-song reversions take priority.
5. Unreadable scene evidence or missing/invalid scene main/finale pauses scene-dependent automatic actions. Explicit memberships remain under their authored conditions, as with MTD. At runtime, a pre-commit failure retries the independent remainder, then valid explicit instructions alone if necessary; exceptions do not trigger a retry. No unverified rule is generalized.

Every inferred condition edit uses a playlist-local native single-track instance, including original tracks. Other playlists, palettes and scripts retain their original wrapper. No new FormID, persistent MusicType, generated ESP or generated JSON is required. Dedicated story/Boss types and positive dragon roles are not automatically enriched. An author-supplied explicit distribution/selector can still target them.

Layered music frameworks may define an empty track before a dependent region plugin supplies its audio and conditions. Harmonia uses the dependency-maximal definitions, requiring agreement between competing final branches. An already-authored battle membership can be restored with its complete supported read-only conditions intact, without inventing a geographic selector. If the same contributed track also has an explicit membership in another original Skyrim playlist, that exact link can be restored there too, including Boss/Civil War uses; the general music pool is never copied into those lists. Deleted/one-shot lists and dependent removals remain protected. Scene matching can follow an unambiguous Location parent chain; cycles, changed parents and unresolved ancestry do not invent coverage.

Complete supported OR clauses can be inherited. A scene song's terminal OR flag can be retained as read-only evidence: no conditions are appended to that song and its expression is never inverted. Unknown functions, unresolved references and unsupported edits remain boundaries. Harmonia does not execute Papyrus or reconstruct every script's timing. See [VALIDATION.md](VALIDATION.md) for the current installed combination and authored Bruma/Orsinium rules.

## Distribution and selection

**Explicit configuration takes priority; automatic recovery fills the remaining supported gaps.** Root `Data/*_MUS.ini` supports MTD additions, clears and Location/Region assignments to existing unambiguous MusicTypes:

```ini
[General]
MUSCombat = 0x800~Example.esp, 0x801~Example.esp

; Clear this list, then use only the explicitly configured tracks.
MUSTavern! = 0x802~Example.esp
; An empty value after ! deliberately leaves the list empty.
MUSTownTest! =

[Location]
ExampleLocation = ExampleMusicType

[Region]
ExampleRegion = ExampleMusicType
```

Local hexadecimal IDs have no load-order prefix. Plugin names and ASCII EditorIDs are case-insensitive. Case-folded EditorIDs must identify one form; collisions, genuine renames and deleted targets remain rejected. Contributions are deduplicated; intentional duplicates already in a plugin are retained unless that list is cleared. Explicit rules can target custom/Boss/palette/silent types. Adding a membership does not remove the track's authored conditions or force it to play.

`Type!` clears that playlist once and retains the union of valid tracks explicitly configured for it, including plain `Type` additions in other files. Automatic membership recovery, inferred guards and original-audio coexistence do not refill a cleared list. Shared track objects and other playlists remain intact. This deterministic clear/union rule deliberately resolves mixed `Type`/`Type!` keys; upstream MTD processes those separate hash-map keys without a defined order.

Location assignments set the existing Location's music pointer. Region assignments set music on its existing sound-data entries, preserving their flags, priorities and other fields; a Region without such an entry remains unchanged. Configurations are processed by alphabetical file path, with the last assignment to an area winning. These explicit mappings take priority over automatic area recovery.

INI parsing uses **SimpleIni**, as MTD does: UTF-8 BOM, CRLF, whitespace, whole-line comments, repeated keys and `<<<TAG` multiline values are accepted. Whitespace-delimited `;`/`#` end comments are also accepted. Unrelated sections/metadata are ignored. Missing/deleted/invalid track references produce `CONFIG_SKIP`; an explicit clear still applies if all its requested tracks are skipped. Runtime MusicType creation, UTF-16/NUL, invalid recognized values and unresolved/ambiguous target references pause the plan with `CONFIG_HANDOFF` rather than applying half a configuration.

Author-supplied CME JSON is read from `Data/SKSE/Plugins/CombatMusic/*.json`:

```json
{
  "combatMusic": [{
    "isCombatMusic": true,
    "newMusic": "Example.esp|0x900",
    "worldspaces": {"AND": true, "forms": ["Example.esp|0x800"]}
  }]
}
```

The six supported filters are `worldspaces`, `cells`, `locations`, `locationKeywords`, `combatTarget` (NPC bases) and `combatTargetKeywords`. Forms may use a unique case-insensitive EditorID or `Plugin.esp|0xLOCAL`. All required (`AND: true`) filters must match; if optional filters exist, at least one must match. Each filter matches any of its listed forms. Locations include parents, with cycle/depth protection. `isCombatMusic: false` selects the dungeon-cleared channel.

Matching actor/Boss rules outrank geographic rules; within a priority, more matching required filters win. Different targets tied at the highest rank preserve the engine choice. A highest-ranked target that is unavailable/externally active also preserves that choice; it does not silently fall through to generic regional music. Combat and cleared channels do not share stop bookkeeping.

JSON parsing uses **JsonCpp**, as CME does. BOMs, comments and extra descriptive fields are accepted. Empty `forms` filters are ignored; a rule with no nonempty filters is omitted, matching upstream's zero-rank behavior rather than becoming a global selector. Duplicate keys, trailing non-comment data and invalid/missing/ambiguous references still pause the plan instead of weakening a rule.

This adapter uses six CME-derived entry points, checked before installation; no ActorUpdate polling or CME combat-music-fix option. It is not a complete CME/MTD implementation. Automatic regional guards remain native track conditions and require **no generated JSON**. An independent custom combat library without a recorded/authored relationship still needs an explicit rule or patch.

## Original audio alongside replacers

Supported file replacements become **additional same-role music candidates**. Original and winning replacement audio enter the same playlist preparation/commit path as added tracks and receive the same effective scene rules. A replacer no longer runs through a separate post-pass that can miss those rules.

The **optional Original Audio HQ pack** supplies independent original-music resources and a verified main/finale manifest. Without that pack, original coexistence is disabled; playlist recovery, explicit configuration and scene protection still work. The main DLL and inspector contain **no BSA parser, original-archive rereader or audio disk-cache writer**. The game loads music through its normal resource system.

For an eligible original use, Harmonia checks the installed manifest, main/finale fingerprints and winning native paths. It retains the replacer's installed path and clones the effective local use with the pack's independent HQ main/finale paths. Effective conditions, cue points and loops remain together in the existing prepare/commit path. A missing or changed file skips that pair, not unrelated compatibility work.

Known stock and HQ fingerprints prevent extra original weight when the winner is already the original composition. An existing same-playlist copy, including the verified MRR audio, also suppresses Harmonia's extra use; main and finale must match as a pair. Explicit memberships and their conditions are retained rather than consolidated or broadened. This is exact identification of known encodings, not perceptual music recognition; arbitrary re-encodings and nested palette copies remain outside that guarantee.

This supports **verified original + final winning replacer**. It cannot recover every overwritten pack, missing source music, changed record audio metadata or a visible unsupported WAV alternative. The optional source has no verified matches for five Update.esm Creation Club dungeon cues or four broken MRR records. Their installed playback remains unchanged. Source, permissions, file mapping and packaging details are in [audio-pack/README.md](audio-pack/README.md). The main/code packages contain no music; the optional archive contains only the attributed MRR/UHDAP-derived resources.

## Other restoration and runtime ownership

- Restore independent additions to eligible official exploration, town, dungeon and tavern lists, and supported ordinary/default-battle contributions.
- Restore independent negative geographic extensions to eligible original uses while preserving baseline conditions, run-on context and dependent reversions. These edits also use private playlist instances.
- Recover one unambiguous independent CELL/WRLD/LCTN music assignment hidden by a baseline override. Only that music field changes; competing assignments, explicit clears and dependent reversions remain untouched. Explicit Location/Region configuration wins; automatic REGN recovery is not inferred.

The shared typed CTDA schema includes functions 18, 56, 58, 59, 74, 67, 310, 359 and 707. Read-only authored tracks/witnesses also support 117 (Region), 562 (Location keyword) and 520 (platform predicate, no arguments); those extra predicates are preserved and are not copied into inferred guards. Unknown functions, alias/global-comparison/package/swap expressions and unsupported run-on contexts are not generalized. Editable chains require complete AND/OR clauses; read-only scene witnesses may retain a terminal OR. Public-list references remain compatible with their ordinary role; direct track-script references remain protected.

At DataLoaded, the planner reads enabled plugins and configurations. Each playlist batch prepares additions, local conditions and normalized audio together, reserves capacity, validates witnesses/resources, then commits. Automatic work must match the winning plugin snapshot; pure explicit MTD operations use the loaded list. Both recheck their actual list before committing. Scene-dependent failure retries independent work; if automatic preparation still fails, valid explicit instructions can run through the same prepare/commit path without inference. A failed original-audio candidate alone can be omitted without blocking valid guarded contributions. Exceptions do not retry.

The DLL reopens enabled plugins through native `TESFile` opening, record/subrecord navigation and data reads. The engine handles compressed records and extended-size fields. Harmonia validates read completion, converts native record-header IDs back to source identities, and submits each complete file to the shared decoder. It retains all relevant versions, rather than only the first or winning definition. The portable file walker and zlib are linked only into the offline inspector. Both paths share CTDA/VMAD decoding, ownership evidence and every music rule; see the [native-reader assessment](AUDIT.md#modex-native-reader-assessment). This still reads plugin history from disk. Audio access is separate: Harmonia has no BSA parser or audio cache.

Explicit area assignments follow, except those whose target playlist failed. Automatic area recovery requires all music batches to succeed; explicit selectors install last. This is not a transaction over every external runtime actor.

If standalone `MusicTypeDistributor.dll`/the MTD plugin identity or `CombatMusic.dll` is loaded, Harmonia pauses its writes. Keep one execution owner. Renamed forks and later external mutations are not controlled.

## Installation and limits

Install `Harmonia-1.0.1.zip` through the mod manager. For original/replacer coexistence, also install `Harmonia-Original-Audio-HQ-1.0.0.zip`. It contains loose independent audio plus a manifest and needs no plugin registration. Without it, only audio coexistence is disabled. Keep each music pack's own plugins, BSA/audio and supported configuration files. Do not install the inspector/source archive as a gameplay dependency. No generator or additional patch ESP is required. Existing authored compatibility patches should be retained unless their purpose has been verified as covered. Legacy 0.11 audio caches are ignored; this release does not delete them. MRR is not a dependency.

Version 0.7 narrows runtime support from 0.6: **1.7.99, 1.6.x, GOG and VR are not enabled**. Static inspection of 1.7.104 found the pinned SDK declares BGSMusicTrack's condition holder as an inline object although the executable uses a pointer. This release accesses that field through a dedicated adapter and the native single-track factory. That fix and its static evidence are documented in validation; they do not constitute an in-game test.

The current **111-plugin combination**, including ECSS, Bruma, SIRENROOT, Chapter II, Northerner Diaries, Musical Lore, Lorkhan and paid Creations, was inspected with its unmodified configuration. All **195 independently identified missing playlist memberships** are covered by the new plan: 15 town, 11 default battle, five Boss and 164 other uses. Five ordinary combat tracks inherit the authored Bruma/Orsinium exclusions locally. ECSS's five, Bruma's 20 and SIRENROOT's 29 independent types remain unchanged. Actual Lorkhan resources yield original/replacement proposals for **60 original tracks across 244 playlist uses with the optional pack**. These are offline results, not native playback results.

Temporary configuration overlays against the same 111 plugins also verified explicit Location/Region assignment proposals, town clear/refill, empty default-battle clear, retained independent Boss links and equivalent selection from commented versus original ECSS JSON. No deployed configuration was changed. The suite passes 315 Python test methods, 523 selector checks, 223 native-reader contract checks and 45 read-only executable checks. All six complete inspector outputs match 1.0.0 byte-for-byte. Contract tests use a scripted API double; the game's reader, Address Library resolution and playback have not been executed.

Two existing referenced audio resources are absent locally: Chapter II's `music/forestfall/mus_palette_forestfall_day_b_17.xwm` and Crone's `music/dread/dread 01 v2.wav` (also no XWM equivalent). Harmonia does not create missing music or delete these authors' references. Arbitrary Papyrus music-stack behavior, dynamic lookups, runtime MusicType creation, unsupported conditions and absent custom battle relationships can still need authored configuration or patches. Other pack versions and every Music Patch for New Lands variant were not tested. See validation for exact scope.

## Build and inspect

Visual Studio 2022 C++ tools, Windows SDK, Git, CMake 3.24+ and Python. Dependencies are pinned in `cmake/Dependencies.cmake`; JsonCpp is linked statically and SimpleIni is header-only. Neither adds a separate runtime DLL.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect HarmoniaRuleTests HarmoniaReaderTests --parallel 4
$env:UNIFIED_MUSIC_INSPECT = (Resolve-Path build/Release/UnifiedMusicInspect.exe).Path
python -m unittest discover -s tests -p 'test_*.py' -v
ctest --test-dir build -C Release --output-on-failure
build/Release/UnifiedMusicInspect.exe Skyrim.esm Example.esp --config Example_MUS.ini --selector Example.json
```

For a gameplay-only build, configure with `-DHARMONIA_BUILD_INSPECTOR=OFF -DBUILD_TESTING=OFF` and build `UnifiedMusic`. This configuration does not fetch or build zlib. The DLL link map also excludes the portable reader and zlib when development targets are enabled.

Pass all enabled plugins/masters. `--config-dir Data` and `--selector-dir PATH` discover the corresponding root files. The inspector does not change plugins or discover a game load order. Output proposals include `APPEND`, `AUTO_COMBAT`, `CLEAR`, `DISTRIBUTE`, `ASSIGN`, `CONDITION_UNION`, `CONDITION_GUARD`, `ISOLATE_GUARD`, `RESTORE_MAPPING` and `SELECTOR`; `USE_RULES`/`AUDIO_USE` JSON lines expose inferred clauses and audio hashes for offline comparisons. Those clauses describe additions to authored eligibility, not a complete playback trace. IDs after the colon are decimal local IDs. Geographic kinds: 0 Cell, 1 Worldspace, 2 Location, 3 Region. Exit 0 means scanning completed; read handoff/skip diagnostics.

Optional resource rehearsal accepts `--visible-dir RESOLVED_LOOSE_OVERLAY` and `--audio-pack PACK_ROOT`. The overlay must supply the relevant resolved game resources; the pack root supplies its manifest and independent audio. `--visible-dir` alone checks scene resources without enabling coexistence. It models verified files, **not engine VFS/BSA precedence**, and writes no audio. Retired archive/cache options are rejected. `tools/verify_native_layout.py` is a separate read-only executable probe requiring pefile/capstone.

## License

AGPL-3.0-or-later. CME-derived entry points and MTD syntax attribution, pinned source revisions and retained notices are listed in [PROVENANCE.md](PROVENANCE.md). Code/source archives contain no music. The separate optional resource archive and its reuse terms are documented in [audio-pack/README.md](audio-pack/README.md). Paid Creation, Lorkhan and regional-mod assets are not redistributed.
