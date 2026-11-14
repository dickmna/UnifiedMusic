# Validation — Harmonia 0.8.0-alpha, 2026-11-14

Release x64 DLL, inspector and selector test executable built with VS 2022 / MSVC 19.44, CMake 4.3.3 and pinned dependencies. Python 3.14 executed the integration fixtures. **Steam 1.7.104 is the only enabled runtime.** No Skyrim process was launched.

## Automated checks

**270 Python test methods passed**, including parameterized subcases (`tests/latest-results.txt`). **523 selector logic checks passed**, with CTest reporting one passing test target (`tests/latest-selector-results.txt`). These totals count checks differently and are not added into a single engine-test number.

Coverage includes bounded plugin/BSA parsing, compressed records, master remapping, independent contributions, deliberate dependent removal, explicit configuration, protected roles, field-only mapping repair, audio cache verification and:

- Partial original exclusion plus a related scoped song establishes takeover; a lone exclusion or unrelated source does not establish it. Removing the only valid evidence stops inference. Removing a ban on one song preserves that exception without erasing other evidence.
- Independent truth tables evaluate generated clauses for world AND quest stage >= 40 AND stage < 50, world AND cell, and world AND time >= 20. Ordinary music remains eligible outside the exact supported scope. Self and explicit-player contexts stay distinct.
- Reordered original baseline conditions with extra quest restrictions can still supply source evidence; a negative term inside an OR group cannot become an unconditional exclusion. Unsupported scene expressions pause the related list while another ordinary list still restores contributions.
- Original and final-winning replacement hashes are distinct; normalized audio and a standalone addition receive identical inferred clauses. A repeated run yields the same plan and exact cache bytes. Main/finale loss pauses the guarded plan together.
- Every inferred native condition edit is local to one playlist. Planner fixtures cover original/shared tracks, config-only Civil War/Boss owners, multiple ambient policies, palette ownership and explicit positive mixing. Scene songs shared with dedicated owners remain untouched.
- Typed VMAD scalar/array object properties, both object formats and quest aliases protect actual references. Integer/float/string bytes matching a FormID do not establish ownership; removed properties are ignored. Unknown structures keep conservative fallback protection. Public list references retain their known ordinary role.
- CME's six filters, combat/cleared channels, strict form resolution, priority, ambiguity, unavailable targets and all 512 three-filter required/optional/matched/actor combinations remain covered. Highest-ranked Boss failure does not fall through to generic regional music.

Synthetic RIFF fixtures prove structural validation and byte handling, not codec decoding. The truth tables evaluate planner output independently of its condition-negation implementation, but do not execute Skyrim's evaluator. Source/ownership review and compilation do not substitute for native execution.

## Actual installed plugins and configuration

All **94 enabled plugins** were inspected without changing them or the active profile. Actual Music Replacer Reverter v1 ESP/INI were supplied from a private workspace outside Data. Aggregate results are in `tests/real-input-results.json`; proprietary records and audio are excluded from the repository.

| Input | Result |
| --- | --- |
| Current 94 plugins | Zero playlist/condition/mapping/distribution commits proposed; about 3 seconds; complex Orsinium takeover pauses default-battle expansion |
| Current 94 + actual MRR | 303 valid unique config pairs; 295 remain executable, 8 default-battle pairs paused by the scene boundary; no new regional guards |
| Five official plugins + actual MRR + synthetic supported land | 303 distributions, one regional contribution group, three ordinary-copy guards, two isolated ordinary uses |

An independent Python record/config comparison checks every expected valid pair and the exact eight paused pairs. MRR's INI references 133 distinct IDs; 12 are absent and affect 32 unique destination/track pairs: `083D, 084F, 0850, 0853, 0854, 0855, 0856, 0857, 0858, 0859, 085A, 085B`. They are diagnosed/skipped rather than reconstructed.

The current Orsinium scene combines geographic and quest conditions with OR grouping, including a trailing OR, and reorders/extends ordinary originals' conditions. A known public MUSCombat reference no longer blocks the planner globally. The planner now identifies the paired source relationship and refuses to broaden its unsupported scene expression. It pauses all eight MRR additions to MUSCombat, including its dragon candidates, while leaving other destination types and the existing dedicated music intact. **This is controlled preservation, not a complete Orsinium compatibility patch.** An explicit verified patch/configuration is still needed to expand that list safely.

The synthetic land adds ordinary exclusions and a supported positive-area song. Actual MRR IDs `080E, 081F, 0820` receive guards; `081D, 081E` receive private ordinary-battle uses while Civil War keeps the original wrappers. Dragon/Boss IDs `0821, 0822, 08FA` keep their conditions. The synthetic geography is not Bruma/ECSS execution.

All 14 previously audited Creation plugin hashes match prior evidence; profile `plugins.txt` and `loadorder.txt` hashes are unchanged. One unsupported VMAD block retains conservative reference fallback. No parser fallback is presented as proof of script behavior.

## Real original audio with a simulated replacer

Read the local `Skyrim - Sounds.bsa` through the bounded reader with five official plugins. **243 eligible original track/list pairs** were unchanged with an empty overlay; no cache instances were proposed. The wider count than 0.7 reflects ordinary shared/public-list uses now being permitted.

A different valid stock combat track was placed at Combat 01's path in a private overlay. Exactly one coexistence proposal resulted. Original Combat 01 (643,446 bytes) and its original finale (76,010 bytes) were cached separately and verified by SHA-256 and exact bytes. Repeated rehearsal produced the same proposal and two cache files. Archive hashes and the replacement overlay remained unchanged. No game Data file was written.

This uses real original resources and a simulated replacement shape. **It is not a real Lorkhan test**, does not invoke the audio decoder and does not simulate the full manager/game VFS or BSA precedence. Runtime uses BSResourceNiBinaryStream for the actual winning resource and checks completed cache visibility.

## Native implementation and executable inspection

`tools/verify_native_layout.py` passed **21 read-only checks** on Steam SkyrimSE.exe 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Aggregate evidence is in `tests/native-layout-results.json`; no executable bytes/disassembly are distributed.

The SDK's inline-looking conditions field is accessed as the executable's TESCondition pointer at offset 0x10. Checks cover the separate 8-byte holder, 0x80 native single factory, fresh sound handles/loop pointer, MusicType virtual dispatch and six CME call sites. Address Library resolution, factory calls and condition evaluation are not executed offline.

The runtime prepares one playlist batch at a time. It validates the winning list, typed conditions, scene witnesses and main/finale resources; creates private condition instances; prepares original-audio versions from the same effective instances; reserves capacity and rechecks before committing. Other owners retain original objects. Failure leaves that batch unchanged, though validated cache files may remain and earlier independent batches may already be committed. There is one DataLoaded application per process. Area recovery is deferred if any runtime music batch fails.

## Remaining boundaries

- Native allocation/destruction, hook timing, playback, condition evaluation, random selection, combat end, transitions and save/reload remain untested.
- Full Bruma, Extended Cut, Lorkhan, Chapter II and all Nexus 89099 variants were not installed/jointly tested.
- Complex OR/dangling conditions, unknown functions, direct script mutation, independent custom combat libraries and unsupported MTD operations can need explicit rules, original-framework setups or authored patches. Harmonia pauses when standalone MTD/CME is loaded; they are not co-executed.
- Supported audio normalization includes the original and final visible winner, not every losing replacer. Changed record audio metadata, palette restructuring, compressed/unsupported original audio and invalid resources are not reconstructed. Re-encodings and MRR copies are not composition-deduplicated; extra candidates alter playback probabilities.
- Inferred takeover is a deliberate default. It cannot establish universal author intent or universal community compatibility. Explicit exceptions take priority; unreadable/unsupported cases are diagnosed.

Historical 0.7 results remain in their tag and are not counted as current native playback validation.
