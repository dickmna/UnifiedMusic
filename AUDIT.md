# Harmonia 1.0.1 — scope and minimality audit

2026-11-15. This is a source, data and offline regression audit. Native Skyrim playback has not been executed.

## Result

The supported feature set uses one startup planner, one per-playlist prepare/commit path and six optional CME-derived selector entry points. No second distribution framework, archive reader, audio disk cache, background polling, generated ESP or external missing-audio repair is needed for that scope. Version 1.0.1 replaces the gameplay DLL's portable plugin reader with native TESFile calls. The offline inspector retains the portable reader; both share one decoder and planner. zlib is no longer a gameplay dependency.

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

Implemented in 1.0.1: the gameplay DLL uses a native TESFile input adapter; the offline inspector keeps the portable file reader. Both feed `PluginDecode` and the existing music planner. No parser fallback, new loading hook, second rule set or additional configuration layer was introduced.

[Modex's `Data::CacheCells`](https://github.com/patchulidev/ModExplorerMenu/blob/7035daece244aaddb2bbcca315d9d99f8df86af7/src/data/Data.cpp) and the attributed [Help Extender CELL reader](https://github.com/shad0wshayd3-TES5/BakaHelpExtender/blob/88eff7fb626718b1dde536dffff639382ce27cfc/src/ObScript/Help.cpp) provide the native read-only opening, navigation, reading and closing pattern. Harmonia's adapter is independently implemented. Their first-entry CELL indexing policy is not used: music recovery requires all relevant source versions, including deletions and dependency history.

| Layer | 1.0.1 implementation |
| --- | --- |
| Gameplay file transport | `NativePlugin` / `NativePluginInput`: `OpenTES(kReadOnly, false)`, native record/subrecord navigation, `ReadData` and `CloseTES(false)`. The engine handles compressed records and `XXXX` fields. |
| Offline file transport | `src/offline/PluginFile`: existing bounded file walker, subrecord parser and zlib decompression, linked only to the inspector. |
| Shared record decoding | `PluginDecode`: plugin/master identities, MUSC memberships, MUST metadata/CTDA, typed VMAD ownership, palette references, area assignments and Location ancestry. |
| Shared music rules | Existing explicit-config priority, dependent-removal handling, scene restrictions, local track isolation and exact authored Boss links. No rule change in this refactor. |
| Original audio | Unchanged optional HQ pack. TESFile reads plugin records; it does not recover hidden audio. No BSA parser or audio cache. |

The pinned [CommonLibSSE declarations](https://github.com/SeaSparrowOG/CommonLibSSE/blob/2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8/include/RE/T/TESFile.h) and [wrappers](https://github.com/SeaSparrowOG/CommonLibSSE/blob/2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8/src/RE/T/TESFile.cpp) are supplemented by two adaptations established through read-only inspection of the supported Steam 1.7.104 executable:

- The engine already remaps record-header FormIDs. Harmonia reverses that mapping through the loaded full/light master table before the shared decoder sees it. Subrecord references remain file-local and are not reversed. TES4 master names must agree with the loaded table.
- `SeekNextForm` has a third argument absent from the pinned SDK declaration. A typed Address Library relocation calls it with ignored-record skipping disabled and eager subrecord initialization disabled. Headers are checked before their bodies are read. Runtime support remains restricted to 1.7.104.

The adapter checks group/record boundaries, native cursor progress, field lengths, read results and complete traversal. It copies relevant fields into dynamically sized buffers; unrelated payloads are skipped through the engine. Music metadata and VMAD on non-music records remain available. A plugin's decoded evidence is admitted only after successful traversal and non-forcing close; a failure aborts startup planning before writes. The compressed-size check limits what Harmonia accepts/copies, not the engine's preceding decompression allocation. File-handle lifetime follows `CloseTES(false)`, including the engine's own retention policy.

Both the normal development build and a separate gameplay-only build succeed. Their link maps contain the native adapter/shared decoder and exclude the portable reader and zlib. Disabling the inspector also removes zlib from the build dependency graph. This reduces custom file-format work in the gameplay DLL; startup speed and native stability have not been benchmarked.

The new contract suite runs the production adapter against a scripted API double: 223 checks cover full/light identity normalization, long fields, compressed-buffer handling, nested groups, CTDA/VMAD evidence, deletions, read/close failures and complete-file admission. Forty-five static executable checks include 17 TESFile checks. The current 111-plugin plans pass through the portable reader and shared rules; six full outputs match 1.0.0 exactly. These are complementary offline checks, **not an execution of TESFile inside Skyrim**. Engine decompression, Address Library resolution and native-to-portable equality on a running game remain untested. Evidence is in [VALIDATION.md](VALIDATION.md) and `tests/native-reader-results.json`.

The previous documentation-only assessment is retained as historical evidence in `tests/native-reader-review-results.json`; it describes the old 1.0.0 reader, not this implementation.

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

The four legacy records and five CC cues without verified HQ counterparts are not added to the manifest. Other mods' paths, records, audio and scripts are not repaired. No game BSA was reopened for the 1.0.0 HQ audit or the 1.0.1 reader rerun. Source archives contain metadata and code only; the optional audio ZIP remains a separate local deliverable.

## Equivalence checks and release boundary

The installed 111-plugin combination was checked with and without the optional pack, plus private explicit-config and real-MRR overlays. All six complete inspector outputs match 1.0.0 exactly, including memberships, dedicated types, scene clauses, original/replacement counts and deduplication. The suite passed 315 Python tests, 523 selector checks, 223 native-reader contract checks and 45 read-only executable checks. The unchanged HQ pack retains its prior complete 239-file decode and mapping audit; audio was not repackaged. See [VALIDATION.md](VALIDATION.md) for exact scope.

Supported runtime remains Steam 1.7.104 only. Offline checks do not establish native file-call execution, hook execution, music transitions, native condition evaluation, save/reload correctness or absence of in-game crashes.
