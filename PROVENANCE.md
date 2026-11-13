# Provenance

Harmonia was named UnifiedMusic through version 0.4; its technical repository and DLL names remain UnifiedMusic. Versions 0.1/0.2 embedded adaptations of Music Type Distributor and Combat Music Extender. Version 0.3 removed those implementations and their configuration/playback adapters, retaining the project's scanner and restoration runtime. The repository remains AGPL-3.0-or-later; this does not relicense historical contributions.

- MTD: ThirdEyeSqueegee; `stop-sine/MusicTypeDistributor-RE`, commit `9a4cdf112c64bff91bec8f834c62bd00c46d2fe1`. Original GPL-3.0-or-later license, exceptions, and source manifest remain in `vendor/MTD`.
- CME: `SeaSparrowOG/CombatMusic`, commit `302c6df30348ce83cfcf72cbf6ae4e8681fcc7b6`. Original AGPL-3.0 license and source manifest remain in `vendor/CME`. That license governs the historical adaptation rather than conflicting metadata summaries.
- Former integration sources remain in the `v0.1.0-alpha` and `v0.2.0-alpha` tags. Neither module is compiled or shipped in the current DLL.
- Remaining dependencies are pinned in `cmake/Dependencies.cmake`. CommonLibSSE, commonlib-shared, spdlog and zlib retain their original notices, included in binary packages.
- Record layouts were checked against TES5Edit's `Core/wbDefinitionsTES5.pas`. No game data, paid Creation plugin, script, or music asset is distributed.
