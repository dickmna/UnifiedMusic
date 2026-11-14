# Validation — Harmonia 0.9.0-alpha, 2026-11-14

Release x64 DLL, inspector and selector test executable built with VS 2022 / MSVC 19.44, CMake 4.3.3 and pinned dependencies. Python 3.14 executed the integration fixtures. **Steam 1.7.104 is the only enabled runtime.** No Skyrim process was launched.

## Automated checks

**282 Python test methods passed**, including parameterized subcases (`tests/latest-results.txt`). **523 selector logic checks passed**, with CTest reporting one passing test target (`tests/latest-selector-results.txt`). These totals count checks differently and are not added into a single engine-test number.

Coverage includes bounded plugin/BSA parsing, compressed records, master remapping, independent contributions, deliberate dependent removal, explicit configuration, protected roles, field-only mapping repair, audio cache verification and:

- A related scoped song and one matching ordinary donor can establish a scene policy without a count threshold. Two excluded songs without a corresponding cue, or an unrelated source using the same area, do not establish it. Removing the only valid evidence stops inference. A dependent per-song removal retains that exception.
- Independent truth tables evaluate complete authored exclusion clauses for world/quest intervals, world/cell and world/time scopes. Self and explicit-player contexts stay distinct. A cue's narrower quest gates do not replace an author's broader ordinary-track restriction.
- Original baseline clauses can be reordered. Donor baseline day/night and dragon eligibility do not propagate; new standalone clock restrictions also remain song-specific. Only full clauses common to matching donors of one authoring source are inherited. Differently conditional OR exclusions cannot be flattened into an unconditional area ban.
- A scene cue's terminal OR is retained as read-only evidence and never edited. Missing quest references, unknown cue functions or missing scene audio pause ordinary uses while independent dragon distributions remain. Another ordinary list can still restore contributions.
- Original and final-winning replacement hashes are distinct; normalized audio and a standalone addition receive identical inferred clauses. A repeated run yields the same plan and exact cache bytes. Main/finale loss pauses the scene-dependent actions together.
- Every inferred native condition edit is local to one playlist. Planner fixtures cover original/shared tracks, config-only Civil War/Boss owners, multiple ambient policies, palette ownership and explicit positive mixing. Scene songs shared with dedicated owners remain untouched.
- Typed VMAD scalar/array object properties, both object formats and quest aliases protect actual references. Integer/float/string bytes matching a FormID do not establish ownership; removed properties are ignored. Unknown structures keep conservative fallback protection. Public list references retain their known ordinary role.
- CME's six filters, combat/cleared channels, strict form resolution, priority, ambiguity, unavailable targets and all 512 three-filter required/optional/matched/actor combinations remain covered. Highest-ranked Boss failure does not fall through to generic regional music.

Synthetic RIFF fixtures prove structural validation and byte handling, not codec decoding. The truth tables evaluate planner output independently; the old scene-negation implementation has been removed. These checks do not execute Skyrim's evaluator. Source/ownership review and compilation do not substitute for native execution.

## Actual installed plugins and configuration

All **94 enabled plugins** were inspected without changing them or the active profile: five official base plugins, 75 `Skyrim.ccc` entries and 14 enabled `plugins.txt` entries; their union matches the load order. Actual Music Replacer Reverter v1 ESP/INI were supplied from a private workspace outside Data. Aggregate results are in `tests/real-input-results.json`; proprietary records and audio are excluded from the repository.

| Input | Result |
| --- | --- |
| Current 94 plugins | Zero playlist/condition/mapping/distribution commits proposed; existing authored ordinary restrictions already present |
| Current 94 + actual MRR | All 303 valid unique config pairs produce executable proposals across 56 types/121 tracks; three ordinary guards and two isolated ordinary uses; no default-battle pairs paused |
| Five official plugins + actual MRR + synthetic supported land | 303 distributions, one regional contribution group, three ordinary-copy guards, two isolated ordinary uses |

An independent Python record/config comparison checks every expected valid pair. MRR's INI references 133 distinct IDs; 12 are absent and affect 32 unique destination/track pairs: `083D, 084F, 0850, 0853, 0854, 0855, 0856, 0857, 0858, 0859, 085A, 085B`. They are diagnosed/skipped rather than reconstructed.

The current Orsinium cue combines geographic and quest conditions with OR grouping, including a trailing OR. The planner retains this cue unchanged and inherits the three added ordinary-track clauses: `GetInCell(legacyorsinium.esm|06E9CC) != 1` on the explicit Player reference, `GetStage(legacyorsinium.esm|014C29) != 40` on Self, and `GetInWorldspace(legacyorsinium.esm|085C50) == 0` on Self. The existing negative-dragon predicate remains each ordinary track's own eligibility; it is not an inherited scene clause. Read-only donor evidence permits irrelevant audio metadata differences, while target mutation still requires stable metadata.

MRR IDs `080E, 081D, 081E, 081F, 0820` receive these clauses. `081D, 081E` use private ordinary-battle instances while their Civil War uses retain the original wrappers. Dragon IDs `0821, 0822, 08FA` retain their conditions and receive no scene guards. All eight previously paused pairs now produce proposals without a mod-name exception or rewriting the cue's OR expression.

An independent evaluator compared the actual authored ordinary conditions against baseline eligibility plus generated clauses in **120 ordinary cases**, and checked the three unchanged dragon roles in **72 cases**: 192 cases passed. Cases vary room, world, quest stages 39/40/41 and dragon target status; some combinations are deliberately hypothetical. This verifies the planner's inheritance, not the cue's Boolean interpretation, quest execution or the game's condition evaluator.

The actual Orsinium main (1,568,654 bytes) and finale (196,758 bytes) were read from its local BSA into a private overlay. Resource-aware inspection produced the same guarded/distribution plan with no scene-resource skips. The archive hash remained unchanged. This checks structural resources and plan preparation; it does not decode/play the music or validate MRR audio playback.

The synthetic land independently yields the same ordinary/shared/dragon split with its own simple geographic restriction. The synthetic geography is not Bruma/ECSS execution.

All 14 previously audited Creation plugin hashes match prior evidence; profile `plugins.txt` and `loadorder.txt` hashes are unchanged. One unsupported VMAD block retains conservative reference fallback. No parser fallback is presented as proof of script behavior.

## Real original audio with a simulated replacer

Read the local `Skyrim - Sounds.bsa` through the bounded reader with five official plugins. **243 eligible original track/list pairs** were unchanged with an empty overlay; no cache instances were proposed. The wider count than 0.7 reflects ordinary shared/public-list uses now being permitted.

A different valid stock combat track was placed at Combat 01's path in a private overlay. Exactly one coexistence proposal resulted. Original Combat 01 (643,446 bytes) and its original finale (76,010 bytes) were cached separately and verified by SHA-256 and exact bytes. Repeated rehearsal produced the same proposal and two cache files. Archive hashes and the replacement overlay remained unchanged. No game Data file was written.

This uses real original resources and a simulated replacement shape. **It is not a real Lorkhan test**, does not invoke the audio decoder and does not simulate the full manager/game VFS or BSA precedence. Runtime uses BSResourceNiBinaryStream for the actual winning resource and checks completed cache visibility.

## Native implementation and executable inspection

`tools/verify_native_layout.py` passed **21 read-only checks** on Steam SkyrimSE.exe 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Aggregate evidence is in `tests/native-layout-results.json`; no executable bytes/disassembly are distributed.

The SDK's inline-looking conditions field is accessed as the executable's TESCondition pointer at offset 0x10. Checks cover the separate 8-byte holder, 0x80 native single factory, fresh sound handles/loop pointer, MusicType virtual dispatch and six CME call sites. Address Library resolution, factory calls and condition evaluation are not executed offline.

The runtime prepares one playlist batch at a time. It validates the winning list, typed conditions, scene witnesses and main/finale resources; creates private condition instances; prepares original-audio versions from the same effective instances; reserves capacity and rechecks before committing. Other owners retain original objects. An explicit pre-commit failure leaves those scene-dependent uses unchanged, removes their planned actions and retries the independent remainder, such as dragon distributions. Exceptions do not trigger a retry. Validated cache files may remain and earlier independent batches may already be committed. There is one DataLoaded application per process. Area recovery is deferred if any runtime music batch fails. The offline inspector exercises the shared scene-removal planner, not native allocation failure recovery.

## Remaining boundaries

- Native allocation/destruction, hook timing, playback, condition evaluation, random selection, combat end, transitions and save/reload remain untested.
- Full Bruma, Extended Cut, Lorkhan, Chapter II and all Nexus 89099 variants were not installed/jointly tested.
- Unknown functions, unresolved/alias conditions, incomplete editable clauses, direct script mutation, independent custom combat libraries and unsupported MTD operations can need explicit rules, original-framework setups or authored patches. Complete supported OR clauses and read-only terminal-OR cues are handled as described above. Harmonia pauses when standalone MTD/CME is loaded; they are not co-executed.
- Supported audio normalization includes the original and final visible winner, not every losing replacer. Changed record audio metadata, palette restructuring, compressed/unsupported original audio and invalid resources are not reconstructed. Re-encodings and MRR copies are not composition-deduplicated; extra candidates alter playback probabilities.
- Inferred takeover is a deliberate default. It cannot establish universal author intent or universal community compatibility. Explicit exceptions take priority; unreadable/unsupported cases are diagnosed.

Historical 0.7/0.8 results remain in their tags and are not counted as current native playback validation.
