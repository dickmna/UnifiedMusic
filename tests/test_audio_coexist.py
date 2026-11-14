"""Synthetic BSA/plugin/resource pairs. RIFF fixtures test structure, not decoding."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_plugin_scan import header, music, record, script_reference, sub
from test_auto_combat_scan import condition, single_track


def xwm(payload=b'original-data'):
    fmt = struct.pack('<HHIIHH', 0x161, 2, 44100, 16000, 2048, 16)
    chunks = b'fmt ' + struct.pack('<I', len(fmt)) + fmt
    chunks += b'data' + struct.pack('<I', len(payload)) + payload + (b'\0' if len(payload) & 1 else b'')
    return b'RIFF' + struct.pack('<I', len(chunks) + 4) + b'XWMA' + chunks


def bsa(entries, *, compressed=False, embedded=False):
    groups = {}
    for name, content in entries:
        folder, filename = name.rsplit('/', 1)
        groups.setdefault(folder, []).append((filename, content))
    names = b''.join(name.encode() + b'\0' for rows in groups.values() for name, _ in rows)
    block_start = 36 + 24 * len(groups)
    data_start = block_start + sum(2 + len(folder.encode()) + 16 * len(rows) for folder, rows in groups.items()) + len(names)
    folder_records, blocks, payloads = b'', b'', b''
    for folder, rows in groups.items():
        folder_records += struct.pack('<QIIQ', 0, len(rows), 0, block_start + len(blocks) + len(names))
        blocks += bytes([len(folder.encode()) + 1]) + folder.encode() + b'\0'
        for name, content in rows:
            if embedded:
                content = bytes([len(name)]) + name.encode() + content
            blocks += struct.pack('<QII', 0, len(content) | (0x40000000 if compressed else 0), data_start + len(payloads))
            payloads += content
    h = b'BSA\0' + struct.pack('<8I', 105, 36, 3 | (0x100 if embedded else 0), len(groups), len(entries),
                               sum(len(s) + 1 for s in groups), len(names), 16)
    return h + folder_records + blocks + names + payloads


def audio_track(fid=0x200, *, path=r'Data\Music\Fixture\battle.wav', finale='', terms=(), flags=0):
    body = sub('EDID', b'OriginalSingle\0') + sub('CNAM', struct.pack('<I', 0x6ED7E048))
    body += sub('ANAM', path.encode() + b'\0')
    if finale:
        body += sub('BNAM', finale.encode() + b'\0')
    body += sub('CITC', struct.pack('<I', len(terms))) + b''.join(sub('CTDA', term) for term in terms)
    return record('MUST', fid, body, flags=flags)


class AudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = Path(os.environ['UNIFIED_MUSIC_INSPECT']).resolve()

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='Harmonia-audio-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.visible = self.root / 'visible'
        self.cache = self.root / 'cache'
        self.visible.mkdir(); self.cache.mkdir()
        self.original = xwm()
        self.replacement = xwm(b'different-replacement')
        self.archive = self.write('Original.bsa', bsa([('music/fixture/battle.xwm', self.original)]))
        self.base = self.write('Skyrim.esm', header() + music(0x100, (0x200,)) + audio_track())

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def override(self, content=None, name='music/fixture/battle.xwm'):
        path = self.visible / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(self.replacement if content is None else content)

    def scan(self, *plugins, cache=False, archives=None, success=True, configs=()):
        args = [str(self.exe), str(self.base), *map(str, plugins)]
        for archive in archives or [self.archive]:
            args += ['--original-archive', str(archive)]
        args += ['--visible-dir', str(self.visible)]
        for config in configs:
            args += ['--config',str(config)]
        if cache:
            args += ['--audio-cache', str(self.cache)]
        result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_unchanged_original_does_not_create_extra_weight_or_cache(self):
        out = self.scan(cache=True)
        self.assertIn('AUDIO_UNCHANGED 1', out)
        self.assertNotIn('AUDIO_COEXIST ', out)
        self.assertFalse(list(self.cache.rglob('*.xwm')))

    def test_explicit_clear_does_not_restore_original_audio_or_touch_other_owner(self):
        self.base.write_bytes(header()+music(0x100,(0x200,))+music(0x101,(0x200,),name='MUSExploreSnow')+audio_track())
        self.override()
        config=self.write('Quiet_MUS.ini',b'[General]\nMUSExploreForest! =\n')
        out=self.scan(cache=True,configs=(config,))
        self.assertIn('CLEAR skyrim.esm:256',out)
        self.assertNotIn('AUDIO_COEXIST skyrim.esm:256',out)
        self.assertIn('AUDIO_COEXIST skyrim.esm:257',out)

    def test_distinct_replacer_keeps_source_and_caches_exact_original(self):
        self.override()
        before = self.archive.read_bytes(), self.base.read_bytes()
        out = self.scan(cache=True)
        digest = hashlib.sha256(self.original).hexdigest()
        self.assertIn('AUDIO_COEXIST skyrim.esm:256 <- skyrim.esm:512 main=' + digest, out)
        self.assertEqual((self.cache / 'music/harmonia' / (digest + '.xwm')).read_bytes(), self.original)
        self.assertEqual(before, (self.archive.read_bytes(), self.base.read_bytes()))
        self.assertEqual((self.visible / 'music/fixture/battle.xwm').read_bytes(), self.replacement)
        self.assertEqual(self.scan(cache=True), out)

    def test_invalid_winning_resource_skips_without_cache(self):
        for content in (b'not-music', self.replacement[:-1], self.replacement.replace(b'XWMA', b'WAVE')):
            self.override(content)
            self.assertIn('AUDIO_SKIP ', self.scan(cache=True))
        self.assertFalse(list(self.cache.rglob('*.xwm')))

    def test_invalid_original_resource_skips(self):
        self.archive.write_bytes(bsa([('music/fixture/battle.xwm', b'invalid')]))
        self.override()
        self.assertNotIn('AUDIO_COEXIST ', self.scan())

    def test_wav_alternative_is_not_mistaken_for_xwm_precedence(self):
        self.override()
        self.override(b'RIFF actual wav', 'music/fixture/battle.wav')
        self.assertIn('AUDIO_SKIP ', self.scan())

    def test_compressed_member_is_never_read_as_uncompressed_audio(self):
        self.archive.write_bytes(bsa([('music/fixture/battle.xwm', self.original)], compressed=True))
        self.override()
        self.assertNotIn('AUDIO_COEXIST ', self.scan())

    def test_embedded_member_names_are_removed_before_validation(self):
        self.archive.write_bytes(bsa([('music/fixture/battle.xwm', self.original)], embedded=True))
        self.override()
        self.assertIn('AUDIO_COEXIST ', self.scan())

    def test_duplicate_original_archives_are_ambiguous(self):
        second = self.write('Second.bsa', self.archive.read_bytes())
        self.override()
        self.assertNotIn('AUDIO_COEXIST ', self.scan(archives=[self.archive, second]))

    def test_duplicate_members_and_unsafe_archive_paths_are_rejected(self):
        for names in (['music/fixture/battle.xwm'] * 2, ['music/../escape.xwm']):
            self.archive.write_bytes(bsa([(name, self.original) for name in names]))
            self.assertIn('unsafe/duplicate', self.scan(success=False))

    def test_truncated_and_out_of_bounds_archives_are_rejected(self):
        valid = self.archive.read_bytes()
        for content in (valid[:35], valid[:-1], valid[:16] + struct.pack('<I', 1000000) + valid[20:]):
            self.archive.write_bytes(content)
            self.scan(success=False)

    def test_main_and_finale_are_an_atomic_pair(self):
        finale = xwm(b'finale-original')
        self.base.write_bytes(header() + music(0x100, (0x200,)) + audio_track(finale=r'Data\Music\Fixture\finale.wav'))
        self.override()
        self.assertNotIn('AUDIO_COEXIST ', self.scan(cache=True))
        self.assertFalse(list(self.cache.rglob('*.xwm')))
        self.archive.write_bytes(bsa([('music/fixture/battle.xwm', self.original), ('music/fixture/finale.xwm', finale)]))
        self.assertIn('finale=' + hashlib.sha256(finale).hexdigest(), self.scan(cache=True))
        self.assertEqual(len(list(self.cache.rglob('*.xwm'))), 2)

    def test_changed_finale_alone_preserves_both_original_resources(self):
        self.base.write_bytes(header() + music(0x100, (0x200,)) + audio_track(finale=r'Data\Music\Fixture\finale.wav'))
        self.archive.write_bytes(bsa([('music/fixture/battle.xwm', self.original), ('music/fixture/finale.xwm', xwm(b'finale'))]))
        self.override(self.replacement, 'music/fixture/finale.xwm')
        self.assertIn('AUDIO_COEXIST ', self.scan(cache=True))
        self.assertEqual(len(list(self.cache.rglob('*.xwm'))), 2)

    def test_existing_corrupt_cache_is_not_overwritten(self):
        digest = hashlib.sha256(self.original).hexdigest()
        corrupt = self.write('cache/music/harmonia/' + digest + '.xwm', b'foreign-content')
        self.override()
        self.assertIn('existing audio cache differs', self.scan(cache=True))
        self.assertEqual(corrupt.read_bytes(), b'foreign-content')

    def test_stale_temporary_is_not_overwritten(self):
        digest = hashlib.sha256(self.original).hexdigest()
        temporary = self.write('cache/music/harmonia/' + digest + '.xwm.tmp', b'other-writer')
        self.override()
        self.assertIn('temporary unavailable', self.scan(cache=True))
        self.assertEqual(temporary.read_bytes(), b'other-writer')

    def test_redirected_cache_is_rejected_before_writing_outside_root(self):
        target = self.root / 'outside';target.mkdir()
        link = self.cache / 'music'
        p = subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(target)], capture_output=True)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.addCleanup(lambda: os.rmdir(link) if link.exists() else None)
        self.override()
        self.assertIn('redirected/non-directory', self.scan(cache=True))
        self.assertEqual(list(target.iterdir()), [])

    def test_audio_metadata_override_prevents_original_record_reconstruction(self):
        patch = self.write('Patch.esp', header(('Skyrim.esm',)) + audio_track(path=r'Data\Music\Other.wav'))
        self.override()
        self.assertIn('AUDIO_CANDIDATES 0 ', self.scan(patch))

    def test_condition_only_override_keeps_audio_candidate(self):
        patch = self.write('Patch.esp', header(('Skyrim.esm',)) + audio_track(terms=(condition(58, 0x300),)))
        self.override()
        self.assertIn('AUDIO_COEXIST ', self.scan(patch))

    def test_direct_scripted_track_oneshot_and_boss_lists_are_preserved(self):
        for name, flags, extra in [('MUSCombatBoss', 4, b''), ('MUSExploreForest', 5, b''),
                                   ('MUSExploreForest', 4, script_reference(0x300, 0x200))]:
            self.base.write_bytes(header() + music(0x100, (0x200,), name=name, music_flags=flags) + audio_track() + extra)
            self.override()
            self.assertIn('AUDIO_CANDIDATES 0 ', self.scan())

    def test_public_list_script_reference_does_not_block_audio_normalization(self):
        self.base.write_bytes(self.base.read_bytes() + script_reference(0x300, 0x100))
        self.override()
        self.assertIn('AUDIO_COEXIST ', self.scan())

    def test_palette_children_are_not_flattened(self):
        palette = record('MUST', 0x201, sub('CNAM', struct.pack('<I', 0x23F678C3)) + sub('SNAM', struct.pack('<I', 0x200)))
        self.base.write_bytes(header() + music(0x100, (0x200, 0x201)) + audio_track() + palette)
        self.override()
        self.assertIn('AUDIO_CANDIDATES 0 ', self.scan())

    def test_dragon_track_is_not_added_as_ordinary_audio(self):
        for comparison, expected in [(1, 0), (0, 1)]:
            self.base.write_bytes(header() + music(0x100, (0x200,), name='MUSCombat') +
                                  audio_track(terms=(condition(707, 0x35d59, comparison=comparison, run_on=2, reference=0x14),)))
            self.override()
            self.assertIn(f'AUDIO_CANDIDATES {expected} ', self.scan())

    def test_deleted_track_is_not_resurrected(self):
        patch = self.write('Patch.esp', header(('Skyrim.esm',)) + audio_track(flags=0x20))
        self.override()
        self.assertIn('AUDIO_CANDIDATES 0 ', self.scan(patch))

    def test_unsafe_record_paths_are_never_used_as_filesystem_targets(self):
        for path in ('Data/Music/../escape.wav', 'C:/outside.wav', 'Music/a:stream.xwm', 'Music/./battle.xwm'):
            self.base.write_bytes(header() + music(0x100, (0x200,)) + audio_track(path=path))
            self.override()
            self.assertIn('unsupported music path', self.scan(cache=True))
        self.assertFalse(list(self.cache.rglob('*.xwm')))


if __name__ == '__main__':
    unittest.main()
