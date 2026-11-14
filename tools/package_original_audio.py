"""Developer packaging only: verified, already-extracted MRR files -> optional ZIP.

No BSA reader, game-directory access, conversion or runtime generation.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import zipfile


def relative(value):
    path = PurePosixPath(value)
    if path.is_absolute() or '..' in path.parts or ':' in value or '\\' in value:
        raise ValueError('Unsafe resource path')
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Extracted MRR Data root')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    manifest_path = repo / 'audio-pack/HarmoniaOriginalAudio.json'
    audit_path = repo / 'tests/audio-pack-results.json'
    audit = json.loads(audit_path.read_text(encoding='utf-8'))
    manifest_text = manifest_path.read_text(encoding='utf-8')
    # This pins the audited main/finale relationships as well as the asset map.
    # Re-audit changed references before distributing an updated resource pack.
    if hashlib.sha256(manifest_text.encode('utf-8')).hexdigest() != audit['manifest_sha256']:
        raise ValueError('Audio references changed since their source audit')
    manifest = json.loads(manifest_text)
    if set(manifest['assets']) != set(audit['mapping']):
        raise ValueError('Manifest and source mapping differ')
    entries = {}
    for original, asset in manifest['assets'].items():
        source = audit['mapping'][original]
        path = args.source / relative(source['source'])
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != source['sha256'] or digest != asset['sha256']:
            raise ValueError(f'Source audio differs: {path}')
        target = relative(asset['file']).as_posix()
        if not target.startswith('music/harmonia/original/') or not target.endswith('.xwm'):
            raise ValueError('Audio must use the independent namespace')
        entries.setdefault(target, path)  # Identical verified bytes may have multiple source aliases.
    if len(entries) != audit['unique_files']:
        raise ValueError('Audio file count differs')
    entries.update({
        'SKSE/Plugins/HarmoniaOriginalAudio.json': manifest_path,
        'Harmonia-Original-Audio/README.md': repo / 'audio-pack/README.md',
        'Harmonia-Original-Audio/audio-pack-results.json': audit_path,
    })
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, path in sorted(entries.items()):
            archive.write(path, name)
    with zipfile.ZipFile(args.output) as archive:
        if archive.testzip() is not None or set(archive.namelist()) != set(entries):
            raise ValueError('Pack verification failed')
        for name, path in entries.items():
            if archive.read(name) != path.read_bytes():
                raise ValueError(f'Pack content differs: {name}')
    print(json.dumps({'file': args.output.name, 'audio_files': audit['unique_files'],
                      'bytes': args.output.stat().st_size,
                      'sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    main()
