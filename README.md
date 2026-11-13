# Harmonia — composer-aware music compatibility

**Harmonia 0.7.0-alpha** restores supported music contributions, carries proven regional exclusions to ordinary additions, isolates shared tracks, and lets supported original/replacement audio coexist. One SKSE DLL; no patch-generation interface or generated ESP. [中文说明](使用说明.md).

The DLL remains `UnifiedMusic.dll` for upgrades. This build supports **Steam Skyrim 1.7.104 only**, with matching SKSE and Address Library. It does not require separate MTD or CME DLLs. Offline validation is complete; native playback has not been tested. See [VALIDATION.md](VALIDATION.md).

## Composer policy

A music pack A and a music pack B contribute to their **authored playlists**. An independent override hiding A does not erase its contribution. If land C consistently excludes the original ordinary music and provides a full-area replacement in that same playlist, supported ordinary additions from A and B inherit that exclusion. C's own replacement remains conditional. Dedicated story/Boss playlists and complex expressions retain their original ownership.

Classification uses MUSC membership, MUST conditions, dependencies, explicit configurations, script references and shared owners. It does not guess intent from filenames, musical style, or words such as “Boss”. Unassigned new tracks stay unassigned. Deliberate dependent removals, changed playlist settings and ambiguous ownership stop inference.

A new geographic guard requires all of these:

1. Original ordinary tracks unanimously exclude the area while retaining their baseline conditions. Shared/read-only originals must retain the exclusion in every dependency-maximal version that could win.
2. A stable, unshared single-track replacement in the same current/planned list has exactly one positive Self geographic condition covering that area. A quest-gated or OR expression does not prove full-area coverage.
3. The addition is stable and ordinary: unconditional ambient music, or unconditional/default-negative-dragon battle music. Direct script references block inferred edits.
4. Live winning playlists, condition witnesses and required replacement tracks still match the scan before commit.

For an eligible shared single, Harmonia creates a private native instance **only in the affected playlist**, deep-copies its conditions, then appends the exclusion. Other playlists and palettes retain the original wrapper. No new FormID or persistent MusicType is created. Positive dragon/Boss predicates and unknown roles are never converted to ordinary music.

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

Playlist conflicts and same-path audio replacement are separate. For supported original single tracks in proven ordinary lists, Harmonia compares an official archive witness with the resource currently visible to the game. If bytes differ, it keeps the winning replacement and appends an independent instance pointing to cached original audio, including its original finale. Current conditions, cue points and loops are copied.

The original witness reader is bounded to BSA v105, uncompressed music members and structurally valid XWMA. It does not unpack entire archives. Runtime resource reads use the game resolver for winning files; this is separate from direct original archive access. A visible WAV alternative, unavailable/invalid main or finale, ambiguous original member, changed record audio metadata, nested palette, dedicated owner or unverified runtime path causes a skip.

Original audio is cached locally at `Data/Music/Harmonia/<SHA256>.xwm`, or the manager's corresponding write destination. The root must be writable and the completed cache must be visible to the game. Existing cache files are byte-verified and never overwritten. No existing music file is renamed or replaced. The package contains no audio.

This supports the **original + final winning replacer** case. It cannot recover every overwritten pack in a stack of replacers. Byte differences can also represent re-encoding of the same composition. It does not fingerprint music or deduplicate an independently re-added MRR copy against original cached audio; installing both can give original music extra weight. A full real Lorkhan installation was not tested.

## Other restoration and runtime ownership

- Restore independent additions to eligible official exploration, town, dungeon and tavern lists, and supported ordinary/default-battle contributions.
- Restore independent negative geographic extensions to eligible original battle tracks while preserving the baseline condition prefix and dependent reversions.
- Recover one unambiguous independent CELL/WRLD/LCTN music assignment hidden by a baseline override. Only that music field changes; competing assignments, explicit clears and dependent reversions remain untouched. REGN rules are not rewritten.

The supported typed CTDA schema includes functions 56, 58, 59, 74, 67, 310, 359 and the recognized 707 dragon predicate. Unknown functions, OR/alias/global-comparison/package/swap expressions and unsupported run-on contexts are not generalized. Ordinary vanilla scripts referencing public MUSCombat are expected; a new mod-script reference protects the battle list. Direct track-script references remain protected.

At DataLoaded, the planner reads enabled plugins and configurations. It prepares additions, native instances and condition nodes, reserves capacity, validates winning snapshots/witnesses, then commits the music batch. Area recovery follows a successful batch. Original-audio coexistence has its own prepare/recheck/commit stage; a failed audio candidate leaves playlist restoration available. Optional explicit selectors install last. It is not a transaction over every external runtime actor.

If standalone `MusicTypeDistributor.dll`/the MTD plugin identity or `CombatMusic.dll` is loaded, Harmonia pauses its writes. Keep one execution owner. Renamed forks and later external mutations are not controlled.

## Installation and limits

Install the release ZIP through the mod manager. Keep each music pack's own plugins, BSA/audio and supported configuration files. Do not install the inspector/source archive as a gameplay dependency. No generator or additional patch ESP is required. Existing authored compatibility patches should be retained unless their purpose has been verified as covered.

Version 0.7 narrows runtime support from 0.6: **1.7.99, 1.6.x, GOG and VR are not enabled**. Static inspection of 1.7.104 found the pinned SDK declares BGSMusicTrack's condition holder as an inline object although the executable uses a pointer. This release accesses that field through a dedicated adapter and the native single-track factory. That fix and its static evidence are documented in validation; they do not constitute an in-game test.

The currently installed 94 plugins produce no inferred edits. With actual MRR ESP/INI, the planner resolves 303 unique distribution pairs (56 types, 121 tracks), reports 12 absent IDs affecting 32 pairs, and produces no regional guards. New mod-script ownership protects automatic battle inference; existing complex regional/quest policies are not proven. “Preserved” does not mean “all conflicts solved”.

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

Pass all enabled plugins/masters. `--config-dir Data` and `--selector-dir PATH` discover the corresponding root files. The inspector does not change plugins or discover a game load order. Output proposals include `APPEND`, `AUTO_COMBAT`, `DISTRIBUTE`, `CONDITION_UNION`, `CONDITION_GUARD`, `ISOLATE_GUARD`, `RESTORE_MAPPING` and `SELECTOR`; IDs after the colon are decimal local IDs. Geographic kinds: 0 Cell, 1 Worldspace, 2 Location. Exit 0 means scanning completed; read handoff/skip diagnostics.

Optional audio rehearsal accepts repeated `--original-archive PATH`, `--visible-dir LOOSE_OVERLAY`, and optional `--audio-cache EXISTING_DIRECTORY`. It models a loose overlay over supplied original archives, **not runtime VFS/BSA precedence**. Only `--audio-cache` writes files; use a private test directory. `tools/verify_native_layout.py` is a separate read-only executable probe requiring pefile/capstone.

## License

AGPL-3.0-or-later. CME-derived entry points and MTD syntax attribution, pinned source revisions and retained notices are listed in [PROVENANCE.md](PROVENANCE.md). No game, paid Creation or downloaded music assets are distributed.
