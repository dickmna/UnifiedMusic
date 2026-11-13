# Validation — Harmonia 0.7.0-alpha, 2026-11-14

Release x64 DLL, inspector and selector test executable built with VS 2022 / MSVC 19.44, CMake 4.3.3 and pinned dependencies. Python 3.14 executed the integration fixtures. **Steam 1.7.104 is the only enabled runtime.** No Skyrim process was launched.

## Automated checks

**243 Python test methods passed**, including their parameterized subcases; output is in `tests/latest-results.txt`. **523 selector logic checks passed** through CTest and the standalone test target; output is in `tests/latest-selector-results.txt`.

Coverage includes the existing parser/compression/master-remapping, independent contributions, intentional removal, script/Boss/palette protection, condition union, mapping and INI tests, plus:

- Music A + B + land C, with no INI, for ambient and ordinary battle roles; explicit positive regional replacement remains unchanged.
- A shared ordinary contribution with two different ambient policies; each target gets its own isolation proposal. Plugin/config-only Civil War/Boss ownership and palette owners remain on the original object; direct VMAD track references block inferred edits.
- Exact negative-dragon variants including explicit player references; positive dragon predicates preserved. Vanilla references to the public battle list retain its known role; newly introduced mod-script references protect it.
- CME's six form filters, combat/cleared channels, EditorID and local-ID resolution, duplicates, missing/deleted/renamed/wrong-type forms, malformed JSON and unsupported fields. Failed rules cannot turn into broad unconditional selectors.
- All 512 required/optional/matched/actor combinations for three filters, plus priority, deduplication, ambiguity and unavailable-target cases. An unavailable highest-ranked Boss rule cannot fall through to broad geographic music.
- Original/replacement pairs, unchanged audio, main/finale completeness, repeated cache reuse, truncated/malformed BSA, compressed/embedded entries, duplicates, path traversal, visible WAV ambiguity, invalid XWMA, corrupt cache and stale temporary files, cache junction rejection, changed metadata and protected roles.

Synthetic RIFF fixtures prove structural validation and byte handling, not codec decoding. Planner outputs prove proposed decisions, not native pointer mutation or playback. Analytic eligibility tables produced by local rehearsal are illustrative expectations and are not counted as independent engine tests.

## Actual plugin and configuration inspection

All 94 currently enabled plugins were read without changing their files or the profile. Actual Music Replacer Reverter v1 ESP/INI were supplied from a private workspace, outside Data.

| Input | Result |
| --- | --- |
| Current 94 plugins | Zero playlist, battle, guard, mapping or distribution proposals; about 3.2 seconds |
| Current 94 + actual MRR ESP/INI | 303 valid unique distribution pairs; 56 types, 121 tracks; no regional guards; about 3.1 seconds |
| Five official plugins + actual MRR + synthetic supported land | 303 distributions, one regional contribution group, three ordinary-copy guards and two isolated ordinary uses; about 2.5 seconds |

An independent Python config/record comparison matched every valid distribution pair. MRR has 251 MUST records; its INI references 133 distinct IDs, of which 12 are absent, affecting 32 unique destination/track pairs: `083D, 084F, 0850, 0853, 0854, 0855, 0856, 0857, 0858, 0859, 085A, 085B`. These are diagnosed/skipped; no missing record is reconstructed.

The synthetic land adds pure negative worldspace restrictions to five ordinary original battle tracks and provides one pure positive-area replacement. Actual MRR IDs `080E, 081F, 0820` receive guards. IDs `081D, 081E` get playlist-local isolation in MUSCombat; their Civil War uses keep the original wrapper. Dragon/Boss IDs `0821, 0822, 08FA` are unchanged. This is actual MRR against **synthetic geography**, not Bruma/ECSS/quest execution.

In the current 94-plugin combination, mod VMAD ownership of public MUSCombat stops automatic combat inference. Existing Orsinium/quest conditions are not proven as unconditional full-region replacement coverage. Thus MRR distribution succeeds, but regional compatibility is not certified. Preservation and zero proposals are not proof of an absence of conflicts.

All 14 previously audited Creation plugin hashes still match prior evidence. Profile `plugins.txt` and `loadorder.txt` hashes remain unchanged. Private plugin dumps and configuration contents are excluded from the source tree.

## Real original audio with a simulated replacer

Read the local `Skyrim - Sounds.bsa` through the bounded archive reader, with the five official plugins. There were 51 eligible original track/list pairs. With an empty loose overlay, all 51 were unchanged and no cache instances were proposed.

In a private overlay, a different valid stock combat track was placed at Combat 01's path to simulate a file replacer. Exactly one coexistence proposal resulted. Original Combat 01 (643,446 bytes) and its original finale (76,010 bytes) were cached separately and checked against their SHA-256 values; the main cache was also compared byte-for-byte to the archive extraction. Repeated rehearsal produced the same proposal and the same two cache files. The original archive hash and the simulated replacement remained unchanged. No game Data file was written.

This tests real original resources and the supported replacement shape. **It is not a real Lorkhan test**, does not invoke the audio decoder, and the inspector's loose-overlay resolver is not a simulation of the game's complete VFS/BSA precedence. Runtime uses BSResourceNiBinaryStream for the winning resource and verifies cache visibility before installing an instance.

## Native implementation and executable inspection

`tools/verify_native_layout.py` performed 21 read-only checks on Steam SkyrimSE.exe 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. The aggregate report is in `tests/native-layout-results.json`; no bytes/disassembly are distributed.

The pinned SDK exposes BGSMusicTrack::conditions as an inline TESCondition, but this executable dereferences a **TESCondition pointer at offset 0x10**, separately allocating an 8-byte holder. The previous 0.6 field access was therefore incompatible with this layout; 0.7 centralizes the corrected access. This is a material fix found by inspection, not an in-game reproduction of a crash.

Additional checks establish the 0x80 native single-track factory allocation, fresh main/finale sound handles, loop-pointer initialization, and MusicType dispatch through the BSIMusicTrack interface for condition checks, play, update and status. The six CME-derived call offsets all lead to the same default-object lookup in the inspected executable. Runtime separately resolves Address Library IDs and checks those sites before writing hooks. **Address Library resolution and native factory calls were not executed offline.**

Runtime preparation validates winning list multisets, typed conditions and replacement witnesses, allocates missing nodes/independent instances, reserves space, rechecks, then commits. Shared instances use native deep condition copy; other owners retain the original wrapper. Original audio has a separate bounded preparation and commit stage. Detected standalone MTD/CME pauses processing. Source review and compilation support these observations; offline tests do not execute the native stages.

## Remaining boundaries

- Native allocations/destruction, hook timing, audio playback, native random conditions, selection probabilities, combat end, cell transitions and save/reload remain untested. No claim of crash-free universal compatibility is made.
- Full Bruma, Extended Cut, Lorkhan, Chapter II or all Nexus 89099 variants were not installed and jointly tested.
- Advanced MTD operations, missing records/audio, independent custom combat relationships, arbitrary Papyrus changes, complex OR/quest/weather policies and later external DLL changes can require explicit configuration or authored patches.
- Original shared battle-track exclusion unions still use conservative ownership rules. The new isolated-guard path covers eligible added/copied ordinary singles; it does not infer every possible original shared-policy conflict or flatten palettes.
- The original-audio layer adds original + final winner only. Multiple losing replacers are not recovered; byte differences can be re-encodings, and MRR copies are not content-deduplicated against original caches.
- Current resource limits, protected predicates or a failed live snapshot may produce fewer runtime actions than offline proposals. Failure diagnostics are part of the supported behavior.

Earlier 0.4 condition rehearsals and 0.6 results remain in their historical tags. They are not counted as current native playback validation.
