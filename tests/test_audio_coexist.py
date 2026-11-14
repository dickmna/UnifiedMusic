"""Optional-pack activation, content deduplication and atomic main/finale handling."""
import hashlib
import json
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


def audio_pack(root, resources, *, pairs=None, baselines=None):
    assets = {}
    for source, data in resources.items():
        digest = hashlib.sha256(data).hexdigest()
        name = 'music/harmonia/original/' + digest + '.xwm'
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        baseline = (baselines or {}).get(source, data)
        assets[source] = {'file': name, 'sha256': digest,
                          'equivalent_sha256': [hashlib.sha256(baseline).hexdigest()]}
    manifest = root / 'SKSE/Plugins/HarmoniaOriginalAudio.json'
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps({'format': 1, 'assets': assets,
        'pairs': pairs if pairs is not None else [[s, ''] for s in resources]}), encoding='utf-8')
    return manifest


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
        self.visible.mkdir()
        self.pack = self.root / 'pack'
        self.original = xwm()
        self.hq = xwm(b'higher-quality-same-composition')
        self.replacement = xwm(b'different-replacement')
        self.main = 'music/fixture/battle.xwm'
        self.manifest = audio_pack(self.pack, {self.main: self.hq}, baselines={self.main: self.original})
        self.base = self.write('Skyrim.esm', header() + music(0x100, (0x200,)) + audio_track())
        self.override(self.original)

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def override(self, content=None, name='music/fixture/battle.xwm'):
        self.write('visible/' + name, self.replacement if content is None else content)

    def scan(self, *plugins, pack=True, success=True, configs=(), extra=()):
        args = [str(self.exe), str(self.base), *map(str, plugins), '--visible-dir', str(self.visible)]
        if pack:
            args += ['--audio-pack', str(self.pack)]
        for config in configs:
            args += ['--config', str(config)]
        result = subprocess.run(args + list(extra), capture_output=True, text=True, encoding='utf-8', timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def snapshot(self):
        return {str(p.relative_to(self.root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in self.root.rglob('*') if p.is_file()}

    def test_no_pack_never_restores_originals(self):
        self.override()
        self.assertIn('AUDIO_DISABLED ', self.scan(pack=False))
        self.manifest.unlink()
        self.assertIn('AUDIO_DISABLED ', self.scan())

    def test_known_vanilla_and_hq_do_not_get_extra_weight(self):
        for content in (self.original, self.hq):
            self.override(content)
            out = self.scan()
            self.assertIn('AUDIO_DUPLICATE 1', out)
            self.assertNotIn('AUDIO_COEXIST ', out)

    def test_distinct_replacer_uses_optional_audio_without_writes(self):
        self.override()
        before = self.snapshot()
        out = self.scan()
        self.assertIn('AUDIO_COEXIST skyrim.esm:256 <- skyrim.esm:512 main=' + hashlib.sha256(self.hq).hexdigest(), out)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.scan(), out)
        self.assertEqual(self.snapshot(), before)

    def test_explicit_clear_does_not_restore_audio_or_touch_other_owner(self):
        self.base.write_bytes(header()+music(0x100,(0x200,))+music(0x101,(0x200,),name='MUSExploreSnow')+audio_track())
        self.override()
        config=self.write('Quiet_MUS.ini',b'[General]\nMUSExploreForest! =\n')
        out=self.scan(configs=(config,))
        self.assertNotIn('AUDIO_COEXIST skyrim.esm:256',out)
        self.assertIn('AUDIO_COEXIST skyrim.esm:257',out)

    def test_invalid_winning_resource_is_not_used(self):
        for content in (b'not-music', self.replacement[:-1], self.replacement.replace(b'XWMA', b'WAVE')):
            self.override(content)
            self.assertIn('AUDIO_SKIP ', self.scan())

    def test_corrupt_or_missing_pack_file_only_disables_that_pair(self):
        entry=json.loads(self.manifest.read_text())['assets'][self.main]
        asset=self.pack/entry['file']
        self.override()
        asset.write_bytes(xwm(b'valid-but-wrong-song'))
        self.assertIn('missing/changed optional', self.scan())
        asset.unlink()
        self.assertIn('missing/changed optional', self.scan())

    def test_wav_alternative_is_not_mistaken_for_xwm_precedence(self):
        self.override()
        self.override(b'RIFF actual wav', 'music/fixture/battle.wav')
        self.assertIn('AUDIO_SKIP ', self.scan())

    def test_main_and_finale_are_a_verified_atomic_pair(self):
        end='music/fixture/finale.xwm'
        finale=xwm(b'finale-original')
        self.base.write_bytes(header()+music(0x100,(0x200,))+audio_track(finale=end))
        self.override()
        self.override(finale,end)
        self.assertIn('no verified main/finale mapping',self.scan())
        audio_pack(self.pack,{self.main:self.hq,end:finale},pairs=[[self.main,end]])
        out=self.scan()
        self.assertIn('finale='+hashlib.sha256(finale).hexdigest(),out)
        (self.pack/json.loads(self.manifest.read_text())['assets'][end]['file']).unlink()
        self.assertNotIn('AUDIO_COEXIST ',self.scan())

    def test_changed_finale_alone_preserves_both_optional_resources(self):
        end='music/fixture/finale.xwm'
        finale=xwm(b'original-finale')
        self.base.write_bytes(header()+music(0x100,(0x200,))+audio_track(finale=end))
        audio_pack(self.pack,{self.main:self.hq,end:finale},pairs=[[self.main,end]],baselines={self.main:self.original})
        self.override(self.replacement,end)
        self.assertIn('AUDIO_COEXIST ',self.scan())
        (self.visible/end).unlink()
        self.assertIn('missing/invalid winning',self.scan())

    def test_existing_copy_in_list_or_explicit_config_suppresses_duplicate(self):
        self.override()
        for blob in (self.original,self.hq):
            self.override(blob,'music/other/copy.xwm')
            copy=audio_track(0x01000800,path='music/other/copy.xwm')
            patch=self.write('Copy.esp',header(('Skyrim.esm',))+copy+music(0x100,(0x200,0x01000800)))
            self.assertIn('AUDIO_DUPLICATE 1',self.scan(patch))
            patch.write_bytes(header(('Skyrim.esm',))+copy)
            config=self.write('Copy_MUS.ini',b'[General]\nMUSExploreForest = 0x800~Copy.esp\n')
            self.assertIn('AUDIO_DUPLICATE 1',self.scan(patch,configs=(config,)))

    def test_two_wrappers_of_same_audio_only_get_one_optional_use_per_list(self):
        self.base.write_bytes(header()+music(0x100,(0x200,0x201))+audio_track()+audio_track(0x201))
        self.override()
        out=self.scan()
        self.assertEqual(out.count('AUDIO_COEXIST '),1)
        self.assertIn('AUDIO_DUPLICATE 1',out)

    def test_bad_manifest_cannot_redirect_assets_or_break_record_recovery(self):
        self.override()
        good=json.loads(self.manifest.read_text())
        for path in ('music/../escape.xwm','music/vanilla.xwm','music/harmonia/original/../escape.xwm'):
            bad=json.loads(json.dumps(good));bad['assets'][self.main]['file']=path
            self.manifest.write_text(json.dumps(bad))
            self.assertIn('AUDIO_PACK_SKIP ',self.scan())
            self.assertNotIn('AUDIO_COEXIST ',self.scan())
        self.manifest.write_text('{"format":1,"format":1}')
        self.assertIn('AUDIO_PACK_SKIP ',self.scan())

    def test_missing_optional_asset_does_not_block_an_independent_pair(self):
        other='music/fixture/other.xwm'
        audio_pack(self.pack,{self.main:self.hq,other:xwm(b'other-original')})
        self.base.write_bytes(header()+music(0x100,(0x200,0x201))+audio_track()+audio_track(0x201,path=other))
        self.override();self.override(xwm(b'other-replacement'),other)
        asset=json.loads(self.manifest.read_text())['assets'][self.main]['file']
        (self.pack/asset).unlink()
        out=self.scan()
        self.assertNotIn('AUDIO_COEXIST skyrim.esm:256 <- skyrim.esm:512',out)
        self.assertIn('AUDIO_COEXIST skyrim.esm:256 <- skyrim.esm:513',out)

    def test_same_main_with_a_different_finale_is_not_a_complete_copy(self):
        end='music/fixture/finale.xwm'
        self.base.write_bytes(header()+music(0x100,(0x200,))+audio_track(finale=end))
        audio_pack(self.pack,{self.main:self.hq,end:xwm(b'finale')},pairs=[[self.main,end]])
        self.override();self.override(xwm(b'replacement-finale'),end)
        self.override(self.hq,'music/other/copy.xwm')
        self.override(xwm(b'unrelated-finale'),'music/other/finale.xwm')
        patch=self.write('Copy.esp',header(('Skyrim.esm',))+music(0x100,(0x200,0x01000800))+
                         audio_track(0x01000800,path='music/other/copy.xwm',finale='music/other/finale.xwm'))
        self.assertIn('AUDIO_COEXIST ',self.scan(patch))

    def test_unverified_or_duplicate_pair_and_digest_are_rejected(self):
        good=json.loads(self.manifest.read_text())
        changes=[lambda d:d['pairs'].append(d['pairs'][0]),
                 lambda d:d['pairs'].__setitem__(0,[self.main,'music/wrong.xwm']),
                 lambda d:d['assets'][self.main].__setitem__('sha256','not-a-hash')]
        for change in changes:
            bad=json.loads(json.dumps(good));change(bad)
            self.manifest.write_text(json.dumps(bad))
            self.assertIn('AUDIO_PACK_SKIP ',self.scan())

    def test_retired_archive_and_disk_cache_options_are_rejected(self):
        for option in ('--original-archive','--audio-cache'):
            self.assertIn('unknown option',self.scan(success=False,extra=(option,str(self.root))))

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
            self.assertIn('unsupported music path', self.scan())


if __name__ == '__main__':
    unittest.main()
