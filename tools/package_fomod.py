"""Combine verified gameplay/HQ archives into a Vortex/MO2 XML installer.

The runtime DLL and all audio bytes are retained unchanged. No game files are
read. Both source ZIPs must match a previously recorded release manifest.
"""
from pathlib import Path, PurePosixPath
import argparse
import hashlib
import json
import zipfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(part)
    return digest.hexdigest()

def checked_names(archive):
    names = [i.filename for i in archive.infolist() if not i.is_dir()]
    if len({n.casefold() for n in names}) != len(names):
        raise ValueError('Duplicate archive destination')
    for name in names:
        p = PurePosixPath(name)
        if p.is_absolute() or '..' in p.parts or '\\' in name or ':' in name:
            raise ValueError('Unsafe archive path')
        if name.casefold().startswith('data/'):
            raise ValueError('Unexpected Data wrapper')
    return names

def package(core, audio, manifest, output):
    known = json.loads(manifest.read_text(encoding='utf-8'))['artifacts']
    for path in (core, audio):
        if sha(path) != known[path.name]['sha256']:
            raise ValueError('Source archive differs from verified release: ' + path.name)
    config = ROOT / 'installer/fomod/ModuleConfig.xml'
    info = ROOT / 'installer/fomod/info.xml'
    tree = ET.parse(config)
    assert tree.find('requiredInstallFiles/folder').attrib == {'source': '00 Core', 'destination': '', 'priority': '0'}
    option = tree.find('installSteps/installStep/optionalFileGroups/group')
    assert option.get('type') == 'SelectAny'
    assert option.find('plugins/plugin/typeDescriptor/type').get('name') == 'Optional'
    assert option.find('plugins/plugin/files/folder').attrib == {'source': '10 Original Audio', 'destination': '', 'priority': '0'}
    ET.parse(info)
    output.parent.mkdir(parents=True, exist_ok=True)
    expected = {}
    with zipfile.ZipFile(core) as base, zipfile.ZipFile(audio) as hq:
        base_names, hq_names = checked_names(base), checked_names(hq)
        assert 'SKSE/Plugins/UnifiedMusic.dll' in base_names
        assert not any(n.lower().endswith(('.xwm', '.esp', '.esm', '.esl')) for n in base_names)
        assert 'SKSE/Plugins/HarmoniaOriginalAudio.json' in hq_names
        assert sum(n.lower().endswith('.xwm') for n in hq_names) == 239
        assert not {n.casefold() for n in base_names} & {n.casefold() for n in hq_names}
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as out:
            for archive, names, prefix in ((base, base_names, '00 Core/'), (hq, hq_names, '10 Original Audio/')):
                for name in names:
                    payload = archive.read(name)
                    dest = prefix + name
                    out.writestr(dest, payload)
                    expected[dest] = hashlib.sha256(payload).hexdigest()
            for path in (config, info):
                payload = path.read_bytes()
                dest = 'fomod/' + path.name
                out.writestr(dest, payload)
                expected[dest] = hashlib.sha256(payload).hexdigest()
    with zipfile.ZipFile(output) as out:
        assert out.testzip() is None
        assert set(checked_names(out)) == set(expected)
        assert all(hashlib.sha256(out.read(n)).hexdigest() == digest for n, digest in expected.items())
    result = {'archive': output.name, 'sha256': sha(output), 'bytes': output.stat().st_size,
              'core_files': len(base_names), 'optional_files': len(hq_names), 'audio_files': 239,
              'default_audio_selected': False, 'all_payload_bytes_unchanged': True,
              'root_fomod_config': True, 'no_data_wrapper': True,
              'core_only_destinations': base_names, 'with_audio_destinations': base_names + hq_names}
    output.with_suffix('.verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return result

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('core', 'audio', 'manifest', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    result = package(args.core, args.audio, args.manifest, args.output)
    print(json.dumps({k:v for k,v in result.items() if not k.endswith('_destinations')}, indent=2))
