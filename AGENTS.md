# UnifiedMusic

- Keep a single DLL and an optional offline inspector. Do not add mod-name compatibility lists or redistribute game/Creation/music-pack assets.
- Preserve scripted/Boss music, track conditions and deliberate deletions; ambiguous cases must remain unchanged with a diagnostic.
- Build Release DLL and inspector, run synthetic scanner tests, and rerun real-plugin inspection when scanner behavior changes.
- Offline checks do not prove runtime hooks, playback or save/reload correctness. Record that distinction in VALIDATION.md.
- Keep pinned upstream licenses and provenance. Never silently broaden supported executable versions.
