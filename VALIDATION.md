# Validation — Harmonia 1.0.0, 2026-11-15

Release x64 DLL, inspector and selector test executable built with VS 2022 / MSVC 19.44, CMake 4.3.3 and pinned dependencies. Python 3.14 executed the integration fixtures. **Steam 1.7.104 is the only enabled runtime.** No Skyrim process was launched.

## Automated checks

The 1.0.0 release audit changes no playlist/scene policy. All 247 original/HQ record mappings were rechecked using the unchanged original plugin evidence and freshly parsed MRR records; all 231 main/finale pairs, 19 finale-bearing records and 239 packaged resources agree. Zero missing or orphaned pack assets were found. Four legacy unresolved records remain excluded, and no missing external reference is redirected. Packaging now rejects any unaudited manifest change, including a rearranged finale whose individual file hash is still valid. Five packaging regression tests cover successful byte preservation, LF/CRLF equivalence, altered pairs, changed audio and missing finales. See [AUDIT.md](AUDIT.md) and `tests/audio-pack-results.json`.

Six complete inspector outputs match the 0.12.0-alpha baseline exactly: current record-only, with-pack, without-pack, real-MRR overlay, explicit town/area assignments and empty battle clear. The release rerun reused the previously extracted resource overlay and verified its recorded file hashes; it opened no BSA. All 111 plugin/profile/configuration inputs remain unchanged. This establishes offline behavior equivalence for those cases, not equivalence for every possible load order or native execution.

**315 Python test methods passed**, including parameterized subcases (`tests/latest-results.txt`). **523 selector logic checks passed**, with CTest reporting one passing test target (`tests/latest-selector-results.txt`). These count different kinds of checks and are not combined into an engine-test total.

Coverage includes bounded plugin/manifest parsing, compressed records, master remapping, independent contributions, dependent removals, explicit configuration, protected roles, field-only mapping repair, optional resource integrity, absent-pack behavior, exact known-encoding deduplication and:

- Case-insensitive unique CME/MTD EditorIDs; case collisions still fail even when one spelling matches exactly. Actual renames, deleted forms and unresolved references remain rejected.
- Explicit Location/Region assignments, alphabetical-file last-assignment precedence, repeated keys and priority over automatic area repair. Wrong-kind, missing and ambiguous references do not leak partial assignments.
- Clear-then-configure, deliberately empty lists, mixed plain/clear keys, missing optional tracks and shared Boss memberships. Cleared playlists reject automatic restoration, guards and original-audio proposals while other owners retain their own plans.
- SimpleIni BOM/CRLF/comments/multiline/metadata syntax, plus whitespace-delimited end comments. CME JSON comments, extra metadata and empty filters preserve the intended selector; an all-empty rule does not become global. Duplicate JSON keys and invalid recognized references still fail.
- Official area evidence for test-named ordinary town lists, with Boss/one-shot counterexamples. Original tracks reused by another plugin can be restored to their authored additional lists.
- Membership restoration independent of winning battle priority/fade settings. Dependency-completed framework tracks retain their final supported conditions. Conflicting final branches remain unresolved; input scan order does not choose a branch.
- Exact additional Boss memberships, explicit dependent Boss removal, and independent dragon membership after an ordinary scene-resource failure. No general pool is copied into dedicated lists.
- Bounded Location ancestry, conflicting parents and cycles. Read-only Region, Location-keyword and platform predicates retain their original expression.
- Scene policies require a related scoped song and matching authored ordinary exclusions, without a song-count threshold. Complete added clauses are inherited; baseline time/dragon eligibility, song-specific exceptions and dependent reversions remain local.
- Independent truth tables cover world/quest intervals, world/cell and world/time clauses. Cue conditions are never negated; terminal OR can remain read-only evidence. Missing or unsupported scene evidence pauses dependent automatic actions while independent roles and explicit memberships remain. Explicit membership overrides an ESP removal without rewriting the track's authored eligibility.
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

Actual installed **Lorkhan plus original audio** produced coexistence proposals for **60 distinct original tracks across 244 playlist uses with the optional HQ pack**. Original and replacement hashes differ. Both audio versions of the five guarded ordinary battle uses have identical inferred clauses. The other five candidate uses are Update.esm Creation Club dungeon tracks without verified HQ source resources; they are skipped. Without the optional pack, no audio coexistence is proposed and the record/scene plan is identical.

Two resources referenced by current playlists are absent from the local resource inventory:

- Chapter II: `music/forestfall/mus_palette_forestfall_day_b_17.xwm`.
- Crone: `music/dread/dread 01 v2.wav`, with no XWM equivalent found either.

These are existing missing inputs. Harmonia does not fabricate audio or delete their authored references. Other missing declared paths are not currently playlist-reachable and are not counted as active playlist failures. Northerner Diaries' non-ASCII filename exists; its legacy filename encoding was checked as bytes, not misreported as a missing UTF-8 name. Native filename resolution and codec playback remain untested.

All 111 plugin hashes, load-order/profile files, Vortex deployment metadata and the original ECSS JSON were unchanged after verification. One unsupported VMAD block retains reference fallback. One competing mapping remains unrepaired. Skip diagnostics for unsupported condition cases are retained; this is not a claim that arbitrary conditions or scripts are now supported.

## Explicit configuration against the installed combination

A separate private configuration overlay used the same 111 plugins and verified resources. It selected an actual ECSS Location and Region, an existing ECSS MusicType and a Chapter II track. No fixture plugin or deployed configuration was changed. The independent expected outcomes were:

- Both area assignments resolve to the specified ECSS type. The selected Region has one existing sound-data block; this establishes a meaningful target for the native operation, but the slot assignment itself is not executed offline.
- Clearing the ordinary town list retains only its explicitly configured Chapter II member. Automatic additions and original-audio candidates do not reappear there. Ordinary battle guards and independently restored Boss links match the baseline.
- A second overlay explicitly leaves the default battle list empty. It produces no automatic battle membership, guard or audio additions. Independent Boss membership restoration and the original ECSS selector remain unchanged.
- A copy of the actual ECSS JSON with BOM, comments, descriptive metadata and an empty optional filter resolves to exactly the same selectors as the unmodified file.
- ECSS, Bruma and SIRENROOT independent playlist contents remain unchanged. Input plugin/profile/deployment/configuration hashes remain unchanged.

Aggregate results are recorded as `configuration_priority` in `tests/real-input-results.json`. These are inspector proposals checked against independent expectations, not a native playback trace.

## Optional HQ source and duplicate regression

The existing MRR v1 archive's SHA-256 matches the prior download. The separate optional package contains **239 XWMA files, all 48 kHz / 192 kbit/s**, in **231 verified main/finale pairs** covering **247 original MUST records**. The 19 records with finales retain their exact native cue-point arrays. Original/HQ encoded durations agree within 0.25 seconds. All 239 packaged files also passed full FFmpeg decoding with no errors; this does not execute Skyrim playback. Every main/finale mapping agrees both by record identity and complete source path after removing MRR's rename suffix. Files are copied unchanged into an independent namespace; MRR's ESP, INI and unreferenced audio are omitted. See `tests/audio-pack-results.json` for source hashes and file mapping.

Four MRR records reference absent audio (`MUSDread01`, `MUSDread03`, `MUSDread04`, `MUSDread01a`); no substitute is guessed. Five newer Creation Club dungeon tracks also have no licensed corresponding audio in the source. These omissions affect optional original coexistence, not the installed cue's existing playback.

Adding the real MRR ESP/INI to a private 112-plugin inspection, with its actual archive audio represented in the verified overlay, suppresses **208 additional original uses** and leaves **36 optional uses**. No scene-resource failure occurs. MRR's existing 32 invalid configuration references (12 distinct IDs) are diagnosed separately; no copied config is shipped. Deduplication preserves an existing original/HQ use's conditions, including deliberately narrow eligibility, instead of deleting or broadening that use. It does not consolidate explicit duplicates already present in the input.

Synthetic integration tests cover missing/corrupt packs, known stock versus HQ encodings, explicit copies, repeated source wrappers, different or missing finales, unverified pair mappings, invalid manifest paths/hashes and rejection of retired archive/cache options. A missing optional asset does not suppress independent pairs. Repeating inspection changes no input file. Runtime source contains no BSA parser, archive-open path or audio disk write. Private source preparation and verification use development-only archive extraction outside the shipped DLL/inspector.

## Native implementation and executable inspection

`tools/verify_native_layout.py` passed **28 read-only checks** on Steam SkyrimSE.exe 1.7.104.0, SHA-256 `846efccf0c1374d71f892907f46549560f2fcb0a75cb87a3eed438baa0f1402f`. Aggregate evidence is in `tests/native-layout-results.json`; no executable bytes/disassembly are distributed.

The SDK's inline-looking conditions field is accessed as the executable's TESCondition pointer at offset 0x10. Checks cover the separate 8-byte holder, 0x80 native single factory, fresh sound handles/loop pointer, MusicType virtual dispatch and six CME call sites. Seven new checks cover the Region manager cast entry, sound-data type dispatch, music-field offset, DataHandler manager field and Location music field used by the MTD-style assignments. Address Library resolution, factory calls, area assignments and condition evaluation are not executed offline.

Runtime prepares one playlist batch at a time, validates typed conditions and scene resources, prepares local conditions and original-audio instances, reserves capacity and rechecks before committing. Automatic work additionally checks the winning plugin snapshot; pure explicit MTD operations use the actual loaded list. Clear removes references from only that list, then appends configured members. Other owners retain their original objects.

A pre-commit failure removes scene-dependent automatic actions and retries the independent remainder. If preparation still fails, valid explicit instructions can retry alone through the same path. Exceptions do not retry. Earlier independent batches may remain. No audio cache is created. Application still runs once at DataLoaded. Explicit area assignments can proceed except where their target playlist failed; automatic area recovery is deferred after any music preparation failure. This fallback and native list/area mutations are reviewed and compiled, not executed by the offline test suite.

## Remaining boundaries

- Native allocation/destruction, hook timing, optional-resource visibility, playback, condition evaluation, random selection, combat end, transitions and save/reload remain untested. Joint offline validation of the installed mods is not joint in-game validation.
- Unknown functions, unresolved/alias conditions, incomplete editable clauses, direct script mutation, independent custom combat libraries and runtime-created MusicTypes can need authored rules, original-framework setups or patches. CME's optional polling fix is not included. Harmonia pauses if standalone MTD/CME is loaded; they are not co-executed.
- Keeping explicit membership during an inference failure preserves MTD-style behavior under the authored conditions; it does not prove automatic protection for an unreadable script/scene. Explicit configuration is authoritative, including deliberate changes to dedicated lists.
- Audio normalization covers the original plus final visible winner, not every losing replacer. Changed record audio metadata, palette restructuring, unsupported audio and invalid resources are not reconstructed. Known stock/HQ bytes, including verified MRR copies, are deduplicated; arbitrary re-encodings and palette-contained copies are not perceptually identified. Adding distinct candidates changes probabilities.
- Inferred scene takeover remains a deliberate default. Other pack versions and every Music Patch for New Lands variant were not checked. Unsupported evidence is diagnosed, not proof of universal author intent or compatibility.

Historical results remain in their tags and are not counted as current native playback validation.
