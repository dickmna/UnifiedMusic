# Harmonia — composer-aware music compatibility

**Harmonia 0.8.0-alpha** restores supported music contributions, applies a practical scene-takeover default to ordinary uses, isolates shared tracks, and lets supported original/replacement audio coexist. One SKSE DLL; no patch-generation interface or generated ESP. [中文说明](使用说明.md).

The DLL remains `UnifiedMusic.dll` for upgrades. This build supports **Steam Skyrim 1.7.104 only**, with matching SKSE and Address Library. It does not require separate MTD or CME DLLs. Offline validation is complete; native playback has not been tested. See [VALIDATION.md](VALIDATION.md).

## Composer policy

The unit of compatibility is **a track's use in one playlist**. Music A and B enrich their authored ordinary playlists. When scene C supplies scoped music and a related plugin excludes ordinary music in the matching scope, Harmonia treats that as scene takeover by default. It no longer requires every original song to be edited. This is an explicit player policy, not a claim to prove every author's intent.

Classification uses original MUSC membership, MUST conditions, dependencies, explicit configurations and typed script properties. It does not classify audio by filename or invent assignments for unassigned tracks. Explicit dependent removals, changed playlist settings, positive eligibility and per-song reversions take precedence.

The small rule set is:

1. Restore independent authored contributions to eligible ordinary lists; retain current order and multiplicity. A reference to public MUSCombat/ambient lists does not by itself establish exclusive script ownership.
2. Pair a scoped song with a matching standalone ordinary exclusion from the same source/dependency family. Sharing the scene song with a dedicated playlist does not erase that authored ordinary use; the scene song is never modified.
3. Preserve the **full supported scene scope**. For `world C AND quest stage >= 40 AND stage < 50`, an ordinary addition gets `outside C OR stage < 40 OR stage >= 50`. It is not banned from C throughout the quest. Supported scopes use AND combinations of Cell/Worldspace/Location, quest/global comparisons, time of day and the ordinary negative-dragon predicate, retaining Self/explicit-player context. An isolated exclusion without paired music creates no general takeover rule.
4. Apply guards to stable ordinary uses: unconditional/time-only ambient tracks, or unconditional/default-negative-dragon combat tracks. Existing authors' eligibility remains intact. Unsupported roles and direct track script references are not promoted to generic music.
5. Recognized takeover with unsupported conditions pauses automatic changes for the affected playlist. Missing/invalid scene main or finale, changed runtime witnesses or allocation failures also leave that playlist unchanged; independent playlists can still proceed.

Every inferred condition edit uses a playlist-local native single-track instance, including original tracks. Other playlists, palettes and scripts retain their original wrapper. No new FormID, persistent MusicType, generated ESP or generated JSON is required. Dedicated story/Boss types and positive dragon roles are not automatically enriched. An author-supplied explicit distribution/selector can still target them.

An OR expression, unsupported function, unresolved reference or dangling OR cannot be broadened into a regional rule. Harmonia does not execute Papyrus or try to reconstruct every script's timing. See [VALIDATION.md](VALIDATION.md) for the actual installed-mod result, including a paused Orsinium battle list.

## Distribution and selection

Root `Data/*_MUS.ini` supports the additive MTD subset for existing unambiguous MusicType EditorIDs:

```ini
[General]
MUSCombat = 0x800~Example.esp, 0x801~Example.esp
```

Local hexadecimal IDs have no load-order prefix. Plugin names are case-insensitive; EditorIDs are exact. UTF-8 BOM, whitespace, whole-line comments, repeated keys and multiple files are accepted. Contributions are deduplicated; intentional duplicates already in a plugin are retained. Explicit rules can target custom/Boss/palette/silent types without changing their conditions.

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

The six supported filters are `worldspaces`, `cells`, `locations`, `locationKeywords`, `combatTarget` (NPC bases) and `combatTargetKeywords`. Forms may use an exact, unique EditorID or `Plugin.esp|0xLOCAL`. All required (`AND: true`) filters must match; if optional filters exist, at least one must match. Each filter matches any of its listed forms. Locations include parents, with cycle/depth protection. `isCombatMusic: false` selects the dungeon-cleared channel.

Matching actor/Boss rules outrank geographic rules; within a priority, more matching required filters win. Different targets tied at the highest rank preserve the engine choice. A highest-ranked target that is unavailable/externally active also preserves that choice; it does not silently fall through to generic regional music. Combat and cleared channels do not share stop bookkeeping. Invalid, missing or ambiguous JSON references pause the plan instead of weakening a rule.

This adapter uses six CME-derived entry points, checked before installation; no ActorUpdate polling or CME combat-music-fix option. It is not a complete CME/MTD implementation. Automatic regional guards remain native track conditions and require **no generated JSON**. An independent custom combat library without a recorded/authored relationship still needs an explicit rule or patch.

## Original audio alongside replacers

Supported file replacements become **additional same-role music candidates**. Original and winning replacement audio enter the same playlist preparation/commit path as added tracks and receive the same effective scene rules. A replacer no longer runs through a separate post-pass that can miss those rules.

At the resource level, Harmonia compares an official archive witness with the game-visible winner. It retains the winner's existing path and prepares a local original-audio instance, including the original finale. This is the runtime equivalent of treating the replacer as an addition; it does not rewrite the installed music pack. Effective conditions, cue points and loops are copied. The original cache is still necessary to give two different audio resources two addresses.

The original witness reader is bounded to BSA v105, uncompressed music members and structurally valid XWMA. It does not unpack entire archives. Runtime resource reads use the game resolver for winning files; this is separate from direct original archive access. A visible WAV alternative, unavailable/invalid main or finale, ambiguous original member, changed record audio metadata, nested palette, dedicated target list or unverified runtime path causes a skip.

Original audio is cached locally at `Data/Music/Harmonia/<SHA256>.xwm`, or the manager's corresponding write destination. The root must be writable and the completed cache must be visible to the game. Existing cache files are byte-verified and never overwritten. No existing music file is renamed or replaced. The package contains no audio.

This supports the **original + final winning replacer** case. It cannot recover every overwritten pack in a stack of replacers. Byte differences can also represent re-encoding of the same composition. It does not fingerprint music or deduplicate an independently re-added MRR copy against original cached audio; installing both can give original music extra weight. A full real Lorkhan installation was not tested.

## Other restoration and runtime ownership

- Restore independent additions to eligible official exploration, town, dungeon and tavern lists, and supported ordinary/default-battle contributions.
- Restore independent negative geographic extensions to eligible original uses while preserving the baseline condition prefix, run-on context and dependent reversions. These edits also use private playlist instances.
- Recover one unambiguous independent CELL/WRLD/LCTN music assignment hidden by a baseline override. Only that music field changes; competing assignments, explicit clears and dependent reversions remain untouched. REGN rules are not rewritten.

The shared typed CTDA schema includes functions 18, 56, 58, 59, 74, 67, 310, 359 and the recognized 707 dragon predicate. Unknown functions, arbitrary OR/alias/global-comparison/package/swap expressions and unsupported run-on contexts are not generalized. Public-list references remain compatible with their ordinary role; direct track-script references remain protected.

At DataLoaded, the planner reads enabled plugins and configurations. Each playlist batch prepares additions, local conditions and normalized audio together, reserves capacity, validates winning snapshots/witnesses/resources, then commits. A failed scene batch leaves that playlist unchanged while independent batches can proceed. A failed original-audio candidate alone can be omitted without blocking valid guarded contributions. Area recovery follows if all runtime batches succeeded; explicit selectors install last. This is not a transaction over every external runtime actor.

If standalone `MusicTypeDistributor.dll`/the MTD plugin identity or `CombatMusic.dll` is loaded, Harmonia pauses its writes. Keep one execution owner. Renamed forks and later external mutations are not controlled.

## Installation and limits

Install the release ZIP through the mod manager. Keep each music pack's own plugins, BSA/audio and supported configuration files. Do not install the inspector/source archive as a gameplay dependency. No generator or additional patch ESP is required. Existing authored compatibility patches should be retained unless their purpose has been verified as covered.

Version 0.7 narrows runtime support from 0.6: **1.7.99, 1.6.x, GOG and VR are not enabled**. Static inspection of 1.7.104 found the pinned SDK declares BGSMusicTrack's condition holder as an inline object although the executable uses a pointer. This release accesses that field through a dedicated adapter and the native single-track factory. That fix and its static evidence are documented in validation; they do not constitute an in-game test.

The current 94 plugins produce no automatic edit proposals. The Orsinium scene's complex OR/quest arrangement pauses expansion of public MUSCombat. With actual MRR ESP/INI, 303 unique configuration pairs are valid; 295 pairs across 55 destination types/118 tracks remain executable and 8 default-battle pairs are paused. Twelve absent IDs affect another 32 pairs. There are no inferred guards for that complex scene. “Preserved” does not mean “all conflicts solved”.

Bruma, Extended Cut, Lorkhan, Chapter II and every Music Patch for New Lands variant were not jointly validated. Arbitrary Papyrus music-stack behavior, dynamic lookups, missing audio, advanced MTD operations, complex author conditions and absent custom battle relationships can still need authored configuration or patches. No general tool can recover missing intent from a filename.

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
