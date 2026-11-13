# Validation — 2026-11-13

## Completed

- Release x64 build of version 0.3.0 `UnifiedMusic.dll` and `UnifiedMusicInspect.exe` using MSVC 19.44 / VS 2022 and pinned dependencies. The DLL links the scanner and CommonLibSSE; embedded MTD/CME, JsonCpp and SimpleIni are removed. SKSE hook/trampoline initialization is disabled.
- 98 synthetic scanner test methods passed: 48 ambient/parser tests (`tests/latest-results.txt`) and 50 automatic combat tests (`tests/latest-combat-results.txt`), including multiple subcases and input permutations.
- Combat cases cover unconditional additions, independent overriding packs, positive Cell/Worldspace/Location gates, player references, master-table reindexing for geographic and quest/global parameters, deliberate removals, changed conditions, script references, custom-type sharing, one-shot content, malformed conditions, Ignored/Partial records, compressed tracks, and unresolved/deleted areas.
- New regression cases cover direct script references to ambient candidates, sharing with custom/scripted/one-shot/original Boss types, palette member references across master tables, malformed/deleted candidates, and ordinary ambient sharing across mapped areas. Battle tests independently cover palette membership and original Boss-type sharing.
- Read-only native scan of 94 locally installed plugins (base game, official content and 14 enabled Creation plugins): successful, approximately 2.54 seconds. No ambient append proposals and no eligible automatic combat restoration groups for this installed set. The Orsinium Boss track was excluded because it also belongs to protected custom music. This demonstrates preservation for that scanned sample, not runtime compatibility certification.
- Source review covered MusicTrack FormWrapper identity, partial/ignored record handling, load-order independence, transactional file parsing, explicit dependent-patch deletions, and shared protection for ambient/battle tracks. The adapter reuses original lists and track conditions. It does not intercept playback, allocate MusicTypes, or pre-evaluate native conditions.
- DLL exports remain `SKSEPlugin_Load` and `SKSEPlugin_Version`; dynamic dependencies are Windows system libraries. Build wiring no longer references embedded CME/MTD or the two removed parsers.
- Neither the game Data directory nor its load order or saves were modified.

## Not completed

- No Skyrim process was launched. Restored-track playback, entering/leaving an area during combat, quest-stage changes, combat start/end, scripted music restoration, and save/reload behavior are unverified. Synthetic scanner tests do not validate runtime append operations or audio-manager behavior.
- Lorkhan and Chapter II were not installed in the development environment. Their particular release combinations are not certified by these tests.
- Synthetic ESP tests exercise the same scanner used by the DLL; they do not exercise SKSE or the real audio manager.
- DLLs or scripts using dynamically constructed lookups can bypass static VMAD discovery. The default battle list is a deliberate exception to whole-type VMAD protection: its own proven contributions can be restored even when a script references that global type. Direct script references to candidate tracks and sharing with other custom/protected types still prevent restoration. Runtime modifications by other DLLs are not fully represented in the static scan.
- Automatic battle restoration supports plain single-track additions and a restricted condition schema. Unknown condition functions, OR branches, quest-only tracks, palette/silence tracks, and unproven custom battle mappings are not automatically merged.
- Version 0.3 does not execute MTD INI or CME JSON rules. Config-only music packs require their original framework or an earlier combined release. Runtime co-installation and initialization order with standalone music frameworks have not been tested.
- Same-path audio file replacement conflicts and incompatible track-condition edits require an explicit policy/patch; this plugin leaves winning track conditions intact.

This is an experimental alpha, not a production compatibility certification.
