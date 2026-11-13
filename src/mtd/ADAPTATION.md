MTD INI compatibility layer

Implementation source: ThirdEyeSqueegee / MusicTypeDistributor 1.4.0, preserved in
stop-sine/MusicTypeDistributor-RE at 9a4cdf112c64bff91bec8f834c62bd00c46d2fe1.
License: GPL-3.0-or-later with vendor/MTD/EXCEPTIONS. UPSTREAM.json records original
file hashes. The final runtime layer is Integration.cpp + Integration.h; CMake
explicitly builds only Integration.cpp. Earlier draft files in this working
folder are not part of the build or release.

At SKSE kDataLoaded: PrepareDistribution(), optionally SetTypePolicy(callback),
then ApplyDistribution(). The host owns the SKSE entry point, logging, and the
runtime compatibility declaration. RunDistribution() invokes both phases. Calls
are idempotent for one load lifetime; hot reload is not provided.

Retained format: Data/*_MUS.ini, [General] target EditorID keys, comma-separated
hexadecimal FormID~PluginFilename values. MusicTypes are found by scanning their
retained EditorIDs; tracks use TESDataHandler's plugin-scoped FormID lookup. No
po3 Tweaks dependency, EditorID cache, parallel_hashmap, or second DLL is needed.

The preservation policy changes destructive legacy behaviors deliberately:
- Append existing track forms, deduplicate, and retain their original conditions.
- Ignore ! clear requests; do not erase an authored list.
- Skip [Location]/[Region] redirects and unknown runtime MusicType creation.
- Protect unknown/custom, combat, boss, silence, stinger, reward and quest targets.
- Default eligible targets are engine-owned exploration/town/dungeon/tavern
  background types. The host can supply a validated FormID policy instead.

Legacy MusicTypeDistributor.ini diagnostic dump toggles are not implemented in
this minimal runtime layer. Skipped instructions and malformed/unresolved track
tokens are logged. This is compatible background distribution, not full emulation
of every destructive MTD configuration feature.

Dependencies: CommonLibSSE::CommonLibSSE and SimpleIni::SimpleIni (C++23). Direct
MSVC compile passed using Visual Studio 2022 17.14.33, Steam CommonLibSSE revision
2415cc998b611d3f0ac15fcfeadca9c83b1bf7f8, /std:c++latest /DSKYRIM_SUPPORT_AE=1.
This checks source compilation, not Skyrim runtime behavior.
