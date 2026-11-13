# Validation — 2026-11-13

## Completed

- Release x64 build of version 0.4.0 `UnifiedMusic.dll` and `UnifiedMusicInspect.exe` using MSVC 19.44 / VS 2022 and pinned dependencies. No embedded CME/MTD, JsonCpp, SimpleIni, playback hooks or trampoline initialization.
- 129 synthetic scanner test methods passed: 49 ambient/parser tests, 50 combat-list tests, and 30 condition-repair tests. Multiple subcases and input-order permutations are included. Logs are `tests/latest-results.txt`, `tests/latest-combat-results.txt`, and `tests/latest-condition-results.txt`.
- Condition cases cover independent A/B exclusion union, semantic comparison equivalence, player references, ignored padding, master-table normalization, original-prefix changes, metadata/audio changes, OR/alias/unknown functions, deleted/unresolved areas, script/Boss/palette protection, existing union patches, and direct/transitive dependency removals. A regression verifies that a patch removing only a battle-list contribution through an intermediate dependency disables both list and condition recovery.
- Read-only inspection of 94 installed plugins completed successfully in approximately 3.55 seconds. It produced no ambient additions, battle groups, or condition repairs. Orsinium's special music remained excluded; its original-track changes were rejected as protected sharing or unsupported condition extensions. This tests preservation for this sample, not a successful real-world condition repair.
- Source review covered typed disk/native condition comparison, winning-source selection, required-track checks, preparation before commit, allocation through the game heap, cleanup of uncommitted nodes, and append-only condition ownership. No original node is copied, deleted or evaluated. These checks were reviewed, not executed inside Skyrim.
- Neither the game Data directory, load order, nor saves were modified.

## Not completed and limits

- No Skyrim process was launched. Native condition-node allocation, source-file resolution, runtime prechecks, playback, entering/leaving areas, quest changes, combat end, and save/reload remain unverified. Scanner tests do not execute the SKSE adapter or audio manager.
- Nexus 89099's complete ESP variants were not downloaded and compared record by record. This release is not certified to replace Music Patch for New Lands. Existing authored patches remain needed for unsupported logic or unverified combinations.
- Lorkhan and Chapter II were not installed for testing. Same-path audio replacements are outside scope.
- Exclusion recovery only covers eligible original Skyrim tracks in the original default battle list. It rejects OR expressions, changed original conditions, non-geographic additions, changed audio/metadata, direct or transitive removals, and shared/scripted/Boss/palette tracks.
- Track presence does not prove audio-file availability or that an alternative is playable in every quest state. Unsupported script behavior and another DLL's changes may remain outside static discovery; semantic comparison checks the recognized winning track at startup only.
- The original global `MUSCombat` remains a deliberate exception to whole-type VMAD protection because default cleanup scripts may refer to it. Direct references to tracks and sharing with other protected types still prevent automatic changes.
- No MTD INI or CME JSON rules are read. Config-only packs need their original framework or an earlier combined release. Runtime co-installation and initialization order with standalone frameworks are unverified.

This is an experimental alpha, not universal music compatibility certification.
