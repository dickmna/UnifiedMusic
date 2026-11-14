"""A source archive must preserve the audited main/finale references exactly."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from test_audio_coexist import audio_pack, xwm


class AudioPackagingTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='Harmonia-package-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / 'extracted'
        self.source.mkdir()
        self.output = self.root / 'optional.zip'
        self.main = 'music/fixture/main.xwm'
        self.end = 'music/fixture/end.xwm'
        resources = {self.main: xwm(b'main'), self.end: xwm(b'finale')}
        self.manifest = self.root / 'audio-pack/HarmoniaOriginalAudio.json'
        self.manifest.parent.mkdir()
        generated = audio_pack(self.root / 'fixture', resources, pairs=[[self.main, self.end]])
        self.manifest.write_text(json.dumps(json.loads(generated.read_text()), indent=2) + '\n', encoding='utf-8')
        (self.manifest.parent / 'README.md').write_text('Source credits', encoding='utf-8')
        self.audit = self.root / 'tests/audio-pack-results.json'
        self.audit.parent.mkdir()
        mapping = {}
        for name, data in resources.items():
            filename = Path(name).name
            (self.source / filename).write_bytes(data)
            mapping[name] = {'source': filename, 'sha256': hashlib.sha256(data).hexdigest()}
        self.audit.write_text(json.dumps({
            'manifest_sha256': hashlib.sha256(self.manifest.read_text(encoding='utf-8').encode()).hexdigest(),
            'mapping': mapping, 'unique_files': 2,
        }), encoding='utf-8')
        self.tool = self.root / 'tools/package_original_audio.py'
        self.tool.parent.mkdir()
        shutil.copyfile(Path(__file__).resolve().parent.parent / 'tools/package_original_audio.py', self.tool)

    def package(self):
        return subprocess.run([sys.executable, str(self.tool), '--source', str(self.source),
                               '--output', str(self.output)], capture_output=True, text=True, timeout=30)

    def test_verified_audio_and_credits_are_packaged_byte_for_byte(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = json.loads(self.manifest.read_text())
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(archive.read('SKSE/Plugins/HarmoniaOriginalAudio.json'), self.manifest.read_bytes())
            self.assertEqual(len(archive.namelist()), 5)
            for original, entry in manifest['assets'].items():
                self.assertEqual(archive.read(entry['file']), (self.source / Path(original).name).read_bytes())

    def test_lf_and_crlf_checkout_have_the_same_audited_references(self):
        self.manifest.write_bytes(self.manifest.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_changed_pair_rejected_even_when_all_files_still_match(self):
        manifest = json.loads(self.manifest.read_text())
        manifest['pairs'] = [[self.main, ''], [self.end, '']]
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        self.output.write_bytes(b'previous release')
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('references changed', result.stderr)
        self.assertEqual(self.output.read_bytes(), b'previous release')

    def test_changed_source_audio_is_rejected_before_output(self):
        (self.source / 'end.xwm').write_bytes(xwm(b'wrong finale'))
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Source audio differs', result.stderr)
        self.assertFalse(self.output.exists())

    def test_missing_finale_is_rejected_before_output(self):
        (self.source / 'end.xwm').unlink()
        self.assertNotEqual(self.package().returncode, 0)
        self.assertFalse(self.output.exists())
