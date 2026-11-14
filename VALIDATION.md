# Validation — Harmonia 0.10.0-alpha, 2026-11-14

Release x64 DLL, inspector and selector test executable built with VS 2022 / MSVC 19.44, CMake 4.3.3 and pinned dependencies. Python 3.14 executed the integration fixtures. **Steam 1.7.104 is the only enabled runtime.** No Skyrim process was launched.

## Automated checks

**295 Python test methods passed**, including parameterized subcases (`tests/latest-results.txt`). **523 selector logic checks passed**, with CTest reporting one passing test target (`tests/latest-selector-results.txt`). These count different kinds of checks and are not combined into an engine-test total.

Coverage includes bounded plugin/BSA parsing, compressed records, master remapping, independent contributions, dependent removals, explicit configuration, protected roles, field-only mapping repair, audio cache verification and:

- Case-insensitive unique CME/MTD EditorIDs; case collisions still fail even when one spelling matches exactly. Actual renames, deleted forms and unresolved references remain rejected.
- Official area evidence for test-named ordinary town lists, with Boss/one-shot counterexamples. Original tracks reused by another plugin can be restored to their authored additional lists.
- Membership restoration independent of winning battle priority/fade settings. Dependency-completed framework tracks retain their final supported conditions. Conflicting final branches remain unresolved; input scan order does not choose a branch.
- Exact additional Boss memberships, explicit dependent Boss removal, and independent dragon membership after an ordinary scene-resource failure. No general pool is copied into dedicated lists.
- Bounded Location ancestry, conflicting parents and cycles. Read-only Region, Location-keyword and platform predicates retain their original expression.
- Scene policies require a related scoped song and matching authored ordinary exclusions, without a song-count threshold. Complete added clauses are inherited; baseline time/dragon eligibility, song-specific exceptions and dependent reversions remain local.
- Independent truth tables cover world/quest intervals, world/cell and world/time clauses. Cue conditions are never negated; terminal OR can remain read-only evidence. Missing or unsupported scene evidence pauses dependent ordinary actions while independent roles can remain.
- Playlist-local instances isolate inferred conditions, including original/shared tracks, config-only Civil War/Boss owners, palette owners and explicit positive mixing. Original and replacement audio receive the same effective ordinary rules.
- Typed VMAD properties protect actual script references; scalar bytes that happen to match an ID do not establish ownership. Unsupported structures retain conservative fallback protection.
- CME's six filters, combat/cleared channels, priority, ambiguity, unavailable targets and all 512 three-filter required/optional/matched/actor combinations. A failed highest-ranked Boss target does not fall through to generic regional music.

Synthetic RIFF fixtures validate structure and bytes, not codec decoding. Planner truth tables do not execute Skyrim's condition evaluator. Native failure recovery is reviewed and compiled; the offline inspector exercises the shared scene-removal planner, not native allocations.

## Current installed combination

Inspected **111 enabled plugins**: five official base plugins, 75 `Skyrim.ccc` entries and 31 enabled `plugins.txt` entries. Their union matches the load order, with no missing masters or invalid master ordering. The actual ECSS JSON was supplied unchanged. Aggregate evidence is in `tests/real-input-results.json`; raw proprietary records and music remain outside the repository.

The installed combination includes ECSS, Beyond Skyrim: Bruma, SIRENROOT, Chapter II, Northerner Diaries, Musical Lore, Around the Fire, Lorkhan and paid Creations, including Orsinium and Crone. Results apply to these deployed files, not every edition of each mod.

| Independently identified missing membership | Recovered in the plan |
| --- | ---: |
| Town: Chapter II 8, Northerner Diaries 3, reused official tracks 4 | 15 / 15 |
| Default battle: Bruma 10, Orsinium 1 | 11 / 11 |
| Boss: five of the Bruma tracks also explicitly assigned here | 5 / 5 |
| Other: Chapter II exploration 144, reused official tracks 20 | 164 / 164 |
| Total | **195 / 195** |

These are playlist memberships, not distinct compositions. The inspector emits 244 APPEND proposals, some already present in the winning list; the independent comparison identifies the 195 actually absent memberships. Runtime appending retains existing membership multiplicity.

ECSS's `ECSSGroveofReflection01` resolves uniquely to the actual `ECSSGroveOfReflection01`; the original configuration no longer causes CONFIG_HANDOFF. The official `MUSTownTest` has 86 official area assignments and is recognized as an ordinary town list. Bruma's framework priority change no longer suppresses battle membership restoration; the current priority and fade fields are retained. Dependent track definitions complete the framework's empty music placeholders without a mod-name exception.

The plan leaves **ECSS's five, Bruma's 20 and SIRENROOT's 29 independent MusicTypes unchanged**. Bruma's restored Boss links retain the tracks' non-dragon and other authored conditions. Musical Lore's shared dragon cue remains dragon-only; it receives no ordinary battle guard. Scripts and dedicated cue expressions are not rewritten or executed by the inspection.

## Actual authored restrictions and resources

Five ordinary combat tracks inherit four authored clauses, only in the default battle use:

- Outside Beyond Skyrim's parent Location `bsassets.esm|6036AD`, function 359 == 0, explicit Player. The positive cue points at a descendant; the recorded chain establishes the relationship.
- Outside Orsinium Cell `legacyorsinium.esm|06E9CC`, function 67 != 1, explicit Player.
- Orsinium Quest `legacyorsinium.esm|014C29` stage != 40, function 58, Self.
- Outside Orsinium Worldspace `legacyorsinium.esm|085C50`, function 310 == 0, Self.

The cue's full OR/keyword/quest expression is retained. No inverse cue expression is synthesized. The tracks' baseline negative-dragon eligibility remains their own qualification. Two shared Civil War uses retain their original objects while default battle gets local instances.

An independent evaluator compared actual ordinary donor conditions with baseline eligibility plus generated clauses in **240 cases**: five tracks across combinations of Beyond Skyrim Location, Orsinium room/world, quest stages 39/40/41 and dragon status. All agreed. Some states are deliberately hypothetical; this checks inheritance, not runtime quest timing or Skyrim's evaluator.

Relevant deployed loose resources and uniquely resolved BSA members were copied into a private verified overlay. Resource-aware inspection checked the **11 distinct scene tracks**, including their referenced main/finale resources, with no scene-resource skips; its membership/guard/selector plan matched record-only inspection. The overlay models the verified local resources, not the engine's full VFS or archive precedence.

Actual installed **Lorkhan plus original audio** produced coexistence proposals for **65 distinct original tracks across 249 playlist uses**. Original and replacement hashes differ. Both audio versions of the five guarded ordinary battle uses have identical inferred clauses. No game cache was created during this combined inspection.

Two resources referenced by current playlists are absent from the local resource inventory:

- Chapter II: `music/forestfall/mus_palette_forestfall_day_b_17.xwm`.
- Crone: `music/dread/dread 01 v2.wav`, with no XWM equivalent found either.

These are existing missing inputs. Harmonia does not fabricate audio or delete their authored references. Other missing declared paths are not currently playlist-reachable and are not counted as active playlist failures. Northerner Diaries' non-ASCII filename exists; its legacy filename encoding was checked as bytes, not misreported as a missing UTF-8 name. Native filename resolution and codec playback remain untested.

All 111 plugin hashes, load-order/profile files, Vortex deployment metadata and the original ECSS JSON were unchanged after verification. One unsupported VMAD block retains reference fallback. One competing mapping remains unrepaired. Skip diagnostics for unsupported condition cases are retained; this is not a claim that arbitrary conditions or scripts are now supported.

## Original-audio regression with a simulated replacer

A separate check reads local `Skyrim - Sounds.bsa` with the five official plugins. **252 eligible original track/list pairs** are unchanged with an empty overlay; no coexistence instances are proposed. A different valid stock combat track placed at Combat 01's path in a private overlay produces exactly one coexistence proposal.

Original Combat 01 (643,446 bytes) and its finale (76,010 bytes) are cached separately and verified by exact bytes and SHA-256. Repeating the rehearsal yields the same proposal and two cache files. Official archive hashes and the replacement overlay remain unchanged. This controlled regression is separate from the real Lorkhan inspection above. It does not decode audio or run the engine resource resolver. All writes are in a private workspace, never game Data.

## Native implementation and executable inspection

`tools/verify_native_layout.py` passed **21 read-only checks** on Steam SkyrimSE.exe 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Aggregate evidence is in `tests/native-layout-results.json`; no executable bytes/disassembly are distributed.

The SDK's inline-looking conditions field is accessed as the executable's TESCondition pointer at offset 0x10. Checks cover the separate 8-byte holder, 0x80 native single factory, fresh sound handles/loop pointer, MusicType virtual dispatch and six CME call sites. Address Library resolution, factory calls and condition evaluation are not executed offline.

Runtime prepares one playlist batch at a time, validates its winning snapshot, typed conditions and scene resources, prepares local conditions and original-audio instances, reserves capacity and rechecks before committing. Other owners retain original objects. An explicit pre-commit failure removes scene-dependent actions and retries the independent remainder; exceptions do not retry. Earlier independent batches and validated cache files may remain. Application runs once at DataLoaded; area recovery is deferred if any runtime music batch fails. This release does not change that lifecycle.

## Remaining boundaries

- Native allocation/destruction, hook timing, cache visibility, playback, condition evaluation, random selection, combat end, transitions and save/reload remain untested. Joint offline validation of the installed mods is not joint in-game validation.
- Unknown functions, unresolved/alias conditions, incomplete editable clauses, direct script mutation, independent custom combat libraries and unsupported MTD operations can need authored rules, original-framework setups or patches. Harmonia pauses if standalone MTD/CME is loaded; they are not co-executed.
- Audio normalization covers the original plus final visible winner, not every losing replacer. Changed record audio metadata, palette restructuring, compressed/unsupported original audio and invalid resources are not reconstructed. Re-encodings and MRR copies are not composition-deduplicated; extra candidates alter probabilities.
- Inferred scene takeover remains a deliberate default. Other pack versions and every Music Patch for New Lands variant were not checked. Unsupported evidence is diagnosed, not proof of universal author intent or compatibility.

Historical 0.7/0.8/0.9 results remain in their tags and are not counted as current native playback validation.
