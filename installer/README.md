# Vortex / MO2 package

`fomod/ModuleConfig.xml` uses the standard XML schema 5 installer. `00 Core` is always copied to the Data root. The optional `10 Original Audio` folder is selected by one unchecked-by-default checkbox and includes both the audio and manifest. There is no extra Data wrapper, ESP, installer executable or script.

Build from the already verified release archives:

```powershell
python tools/package_fomod.py --core Harmonia-1.0.1.zip --audio Harmonia-Original-Audio-HQ-1.0.0.zip --manifest Harmonia-1.0.1-sha256.json --output Harmonia-1.0.1-FOMOD.zip
```

The tool checks both input SHA-256 values, rejects unsafe/colliding paths, preserves every payload byte and verifies the resulting archive. Its adjacent JSON report includes every installation destination. Audio and DLLs stay outside Git.

Validation on 2026-11-15 used the installed Vortex `@nexusmods/fomod-installer-native` 0.13.4 engine with XML validation enabled. Both choices returned `Installation successful`: 16 core files; 258 files with the optional pack. Every copy destination exactly matched the packaging report, and the option was unselected initially. The harness operated only on an extracted staging directory and did not deploy files to game Data. MO2 was not installed; its compatibility assessment is based on the shared standard FOMOD format and Data-level destinations.
