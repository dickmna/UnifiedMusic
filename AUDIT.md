# Harmonia 1.0.0 — scope and minimality audit

2026-11-15. This is a source, data and offline regression audit. Native Skyrim playback has not been executed.

## Result

The supported feature set uses one startup planner, one per-playlist prepare/commit path and six optional CME-derived selector entry points. No second distribution framework, archive reader, audio disk cache, background polling, generated ESP or external missing-audio repair is needed for that scope. The 1.0.0 cleanup removes the obsolete ranged-file helper while retaining the bounded complete-file read used by the offline inspector.

This is a practical minimal design for the stated feature set, not a proof of the smallest possible program or full MTD/CME equivalence. Guards for independent ownership, explicit clear, native snapshots, main/finale integrity and ambiguous rules remain necessary to preserve the requested behavior.

## Upstream operations retained and boundaries

| Area | Implementation and comparison |
| --- | --- |
| Existing MusicType distribution | MTD-style native list clear/append; SimpleIni parsing. Conditions and shared track owners remain intact. Mixed clear/add keys have deterministic union semantics; invalid targets pause the plan. This is a deliberate difference from unspecified upstream key iteration. |
| Location and Region assignment | Same native music-pointer operations as pinned MTD. Existing Region sound entries retain their other fields; no new sound entries are synthesized. |
| Explicit combat and cleared selection | Six call sites and six filter kinds from pinned CME, using JsonCpp. Highest-rank ties and unavailable choices preserve the engine choice; combat stop bookkeeping is kept separate from the cleared channel. |
| Automatic compatibility | Recover independent authored memberships, apply complete authored scene restrictions to local ordinary uses, preserve dedicated roles and exact authored Boss links. These are Harmonia additions, not upstream-equivalence claims. |
| Optional original audio | Resolve only audited original/main-finale relationships; clone the effective local use with independent HQ paths in the same commit. Known stock/HQ copies suppress extra uses. |
| Deliberately omitted | MTD runtime MusicType creation, CME ActorUpdate polling/combat-fix option, arbitrary script execution and missing-file guesses. Configurations depending on these still need their original framework or an authored alternative. |

Pinned source revisions, licenses and adaptation history are in [PROVENANCE.md](PROVENANCE.md). Standalone MTD/CME must not run concurrently with Harmonia's writes.

## Modex native-reader assessment

Reassessed on 2026-11-15. **Prefer a native TESFile transport for the gameplay DLL and a separate portable transport for the offline inspector, sharing record decoding and music policy.** The earlier recommendation to retain the portable reader in the DLL because offline verification needs it was too broad: a development tool's dependencies need not ship in the gameplay target. The current 1.0.0 implementation remains unchanged; this is a corrected architectural recommendation, not an implemented or validated native adapter.

[Modex's `Data::CacheCells`](https://github.com/patchulidev/ModExplorerMenu/blob/7035daece244aaddb2bbcca315d9d99f8df86af7/src/data/Data.cpp) opens each loaded plugin with `OpenTES(kReadOnly, false)`, walks records with `SeekNextForm(true)`, reads CELL/EDID subrecords with `GetCurrentSubRecordType`, `ReadData` and `SeekNextSubrecord`, then calls `CloseTES(false)`. It keeps the first entry for each resolved cell ID. Its source attributes the approach to [Help Extender's CELL reader](https://github.com/shad0wshayd3-TES5/BakaHelpExtender/blob/88eff7fb626718b1dde536dffff639382ce27cfc/src/ObScript/Help.cpp). This is a precedent for reopening plugin files through the engine, not a way to recover overwritten definitions from the final in-memory forms.

Harmonia's pinned [TESFile declarations](https://github.com/SeaSparrowOG/CommonLibSSE/blob/2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8/include/RE/T/TESFile.h) expose these navigation/read methods, record headers, subrecord sizes and master information. The [implementations](https://github.com/SeaSparrowOG/CommonLibSSE/blob/2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8/src/RE/T/TESFile.cpp) forward to relocated engine functions. They are not a standalone parsing library that the inspector can execute without Skyrim. The newer Modex sample also uses `GetRuntimeFormID`, which is absent from Harmonia's pinned TESFile declaration; Harmonia would still need its existing master/local-ID resolution or an equivalent adapter.

| Responsibility | Effect of using TESFile |
| --- | --- |
| File opening, GRUP/record traversal, compression and subrecord traversal | Can potentially move to the engine. The adapter must still check read results, sizes, traversal completion and cleanup. This review has not executed or compared that path. |
| Per-plugin MUSC memberships, deleted records and dependency history | Must still be collected. Keeping only the first or winning definition loses evidence needed to distinguish independent additions from deliberate dependent removals. |
| MUST metadata and CTDA, typed VMAD references, palette ownership, area assignments and Location ancestry | Harmonia's decoding and protection rules remain necessary. Restricting the reader to music records would omit area and script evidence. |
| Scene policies, explicit-config priority, shared-track isolation and exact Boss links | Unchanged. A file-reading API does not supply these policies. |
| Optional original audio | Unchanged. TESFile reads plugin records; it does not provide hidden original audio. The independent optional HQ pack remains the source. |

The current `unified_music_core` supplies the same reader and planner to both the DLL and `UnifiedMusicInspect`. The preferred split has two small input adapters feeding one shared decoder/planner. Compile the engine adapter into the DLL and the portable file walker/decompressor into the inspector only. Move the portable reader's zlib linkage out of the shared core. Preserve each plugin's identity, masters, record flags and relevant subrecord bytes at that boundary; neither adapter should implement merging or scene inference. Separate build targets can keep the offline reader out of the shipped DLL without duplicating the music rules.

Two transports create a validation obligation, but their existence alone is not evidence that this design is less minimal. The relevant benefits are reduced custom file-format maintenance in the gameplay DLL and reuse of an engine-reading path already used by other plugins. Total repository line count is a poor deciding metric when it mixes production code with development tools. A native path is not automatically faster or more stable; those claims still need measurement. It also continues to reopen ESPs to recover their source definitions.

Before a future replacement, compare every source definition and final plan on both paths in the supported executable. Include compressed and extended-size records, nested groups, master/light-plugin references, deleted/ignored records, failures versus end-of-file, and engine cursor/handle ownership. Preserve full CTDA/VMAD evidence and all-or-nothing file admission. Modex's cell-index loop alone does not demonstrate those requirements. No new game-load hooks are justified merely to avoid reopening ESPs.

The retained implementation was rebuilt and rerun during the original review. Six full inspector outputs match their pre-review 1.0.0 baselines byte-for-byte; detailed evidence and the distinction from native TESFile equivalence are in [VALIDATION.md](VALIDATION.md) and `tests/native-reader-review-results.json`. Those historical results are not tests of the proposed split. The reassessment changes documentation only. Version and release artifacts remain 1.0.0.

### Which upstream work to reuse

- **Modex / Help Extender:** the most direct references for native read-only opening, record/subrecord traversal and close handling. Reuse that API pattern, with bounded buffers, checked failures and full history collection. A CELL catalog's first-entry policy is not a music merge policy.
- **Dynamic Music and Sounds:** the previously downloaded 0.2.0 source separates `ESP/Parser` from `Merge/MusicTypeMerger`, `MusicTrackMerger` and `SoundDescriptorMerger`. Its parser uses `std::ifstream` and zlib `uncompress`; its music-type merge compares source changes and applies them to the loaded list. This supports learning the separation and delta model, not attributing native TESFile reading to it. Its parsed music structures do not carry Harmonia's CTDA/VMAD scene evidence. [Upstream project and source download](https://www.nexusmods.com/skyrimspecialedition/mods/191993?tab=files).
- **Dynamic Leveled Lists / Dynamic NPCs:** useful semantic references, not evidence here for a native reader. The [list guide](https://supertron42.github.io/wiki/dynamicleveledlists/index.html) distinguishes repeated entries, removals and record protection. The [NPC release history](https://www.nexusmods.com/skyrimspecialedition/mods/172584) records a fix for unresolved references being mistaken for intentional removal. Preserve that distinction when changing Harmonia's input adapter. These observations come from the author's documentation, not a new audit of those two parsers.
- **MTD / CME:** retain the existing, attributed native distribution and selection operations described above. They remain execution-layer references; replacing the reader does not require redesigning that layer or adding their omitted features.

These projects establish specific precedents. They do not collectively establish that Harmonia's new adapter or its composer policy has already been tested in game.

## HQ-reference audit

The original/HQ mapping already excluded four legacy unresolved records; there was no valid alternative pairing to invent. Version 1.0.0 rechecks and enforces that boundary at packaging time:

- 247 original track records match the copied records by identity and full resource paths.
- 231 distinct main/finale pairs cover exactly 239 verified HQ resources; no missing or orphaned package reference remains.
- All 19 finale-bearing records retain matching native cue-point arrays. The prior original/HQ duration evidence and stock fingerprints remain tied to unchanged source hashes.
- Source and independent-path bytes match the audited SHA-256 values. Full decoding covers every packaged audio file.
- The packager checks the audited manifest fingerprint as well as individual files. Swapping a finale cannot pass merely because both audio files exist. LF and CRLF source checkouts have the same normalized fingerprint.

The four legacy records and five CC cues without verified HQ counterparts are not added to the manifest. Other mods' paths, records, audio and scripts are not repaired. No game BSA was reopened for this release audit. Source archives contain metadata and code only; the optional audio ZIP remains a separate local deliverable.

## Equivalence checks and release boundary

The installed 111-plugin combination was checked with and without the optional pack, plus private explicit-config and real-MRR overlays. All six complete inspector outputs match the 0.12.0 baseline exactly, including memberships, dedicated types, scene clauses, original/replacement counts and deduplication. The suite passed 315 Python tests, 523 selector checks and 28 read-only executable checks; all 239 audio files decoded without errors. Packaging tests also verified actual output bytes and rejection before output when a source was missing or changed. See [VALIDATION.md](VALIDATION.md) for results and limitations.

The 1.0.0 label is the requested final package/version designation. Supported runtime remains Steam 1.7.104 only. Offline checks cannot establish hook execution, music transitions, native condition evaluation, save/reload correctness or absence of in-game crashes.
