# Validation — 2026-11-13

## Completed

- Release x64 build of `UnifiedMusic.dll` and `UnifiedMusicInspect.exe` using MSVC 19.44 / VS 2022 and pinned dependencies. Final rebuild includes CME ownership and strict JSON-field validation changes.
- 33 synthetic scanner test methods, including multiple subcases and all six input permutations for dependency-removal and original-definition ordering tests: passed. See `tests/latest-results.txt`.
- Read-only native scan of 94 locally installed plugins (base game, official content and 14 enabled Creation plugins): successful, approximately 2.75 seconds on the development machine. No automatic append proposals for this installed set. Existing custom music types remained outside the generic ambient merge policy.
- Source review covered MusicTrack FormWrapper identity, partial-record handling, load-order independence, transactional file parsing, explicit dependent-patch deletions, script-bound replacement suppression, and CME ownership boundaries.
- Neither the game Data directory nor its load order or saves were modified.

## Not completed

- No Skyrim process was launched. Hook execution, playback transitions, combat start/end, scripted music restoration, and save/reload behavior are unverified.
- Lorkhan and Chapter II were not installed in the development environment. Their particular release combinations are not certified by these tests.
- Synthetic ESP tests exercise the same scanner used by the DLL; they do not exercise SKSE or the real audio manager.
- DLLs or scripts using dynamically constructed lookups can bypass static VMAD discovery. Existing script-bound types and already-active candidates are protected, but this cannot guarantee every future runtime interaction.
- Same-path audio file replacement conflicts and incompatible track-condition edits require an explicit policy/patch; this plugin leaves winning track conditions intact.

This is an experimental alpha, not a production compatibility certification.
