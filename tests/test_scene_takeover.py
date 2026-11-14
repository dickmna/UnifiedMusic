"""Scene truth tables, normalized audio and typed references through the inspector.

Tiny audio fixtures validate resource identity/structure, not codec playback.
"""
import hashlib
import itertools
import json
import operator
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_plugin_scan import header, music, record, script_reference, sub
from test_auto_combat_scan import area, condition, single_track
from test_audio_coexist import audio_track, audio_pack, xwm


def rows(output, name):
    return [json.loads(s[len(name)+1:]) for s in output.splitlines() if s.startswith(name+' ')]


def permits(clauses, values):
    comparisons = (operator.eq, operator.ne, operator.gt, operator.ge, operator.lt, operator.le)
    return all(any(comparisons[t['op']](values[(t['function'], t['parameter'])],
                    struct.unpack('<f', struct.pack('<I', t['comparison_bits']))[0])
                   for t in clause) for clause in clauses)


class SceneFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = Path(os.environ['UNIFIED_MUSIC_INSPECT']).resolve()

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='Harmonia-scenes-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def fixture(self, *, scope=None, extra=b'', base_extra=b'', pack_extra=b'',
                ambient=False, role=(), exclude=True, player=False, restriction=None):
        name = 'MUSExploreForest' if ambient else 'MUSCombat'
        base = self.write('Skyrim.esm', header() + music(0x100, (0x200, 0x201), name=name)
            + audio_track(terms=role) + single_track(0x201, role, path=r'Data\Music\Fixture\other.wav') + base_extra)
        gate = condition(run_on=2 if player else 0, reference=0x14 if player else 0)
        ban = condition(comparison=0, run_on=2 if player else 0, reference=0x14 if player else 0)
        if restriction is None:
            restriction = (ban,) if exclude else ()
        land = self.write('Land.esp', header(('Skyrim.esm',)) + area('WRLD', 0x01000900)
            + music(0x100, (0x200, 0x201, 0x01000800), name=name)
            + audio_track(terms=role + restriction)
            + audio_track(0x01000800, path=r'Data\Music\Land\scene.wav',
                          terms=(gate,) if scope is None else scope) + extra)
        pack = self.write('Pack.esp', header(('Skyrim.esm',))
            + music(0x100, (0x200, 0x201, 0x01000800), name=name)
            + audio_track(0x01000800, path=r'Data\Music\Pack\added.wav', terms=role) + pack_extra)
        return base, land, pack

    def scan(self, *paths, audio=False, configs=()):
        args = [str(self.exe), *map(str, paths)]
        for config in configs:
            args += ['--config', str(config)]
        if audio:
            args += ['--audio-pack', str(self.root/'pack'), '--visible-dir', str(self.root/'visible')]
        proc = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', timeout=30)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout

    def resources(self, *, scene=True):
        audio_pack(self.root/'pack', {'music/fixture/battle.xwm': xwm(b'original'), 'music/fixture/other.xwm': xwm(b'other')})
        self.write('visible/music/fixture/other.xwm', xwm(b'other'))
        self.write('visible/music/fixture/battle.xwm', xwm(b'replacement'))
        if scene:
            self.write('visible/music/land/scene.xwm', xwm(b'scene'))

    def pack_rule(self, output):
        return next(r for r in rows(output, 'USE_RULES') if r['track']=='pack.esp:2048')


class SceneTests(SceneFixture):
    def test_partial_exclusion_establishes_takeover_and_originals_follow(self):
        out = self.scan(*self.fixture())
        rules = rows(out, 'USE_RULES')
        self.assertEqual({r['track'] for r in rules}, {'skyrim.esm:513', 'pack.esp:2048'})
        for r in rules:
            self.assertFalse(permits(r['clauses'], {(310, 'land.esp:2304'): 1}))
            self.assertTrue(permits(r['clauses'], {(310, 'land.esp:2304'): 0}))

    def test_lone_exclusion_without_scoped_music_does_not_take_over(self):
        out = self.scan(*self.fixture(scope=()))
        self.assertNotIn('SCENE_TAKEOVER', out)
        self.assertFalse(rows(out, 'USE_RULES'))
        self.assertIn('AUTO_COMBAT * <- land.esp:2048,pack.esp:2048', out)

    def test_scoped_music_without_related_exclusion_remains_a_mix(self):
        out = self.scan(*self.fixture(exclude=False))
        self.assertFalse(rows(out, 'USE_RULES'))

    def test_quest_interval_keeps_full_de_morgan_scope(self):
        scope = (condition(), condition(58, 0x01000A00, flags=0x60, comparison=40),
                 condition(58, 0x01000A00, flags=0x80, comparison=50))
        restriction = (condition(comparison=0, flags=1),
                       condition(58, 0x01000A00, flags=0x81, comparison=40),
                       condition(58, 0x01000A00, flags=0x60, comparison=50))
        out = self.scan(*self.fixture(scope=scope, restriction=restriction, extra=area('QUST', 0x01000A00)))
        clauses = self.pack_rule(out)['clauses']
        self.assertEqual(len(clauses), 1)
        self.assertEqual([t['or_next'] for t in clauses[0]], [True, True, False])
        for inside, stage in itertools.product((0, 1), (0, 39, 40, 49, 50, 90)):
            with self.subTest(inside=inside, stage=stage):
                self.assertEqual(permits(clauses, {(310, 'land.esp:2304'): inside, (58, 'land.esp:2560'): stage}),
                                 not (inside and 40 <= stage < 50))

    def test_multiple_geographic_gates_do_not_become_a_worldwide_ban(self):
        restriction=(condition(comparison=0, flags=1), condition(67, 0x01000901, comparison=0))
        out = self.scan(*self.fixture(scope=(condition(), condition(67, 0x01000901)), restriction=restriction, extra=area('CELL', 0x01000901)))
        clauses = self.pack_rule(out)['clauses']
        for world, cell in itertools.product((0, 1), repeat=2):
            self.assertEqual(permits(clauses, {(310, 'land.esp:2304'): world, (67, 'land.esp:2305'): cell}), not(world and cell))

    def test_time_gate_is_retained_without_reinterpreting_it_as_a_whole_region(self):
        restriction=(condition(comparison=0, flags=1), condition(18, 0, flags=0x80, comparison=20))
        out = self.scan(*self.fixture(scope=(condition(), condition(18, 0, flags=0x60, comparison=20)), restriction=restriction, ambient=True))
        clauses = self.pack_rule(out)['clauses']
        time_key = next(t['parameter'] for t in clauses[0] if t['function']==18)
        for inside, hour in itertools.product((0, 1), (0, 12, 19, 20, 23)):
            self.assertEqual(permits(clauses, {(310, 'land.esp:2304'): inside, (18, time_key): hour}), not(inside and hour>=20))

    def test_matching_player_reference_is_preserved(self):
        out = self.scan(*self.fixture(player=True))
        self.assertTrue(self.pack_rule(out)['clauses'][0][0]['player_reference'])

    def test_dependent_reversion_removes_the_only_takeover_evidence(self):
        paths = self.fixture()
        patch = self.write('Mix.esp', header(('Skyrim.esm', 'Land.esp')) + audio_track(terms=()))
        out = self.scan(*paths, patch)
        self.assertNotIn('skyrim.esm:512', {r['track'] for r in rows(out, 'USE_RULES')})
        self.assertFalse(rows(out, 'USE_RULES'))

    def test_explicit_positive_mix_is_preserved(self):
        paths = self.fixture()
        patch = self.write('Mix.esp', header(('Skyrim.esm', 'Land.esp'))
                           + single_track(0x201, (condition(),), path=r'Data\Music\Fixture\other.wav'))
        out = self.scan(*paths, patch)
        self.assertNotIn('skyrim.esm:513', {r['track'] for r in rows(out, 'USE_RULES')})
        self.assertTrue(self.pack_rule(out)['clauses'])

    def test_reordered_baseline_and_extra_quest_gate_still_supply_scene_evidence(self):
        role = (condition(707, 0x35D59, comparison=0),)
        paths = self.fixture(role=role)
        old = audio_track(terms=role+(condition(comparison=0),))
        new = audio_track(terms=(condition(comparison=0), condition(58, 0x01000A00, flags=0x20, comparison=40))+role)
        paths[1].write_bytes(paths[1].read_bytes().replace(old,new)+area('QUST', 0x01000A00))
        out = self.scan(*paths)
        self.assertTrue(self.pack_rule(out)['clauses'])

    def test_complex_scene_preserves_cue_or_and_inherits_reordered_original_policy(self):
        role = (condition(707, 0x35D59, comparison=0),)
        paths = self.fixture(role=role, scope=(condition(flags=1), condition(58, 0x01000A00)), extra=area('QUST',0x01000A00))
        old = audio_track(terms=role+(condition(comparison=0),))
        new = audio_track(terms=(condition(comparison=0), condition(58,0x01000A00,flags=0x20,comparison=40))+role)
        paths[1].write_bytes(paths[1].read_bytes().replace(old,new))
        out = self.scan(*paths)
        self.assertNotIn('SCENE_UNSUPPORTED ', out)
        self.assertEqual({t['function'] for c in self.pack_rule(out)['clauses'] for t in c}, {310,58})
        self.assertIn('APPEND skyrim.esm:256 <- land.esp:2048', out)
        self.assertNotIn('land.esp:2048', {r['track'] for r in rows(out,'USE_RULES')})

    def test_negative_term_in_or_group_is_inherited_as_a_complete_clause(self):
        paths = self.fixture()
        old = audio_track(terms=(condition(comparison=0),))
        new = audio_track(terms=(condition(58,0x01000A00,flags=1), condition(comparison=0)))
        paths[1].write_bytes(paths[1].read_bytes().replace(old,new)+area('QUST',0x01000A00))
        out = self.scan(*paths)
        clauses=self.pack_rule(out)['clauses']
        for inside,stage in itertools.product((0,1),(0,1,2)):
            self.assertEqual(permits(clauses,{(310,'land.esp:2304'):inside,(58,'land.esp:2560'):stage}), stage==1 or not inside)

    def test_authored_exclusion_union_keeps_explicit_player_context(self):
        paths = self.fixture(player=True)
        independent = self.write('Independent.esp', header(('Skyrim.esm',))+audio_track())
        out = self.scan(*paths,independent)
        original = next(r for r in rows(out,'USE_RULES') if r['track']=='skyrim.esm:512')
        self.assertEqual(len(original['clauses']), 1)
        self.assertTrue(original['clauses'][0][0]['player_reference'])

    def test_shared_boss_track_changes_only_ordinary_use(self):
        out = self.scan(*self.fixture(pack_extra=music(0x01000101, (0x01000800,), name='DedicatedBoss')))
        self.assertIn('ISOLATE_GUARD pack.esp:2048 IN skyrim.esm:256', out)
        self.assertEqual({r['type'] for r in rows(out, 'USE_RULES')}, {'skyrim.esm:256'})
        self.assertNotIn('APPEND pack.esp:257', out)

    def test_public_playlist_reference_is_not_exclusive_script_ownership(self):
        out = self.scan(*self.fixture(extra=script_reference(0x01000B00, 0x100)))
        self.assertTrue(self.pack_rule(out)['clauses'])

    def test_direct_track_reference_remains_author_owned(self):
        out = self.scan(*self.fixture(pack_extra=script_reference(0x01000B00, 0x01000800)))
        self.assertNotIn('pack.esp:2048', {r['track'] for r in rows(out, 'USE_RULES')})
        self.assertNotIn('AUTO_COMBAT * <- pack.esp:2048', out)

    def test_read_only_scene_or_does_not_block_authored_exclusions(self):
        base_extra = music(0x101, (), name='MUSTavern')
        pack_extra = music(0x101, (0x01000801,), name='MUSTavern') + single_track(0x01000801)
        out = self.scan(*self.fixture(scope=(condition(flags=1), condition(58, 0x01000A00)),
            extra=area('QUST', 0x01000A00), base_extra=base_extra, pack_extra=pack_extra))
        self.assertNotIn('SCENE_UNSUPPORTED ', out)
        self.assertTrue(self.pack_rule(out)['clauses'])
        self.assertIn('AUTO_COMBAT * <- pack.esp:2048', out)
        self.assertIn('APPEND skyrim.esm:257 <- pack.esp:2049', out)

    def test_missing_quest_definition_does_not_broaden_guard(self):
        out = self.scan(*self.fixture(scope=(condition(), condition(58, 0x01000A00))))
        self.assertIn('SCENE_UNSUPPORTED ', out)
        self.assertFalse(rows(out, 'USE_RULES'))
        self.assertNotIn('AUTO_COMBAT ', out)

    def test_normalized_audio_and_added_track_receive_the_same_scope(self):
        paths = self.fixture()
        # Force an independent override to hide the author's original exclusion.
        patch = self.write('Independent.esp', header(('Skyrim.esm',)) + audio_track())
        self.resources()
        out = self.scan(*paths, patch, audio=True)
        use, = rows(out, 'AUDIO_USE')
        self.assertEqual(use['clauses'], self.pack_rule(out)['clauses'])
        self.assertEqual(use['original'], hashlib.sha256(xwm(b'original')).hexdigest())
        self.assertEqual(use['replacement'], hashlib.sha256(xwm(b'replacement')).hexdigest())
        self.assertNotEqual(use['original'], use['replacement'])
        # A second startup produces the same plan without writing resources.
        before = {p.name: p.read_bytes() for p in (self.root/'pack').rglob('*.xwm')}
        self.assertEqual(self.scan(*paths, patch, audio=True), out)
        self.assertEqual({p.name: p.read_bytes() for p in (self.root/'pack').rglob('*.xwm')}, before)

    def test_missing_scene_audio_pauses_guard_and_audio_pair_together(self):
        paths = self.fixture()
        self.resources(scene=False)
        out = self.scan(*paths, audio=True)
        self.assertIn('SCENE_RESOURCE_SKIP ', out)
        self.assertFalse(rows(out, 'USE_RULES'))
        self.assertFalse(rows(out, 'AUDIO_USE'))
        self.assertNotIn('AUTO_COMBAT ', out)

    def test_missing_scene_finale_also_pauses_guard(self):
        paths = self.fixture()
        old = audio_track(0x01000800, path=r'Data\Music\Land\scene.wav', terms=(condition(),))
        new = audio_track(0x01000800, path=r'Data\Music\Land\scene.wav', finale=r'Data\Music\Land\finale.wav', terms=(condition(),))
        paths[1].write_bytes(paths[1].read_bytes().replace(old, new))
        self.resources()
        self.assertIn('SCENE_RESOURCE_SKIP ', self.scan(*paths, audio=True))

    def test_scan_order_has_no_effect_on_policy(self):
        paths = self.fixture()
        reference = sorted(rows(self.scan(*paths), 'USE_RULES'), key=lambda r:r['track'])
        for order in itertools.permutations(paths):
            self.assertEqual(sorted(rows(self.scan(*order), 'USE_RULES'), key=lambda r:r['track']), reference)


def string(value):
    return struct.pack('<H', len(value)) + value


def vmad_property(kind, value, *, status=1, fmt=2):
    return struct.pack('<HHH', 5, fmt, 1) + string(b'Script') + struct.pack('<BH', 0, 1) + string(b'Property') + bytes((kind, status)) + value


class TypedReferenceTests(SceneFixture):
    def typed_output(self, vmad):
        extra = record('QUST', 0x01000B00, sub('VMAD', vmad))
        return self.scan(*self.fixture(pack_extra=extra))

    def test_integer_and_string_bytes_do_not_become_form_references(self):
        raw = struct.pack('<I', 0x01000800)
        for kind, value in ((3, raw), (4, raw), (2, string(b'prefix'+raw+b'suffix'))):
            out = self.typed_output(vmad_property(kind, value))
            self.assertNotIn('VMAD_FALLBACK ', out)
            self.assertTrue(self.pack_rule(out)['clauses'])

    def test_object_arrays_and_format_one_protect_actual_references(self):
        for fmt, obj in ((2, struct.pack('<II', 0, 0x01000800)), (1, struct.pack('<II', 0x01000800, 0))):
            for kind, value in ((1, obj), (11, struct.pack('<I', 1)+obj)):
                out = self.typed_output(vmad_property(kind, value, fmt=fmt))
                self.assertNotIn('VMAD_FALLBACK ', out)
                self.assertNotIn('pack.esp:2048', {r['track'] for r in rows(out, 'USE_RULES')})

    def test_removed_object_property_is_not_live_ownership(self):
        out = self.typed_output(vmad_property(1, struct.pack('<II', 0, 0x01000800), status=3))
        self.assertTrue(self.pack_rule(out)['clauses'])

    def test_quest_alias_property_is_a_real_reference(self):
        main = struct.pack('<HHH', 5, 2, 0)
        trailer = struct.pack('<BH', 2, 0) + string(b'') + struct.pack('<H', 1)
        alias = struct.pack('<II', 0, 0) + vmad_property(1, struct.pack('<II', 0, 0x01000800))
        out = self.typed_output(main + trailer + alias)
        self.assertNotIn('VMAD_FALLBACK ', out)
        self.assertNotIn('pack.esp:2048', {r['track'] for r in rows(out, 'USE_RULES')})
