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
