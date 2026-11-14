# Harmonia — composer-aware music compatibility

**Harmonia 0.10.0-alpha** restores supported music contributions, inherits authored scene restrictions for ordinary uses, isolates shared tracks, and lets supported original/replacement audio coexist. One SKSE DLL; no patch-generation interface or generated ESP. [中文说明](使用说明.md).

The DLL remains `UnifiedMusic.dll` for upgrades. This build supports **Steam Skyrim 1.7.104 only**, with matching SKSE and Address Library. It does not require separate MTD or CME DLLs. Offline validation is complete; native playback has not been tested. See [VALIDATION.md](VALIDATION.md).

## Composer policy

The unit of compatibility is **a track's use in one playlist**. Music A and B enrich their authored ordinary playlists. When scene C supplies scoped music and a related plugin excludes ordinary music in the matching scope, Harmonia treats that as a scene arrangement. The author's existing ordinary-track restrictions supply the inherited rule. There is no song-count threshold. This is an explicit player policy, not a claim to prove every author's intent.

Classification uses original MUSC membership, MUST conditions, dependencies, explicit configurations and typed script properties. An official area assignment can establish ordinary use for an otherwise test-named town/exploration list; the word “Test” alone is not proof that a playlist is unused. Dedicated, silent and one-shot roles remain protected. Audio filenames do not establish a role. Explicit dependent removals, positive eligibility and per-song reversions take precedence.

The small rule set is:

1. Restore independent authored contributions to eligible ordinary lists, including original tracks deliberately reused by another mod; retain current order and multiplicity. A reference to public MUSCombat/ambient lists does not by itself establish exclusive script ownership. Battle priority/fade settings remain the winning plugin's fields and do not suppress membership restoration.
2. Pair a scoped song with a matching ordinary exclusion from the same source/dependency family. One matching donor can establish a policy; multiple unrelated exclusions cannot. Sharing the scene song with a dedicated playlist does not erase its authored ordinary use. The scene song is never modified.
3. Extract **complete added condition clauses** from ordinary donors, removing their original eligibility. Within each authoring source, inherit only clauses common to its matching donors. Keep AND/OR grouping, quest/global comparisons and Self/explicit-player context. A donor's original night-only or negative-dragon condition is not copied; newly added standalone clock restrictions also remain song-specific. Time terms inside a scene/quest OR clause remain intact. Do not negate or reconstruct the scene song's playback expression.
4. Apply these rules to stable ordinary uses: unconditional/time-only ambient tracks, or unconditional/default-negative-dragon combat tracks. For example, an authored ordinary rule `outside C OR quest stage < 40 OR stage >= 50` is inherited as written. If the author instead excludes ordinary music from all of C, that broader authored restriction is retained even if a Boss cue has narrower gates. Explicit positive assignments and dependent per-song reversions take priority.
5. Unreadable scene evidence or missing/invalid scene main/finale pauses scene-dependent ordinary actions. Independent dragon/dedicated distributions can remain. At runtime, an explicit pre-commit failure removes those dependent actions and retries the independent remainder; exceptions do not trigger a retry. No unverified rule is generalized.

Every inferred condition edit uses a playlist-local native single-track instance, including original tracks. Other playlists, palettes and scripts retain their original wrapper. No new FormID, persistent MusicType, generated ESP or generated JSON is required. Dedicated story/Boss types and positive dragon roles are not automatically enriched. An author-supplied explicit distribution/selector can still target them.

Layered music frameworks may define an empty track before a dependent region plugin supplies its audio and conditions. Harmonia uses the dependency-maximal definitions, requiring agreement between competing final branches. An already-authored battle membership can be restored with its complete supported read-only conditions intact, without inventing a geographic selector. If the same contributed track also has an explicit membership in another original Skyrim playlist, that exact link can be restored there too, including Boss/Civil War uses; the general music pool is never copied into those lists. Deleted/one-shot lists and dependent removals remain protected. Scene matching can follow an unambiguous Location parent chain; cycles, changed parents and unresolved ancestry do not invent coverage.

Complete supported OR clauses can be inherited. A scene song's terminal OR flag can be retained as read-only evidence: no conditions are appended to that song and its expression is never inverted. Unknown functions, unresolved references and unsupported edits remain boundaries. Harmonia does not execute Papyrus or reconstruct every script's timing. See [VALIDATION.md](VALIDATION.md) for the current installed combination and authored Bruma/Orsinium rules.

## Distribution and selection

Root `Data/*_MUS.ini` supports the additive MTD subset for existing unambiguous MusicType EditorIDs:

```ini
[General]
MUSCombat = 0x800~Example.esp, 0x801~Example.esp
```

Local hexadecimal IDs have no load-order prefix. Plugin names and ASCII EditorIDs are case-insensitive. Case-folded EditorIDs must identify one form; collisions, genuine renames and deleted targets remain rejected. UTF-8 BOM, whitespace, whole-line comments, repeated keys and multiple files are accepted. Contributions are deduplicated; intentional duplicates already in a plugin are retained. Explicit rules can target custom/Boss/palette/silent types without changing their conditions.

Missing/deleted/invalid tracks and deliberately removed contributions produce `CONFIG_SKIP`. Unsupported clears (`!`), runtime type creation, Location/Region assignment, unknown sections, malformed syntax, UTF-16 and ambiguous target types pause **all automatic writes** with `CONFIG_HANDOFF`. Inline comments and multiline values are outside this subset.

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

Matching actor/Boss rules outrank geographic rules; within a priority, more matching required filters win. Different targets tied at the highest rank preserve the engine choice. A highest-ranked target that is unavailable/externally active also preserves that choice; it does not silently fall through to generic regional music. Combat and cleared channels do not share stop bookkeeping. Invalid, missing or ambiguous JSON references pause the plan instead of weakening a rule.

This adapter uses six CME-derived entry points, checked before installation; no ActorUpdate polling or CME combat-music-fix option. It is not a complete CME/MTD implementation. Automatic regional guards remain native track conditions and require **no generated JSON**. An independent custom combat library without a recorded/authored relationship still needs an explicit rule or patch.

## Original audio alongside replacers

Supported file replacements become **additional same-role music candidates**. Original and winning replacement audio enter the same playlist preparation/commit path as added tracks and receive the same effective scene rules. A replacer no longer runs through a separate post-pass that can miss those rules.

At the resource level, Harmonia compares an official archive witness with the game-visible winner. It retains the winner's existing path and prepares a local original-audio instance, including the original finale. This is the runtime equivalent of treating the replacer as an addition; it does not rewrite the installed music pack. Effective conditions, cue points and loops are copied. The original cache is still necessary to give two different audio resources two addresses.

The original witness reader is bounded to BSA v105, uncompressed music members and structurally valid XWMA. It does not unpack entire archives. Runtime resource reads use the game resolver for winning files; this is separate from direct original archive access. A visible WAV alternative, unavailable/invalid main or finale, ambiguous original member, changed record audio metadata, nested palette, dedicated target list or unverified runtime path causes a skip.

Original audio is cached locally at `Data/Music/Harmonia/<SHA256>.xwm`, or the manager's corresponding write destination. The root must be writable and the completed cache must be visible to the game. Existing cache files are byte-verified and never overwritten. No existing music file is renamed or replaced. The package contains no audio.

This supports the **original + final winning replacer** case. It cannot recover every overwritten pack in a stack of replacers. Byte differences can also represent re-encoding of the same composition. It does not fingerprint music or deduplicate an independently re-added MRR copy against original cached audio; installing both can give original music extra weight. The installed Lorkhan resources were checked offline; native playback remains untested.

## Other restoration and runtime ownership

- Restore independent additions to eligible official exploration, town, dungeon and tavern lists, and supported ordinary/default-battle contributions.
- Restore independent negative geographic extensions to eligible original uses while preserving baseline conditions, run-on context and dependent reversions. These edits also use private playlist instances.
- Recover one unambiguous independent CELL/WRLD/LCTN music assignment hidden by a baseline override. Only that music field changes; competing assignments, explicit clears and dependent reversions remain untouched. REGN rules are not rewritten.

The shared typed CTDA schema includes functions 18, 56, 58, 59, 74, 67, 310, 359 and 707. Read-only authored tracks/witnesses also support 117 (Region), 562 (Location keyword) and 520 (platform predicate, no arguments); those extra predicates are preserved and are not copied into inferred guards. Unknown functions, alias/global-comparison/package/swap expressions and unsupported run-on contexts are not generalized. Editable chains require complete AND/OR clauses; read-only scene witnesses may retain a terminal OR. Public-list references remain compatible with their ordinary role; direct track-script references remain protected.

At DataLoaded, the planner reads enabled plugins and configurations. Each playlist batch prepares additions, local conditions and normalized audio together, reserves capacity, validates winning snapshots/witnesses/resources, then commits. Scene-dependent failure leaves those uses unchanged; independent roles in the same playlist can be retried and independent playlists can proceed. A failed original-audio candidate alone can be omitted without blocking valid guarded contributions. Area recovery follows if all runtime batches succeeded; explicit selectors install last. This is not a transaction over every external runtime actor.

If standalone `MusicTypeDistributor.dll`/the MTD plugin identity or `CombatMusic.dll` is loaded, Harmonia pauses its writes. Keep one execution owner. Renamed forks and later external mutations are not controlled.

## Installation and limits

Install the release ZIP through the mod manager. Keep each music pack's own plugins, BSA/audio and supported configuration files. Do not install the inspector/source archive as a gameplay dependency. No generator or additional patch ESP is required. Existing authored compatibility patches should be retained unless their purpose has been verified as covered.

Version 0.7 narrows runtime support from 0.6: **1.7.99, 1.6.x, GOG and VR are not enabled**. Static inspection of 1.7.104 found the pinned SDK declares BGSMusicTrack's condition holder as an inline object although the executable uses a pointer. This release accesses that field through a dedicated adapter and the native single-track factory. That fix and its static evidence are documented in validation; they do not constitute an in-game test.

The current **111-plugin combination**, including ECSS, Bruma, SIRENROOT, Chapter II, Northerner Diaries, Musical Lore, Lorkhan and paid Creations, was inspected with its unmodified configuration. All **195 independently identified missing playlist memberships** are covered by the new plan: 15 town, 11 default battle, five Boss and 164 other uses. Five ordinary combat tracks inherit the authored Bruma/Orsinium exclusions locally. ECSS's five, Bruma's 20 and SIRENROOT's 29 independent types remain unchanged. Actual Lorkhan resources yield original/replacement proposals for **65 original tracks across 249 playlist uses**. These are offline results, not native playback results.

Two existing referenced audio resources are absent locally: Chapter II's `music/forestfall/mus_palette_forestfall_day_b_17.xwm` and Crone's `music/dread/dread 01 v2.wav` (also no XWM equivalent). Harmonia does not create missing music or delete these authors' references. Arbitrary Papyrus music-stack behavior, dynamic lookups, advanced MTD operations, unsupported conditions and absent custom battle relationships can still need authored configuration or patches. Other pack versions and every Music Patch for New Lands variant were not tested. See validation for exact scope.

## Build and inspect

Visual Studio 2022 C++ tools, Windows SDK, Git, CMake 3.24+ and Python. Dependencies are pinned in `cmake/Dependencies.cmake`; JsonCpp is linked statically for the small JSON adapter. No SimpleIni dependency.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target UnifiedMusic UnifiedMusicInspect HarmoniaRuleTests --parallel 4
$env:UNIFIED_MUSIC_INSPECT = (Resolve-Path build/Release/UnifiedMusicInspect.exe).Path
python -m unittest discover -s tests -p 'test_*.py' -v
ctest --test-dir build -C Release --output-on-failure
build/Release/UnifiedMusicInspect.exe Skyrim.esm Example.esp --config Example_MUS.ini --selector Example.json
```

Pass all enabled plugins/masters. `--config-dir Data` and `--selector-dir PATH` discover the corresponding root files. The inspector does not change plugins or discover a game load order. Output proposals include `APPEND`, `AUTO_COMBAT`, `DISTRIBUTE`, `CONDITION_UNION`, `CONDITION_GUARD`, `ISOLATE_GUARD`, `RESTORE_MAPPING` and `SELECTOR`; `USE_RULES`/`AUDIO_USE` JSON lines expose inferred clauses and audio hashes for offline comparisons. Those clauses describe additions to authored eligibility, not a complete playback trace. IDs after the colon are decimal local IDs. Geographic kinds: 0 Cell, 1 Worldspace, 2 Location. Exit 0 means scanning completed; read handoff/skip diagnostics.

Optional audio rehearsal accepts repeated `--original-archive PATH`, `--visible-dir LOOSE_OVERLAY`, and optional `--audio-cache EXISTING_DIRECTORY`. It models a loose overlay over supplied original archives, **not runtime VFS/BSA precedence**. Only `--audio-cache` writes files; use a private test directory. `tools/verify_native_layout.py` is a separate read-only executable probe requiring pefile/capstone.

## License

AGPL-3.0-or-later. CME-derived entry points and MTD syntax attribution, pinned source revisions and retained notices are listed in [PROVENANCE.md](PROVENANCE.md). No game, paid Creation or downloaded music assets are distributed.
