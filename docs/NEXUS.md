# Harmonia — Automatic Music Integration

**More music. Room for every scene. One SKSE plugin.**

Harmonia brings independently added music back into playlists when Skyrim's winning plugin record leaves it out. It also carries supported regional and story exclusions over to the added tracks, so a new soundtrack does not drown out a location's own score.

Install your music mods, install Harmonia, and launch through SKSE. There is no patch generator to run and no extra ESP to enable.

## What it does

- Recovers eligible tracks lost when several plugins edit the same playlist.
- Follows the playlists chosen by the music authors: exploration, towns, taverns, dungeons and combat keep their own arrangements.
- Respects intentional removals by dependent patches and supported explicit configuration.
- Extends supported scene exclusions to the recovered music, while keeping regional, quest and boss cues in their intended roles.
- Isolates a shared track's conditions by playlist, so protecting a story scene does not accidentally silence its use elsewhere.
- Optionally adds verified HQ original recordings alongside the winning music replacer.

## How the merge works

Music mod A adds two tracks to a forest playlist. Music mod B adds three to the same playlist. Skyrim normally uses the winning version of that playlist, which can leave A's additions out.

Harmonia reads the enabled plugins through the game's native file interface at startup and compares their authored playlist memberships. It restores eligible independent additions in memory. A's two tracks and B's three can then participate in the same playlist under the game's normal selection rules.

This is not filename guessing. Tracks are assigned by actual music records and supported configuration. A deliberate removal by a patch that depends on the source mod is retained. Explicit playlist clears reserve that list for explicitly configured content, including an intentionally empty list. Ordinary combat recovery keeps the winning priority and fade settings.

## When the local soundtrack takes over

Suppose a regional mod adds a special valley theme and tells ordinary exploration music to stay out of that valley during a quest. Harmonia looks for the related dedicated cue and the authored exclusion. It then carries the supported shared restriction over to eligible additions in the affected playlist.

The valley keeps its own music during that scene. Ordinary music becomes eligible again when the authored restriction ends. A track's own night-only or other baseline conditions remain intact.

Harmonia preserves complete condition groups and their AND/OR relationships. Shared tracks receive playlist-local instances, keeping unrelated uses intact. Dedicated and one-shot playlists are protected from general additions; a boss membership is restored only when that exact membership was authored. Supported explicit rules take priority over automatic inference.

## Original music alongside replacers

The FOMOD installer includes an optional **Original Audio HQ** checkbox, off by default.

Select it to install independently named original recordings and their verified main-track/finale manifest. Where supported, Harmonia adds the original as another candidate beside the final winning replacement, including replacers such as Lorkhan. Both follow the playlist's scene restrictions. Recognized original copies already present in the same playlist suppress extra Harmonia copies.

Leave it unchecked to use playlist integration and scene protection without adding original audio. The DLL does not extract game archives or build an audio cache. If two replacers overwrite the same file path, your mod manager still decides which replacement wins; Harmonia does not recover every overwritten replacement.

The optional pack contains 239 audio files. It covers the verified source material, not every soundtrack file ever supplied with the game. Unsupported or unavailable original resources remain as currently installed.

## Requirements and installation

**This build supports Steam Skyrim 1.7.104 only**, with SKSE 2.3.1 and the matching Address Library. Other executable versions have not been enabled.

1. Install your music and regional mods normally.
2. Install the Harmonia FOMOD with Vortex or MO2.
3. Optionally select Original Audio HQ.
4. Keep your music assets and configuration files, then launch through SKSE.

Harmonia does not require MTD or CME. It supports existing-type MTD track, clear, Location and Region instructions and supported CME combat/cleared-dungeon selectors. Disable the standalone MTD and CME DLLs when using Harmonia to perform that work; detection of either pauses Harmonia's writes to avoid two managers changing the same state.

## Compatibility and scope

Harmonia is an automatic compatibility tool, not a guarantee for every scripted music system. Runtime-created music types, unsupported configuration syntax, complex scripted switching and completely independent combat libraries without an authored relationship may still require the original framework or a specific patch. Missing music assets cannot be repaired by playlist merging. Ambiguous scene evidence is left unresolved rather than used to rewrite dedicated cues.

The 1.0.1 native-reader build preserves the offline plans from 1.0.0. Checks against a 111-plugin setup included Lorkhan, Chapter II, Northerner Diaries, Beyond Skyrim: Bruma, Extended Cut: Saints & Seducers and SIRENROOT. The FOMOD's core-only and HQ selections were also checked through Vortex's installed installer engine. MO2 compatibility was checked structurally. **In-game playback, transitions and save/reload behavior still need validation.**

[SKSE](https://skse.silverlock.org/) · [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

## Credits and disclosure

Includes adaptations of ThirdEyeSqueegee's [Music Type Distributor](https://github.com/stop-sine/MusicTypeDistributor-RE) and SeaSparrowOG's [Combat Music Extender](https://github.com/SeaSparrowOG/CombatMusic). Optional audio comes from MadAborModding's [Music Replacer Reverter](https://www.nexusmods.com/skyrimspecialedition/mods/137806), sourced from sharrken's [UHDAP](https://www.nexusmods.com/skyrimspecialedition/mods/18115); original music by Bethesda's soundtrack creators, including Jeremy Soule. [Dependency credits and license notices](https://github.com/dickmna/UnifiedMusic/blob/main/PROVENANCE.md).

Generative AI (ChatGPT/Codex) was used to produce the custom implementation, page text and cover artwork. The optional original soundtrack is not AI-generated.

[GitHub — source code, build instructions and validation reports](https://github.com/dickmna/UnifiedMusic)
