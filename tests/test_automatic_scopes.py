"""Offline regressions for inferred regional guards and three-way assignments."""
import itertools
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_auto_combat_scan import area, condition, single_track
from test_plugin_scan import group, header, music, record, script_reference, sub


def assigned(sig, key, value=None, *, flags=0, extra=b''):
    field = {'CELL': 'XCMO', 'WRLD': 'ZNAM', 'LCTN': 'NAM1', 'REGN': 'RDMO'}[sig]
    body = sub('EDID', b'AreaFixture\0') + extra
    if value is not None:
        body += sub(field, struct.pack('<I', value))
    return record(sig, key, body, flags=flags)


class Fixture(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='Harmonia-scope-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.exe = os.environ['UNIFIED_MUSIC_INSPECT']

    def write(self, name, body):
        path = self.root / name
        path.write_bytes(body)
        return path

    def scan(self, *paths):
        result = subprocess.run([self.exe, *map(str, paths)], capture_output=True,
                                text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout


class MappingTests(Fixture):
    def fixture(self, sig='CELL', baseline=0x100, *, change=0x101,
                patch_value=None, patch_masters=('Skyrim.esm',), extra=b'', flags=0):
        base = self.write('Skyrim.esm', header() + assigned(sig, 0x900, baseline)
                          + music(0x100, (), name='OriginalMusic')
                          + music(0x101, (), name='NewLandStoryMusic')
                          + music(0x102, (), name='AnotherChoice'))
        author = self.write('Author.esp', header(('Skyrim.esm',)) + assigned(sig, 0x900, change))
        value = baseline if patch_value is None else patch_value
        other = self.write('Other.esp', header(patch_masters)
                           + assigned(sig, 0x900, value, flags=flags, extra=extra))
        return base, author, other

    def test_independent_lighting_override_preserves_authored_cell_music(self):
        output = self.scan(*self.fixture(extra=sub('XCLL', bytes(92))))
        self.assertIn('RESTORE_MAPPING 0:skyrim.esm:2304 <- skyrim.esm:257', output)

    def test_worldspace_and_location_fields(self):
        for sig, kind in [('WRLD', 1), ('LCTN', 2)]:
            with self.subTest(sig=sig):
                self.assertIn(f'RESTORE_MAPPING {kind}:', self.scan(*self.fixture(sig)))

    def test_null_and_absent_baselines(self):
        for value in (None, 0):
            with self.subTest(value=value):
                self.assertIn('RESTORE_MAPPING ', self.scan(*self.fixture(baseline=value)))

    def test_scan_order_does_not_decide_winner(self):
        for paths in itertools.permutations(self.fixture()):
            self.assertIn('RESTORE_MAPPING ', self.scan(*paths))

    def test_direct_reversion_is_respected(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(patch_masters=('Skyrim.esm', 'Author.esp'))))

    def test_transitive_reversion_is_respected(self):
        base, author, other = self.fixture(patch_masters=('Skyrim.esm', 'Bridge.esp'))
        bridge = self.write('Bridge.esp', header(('Skyrim.esm', 'Author.esp')))
        self.assertNotIn('RESTORE_MAPPING ', self.scan(base, author, bridge, other))

    def test_competing_new_assignments_are_not_chosen(self):
        base, author, other = self.fixture()
        rival = self.write('Rival.esp', header(('Skyrim.esm',)) + assigned('CELL', 0x900, 0x102))
        output = self.scan(base, author, rival, other)
        self.assertNotIn('RESTORE_MAPPING ', output)
        self.assertIn('competing assignments', output)

    def test_explicit_clear_is_not_undone(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(change=0)))

    def test_deletion_blocks_restoration(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(flags=0x20)))

    def test_ignored_partial_override_does_not_create_conflict(self):
        for flag in (0x1000, 0x4000):
            self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(flags=flag)))

    def test_duplicate_or_malformed_field_is_rejected(self):
        for payload in (b'x', struct.pack('<I', 0x100)):
            self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(extra=sub('XCMO', payload))))

    def test_region_rules_are_not_flattened(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture('REGN')))

    def test_missing_music_type_is_not_assigned(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(change=0x777)))

    def test_deleted_music_type_is_not_assigned(self):
        paths = self.fixture()
        deletion = self.write('Delete.esp', header(('Skyrim.esm',)) + music(0x101, (), flags=0x20))
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*paths, deletion))

    def test_new_land_and_master_slot_normalization(self):
        base = self.write('Skyrim.esm', header())
        land = self.write('Land.esm', header(('Skyrim.esm',)) + assigned('WRLD', 0x01000900)
                          + music(0x01000100, (), name='LandExplore'))
        author = self.write('Author.esp', header(('Land.esm', 'Skyrim.esm'))
                            + assigned('WRLD', 0x900, 0x100))
        other = self.write('Other.esp', header(('Skyrim.esm', 'Land.esm')) + assigned('WRLD', 0x01000900))
        self.assertIn('RESTORE_MAPPING 1:land.esm:2304 <- land.esm:256', self.scan(base, land, author, other))

    def test_already_agreeing_overrides_need_no_plan(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(patch_value=0x101)))

    def test_unchanged_ancestor_cannot_overwrite_its_dependent_author(self):
        base, author, other = self.fixture()
        author.write_bytes(header(('Skyrim.esm', 'Other.esp')) + assigned('CELL', 0x900, 0x101))
        self.assertNotIn('RESTORE_MAPPING ', self.scan(base, other, author))

    def test_missing_dependency_does_not_establish_independence(self):
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*self.fixture(patch_masters=('Skyrim.esm', 'Missing.esp'))))

    def test_dependency_cycle_is_rejected(self):
        paths = self.fixture(patch_masters=('Skyrim.esm', 'Bridge.esp'))
        bridge = self.write('Bridge.esp', header(('Skyrim.esm', 'Other.esp')))
        self.assertNotIn('RESTORE_MAPPING ', self.scan(*paths, bridge))


class GuardTests(Fixture):
    def fixture(self, *, sig='WRLD', all_excluded=True, replacement_conditions=None,
                generic_extra=b'', land_extra=b'', base_conditions=(), dragon=False,
                guard_reference=False, generic_conditions=()):
        fn = {'CELL': 67, 'WRLD': 310, 'LCTN': 359}[sig]
        base_tracks = (0x200, 0x201, 0x202) if dragon else (0x200, 0x201)
        base_body = header() + music(0x100, base_tracks, name='MUSCombat', priority=2)
        for key in (0x200, 0x201):
            base_body += single_track(key, base_conditions)
        if dragon:
            base_body += single_track(0x202, (condition(707, 0x35D59, run_on=2, reference=0x14),))
        base = self.write('Skyrim.esm', base_body)
        excluded = condition(fn, 0x01000900, comparison=0,
                             run_on=2 if guard_reference else 0,
                             reference=0x14 if guard_reference else 0)
        if replacement_conditions is None:
            replacement_conditions = (condition(fn, 0x01000900),)
        land_body = header(('Skyrim.esm',)) + area(sig, 0x01000900)
        land_body += music(0x100, (*base_tracks, 0x01000800), name='MUSCombat', priority=2)
        land_body += single_track(0x200, base_conditions + (excluded,))
        land_body += single_track(0x201, base_conditions + ((excluded,) if all_excluded else ()))
        land_body += single_track(0x01000800, replacement_conditions)
        land = self.write('Land.esp', land_body + land_extra)
        generic = self.write('Pack.esp', header(('Skyrim.esm',))
                             + music(0x100, (*base_tracks, 0x01000800), name='MUSCombat', priority=2)
                             + single_track(0x01000800, generic_conditions) + generic_extra)
        return base, land, generic

    def guards(self, *paths):
        return [line for line in self.scan(*paths).splitlines() if line.startswith('CONDITION_GUARD ')]

    def test_proven_region_excludes_new_generic_music(self):
        self.assertEqual(self.guards(*self.fixture()), ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304'])

    def test_cell_and_location_policies(self):
        for sig, kind in [('CELL', 0), ('LCTN', 2)]:
            self.assertEqual(self.guards(*self.fixture(sig=sig)), [f'CONDITION_GUARD pack.esp:2048 <- {kind}:land.esp:2304'])

    def test_all_input_orders_preserve_the_policy(self):
        for paths in itertools.permutations(self.fixture()):
            self.assertEqual(len(self.guards(*paths)), 1)

    def test_dragon_tracks_do_not_disable_ordinary_policy(self):
        normal = (condition(707, 0x35D59, comparison=0, run_on=2, reference=0x14),)
        self.assertEqual(len(self.guards(*self.fixture(base_conditions=normal, dragon=True))), 1)

    def test_partial_original_exclusion_defines_takeover_default(self):
        self.assertEqual(self.guards(*self.fixture(all_excluded=False)),
                         ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304',
                          'CONDITION_GUARD skyrim.esm:513 <- 1:land.esp:2304'])

    def test_quest_gated_replacement_cannot_guarantee_coverage(self):
        terms = (condition(), condition(58, 0x01000A00, comparison=40))
        self.assertEqual(self.guards(*self.fixture(replacement_conditions=terms)), [])

    def test_authored_world_exclusion_is_not_replaced_by_narrower_cue_gates(self):
        terms = (condition(), condition(67, 0x01000901))
        self.assertEqual(self.guards(*self.fixture(replacement_conditions=terms,
                         land_extra=area('CELL', 0x01000901))), ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304'])

    def test_read_only_cue_or_does_not_prevent_inheriting_authored_exclusion(self):
        self.assertEqual(self.guards(*self.fixture(replacement_conditions=(condition(flags=1),))),
                         ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304'])

    def test_shared_boss_or_scripted_generic_track_is_unchanged(self):
        for extra in (music(0x01000101, (0x01000800,), name='Boss'),
                      script_reference(0x01000A00, 0x01000800)):
            self.assertEqual(self.guards(*self.fixture(generic_extra=extra)), [])

    def test_scripted_replacement_does_not_prove_region_coverage(self):
        self.assertEqual(self.guards(*self.fixture(land_extra=script_reference(0x01000A00, 0x01000800))), [])

    def test_quest_baseline_is_not_reinterpreted_as_ordinary_music(self):
        self.assertEqual(self.guards(*self.fixture(base_conditions=(condition(58, 0xA00),))), [])

    def test_explicit_reference_is_not_equated_with_self(self):
        self.assertEqual(self.guards(*self.fixture(guard_reference=True)), [])

    def test_conditional_generic_track_stays_author_owned(self):
        self.assertEqual(self.guards(*self.fixture(generic_conditions=(condition(58, 0x01000A00),))), [])

    def test_changed_generic_audio_is_not_guarded(self):
        paths = self.fixture()
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Pack.esp'))
                           + single_track(0x01000800, path='Data\\Music\\Other.wav'))
        self.assertEqual(self.guards(*paths, patch), [])

    def test_changed_replacement_audio_is_not_policy_evidence(self):
        paths = self.fixture()
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp'))
                           + single_track(0x01000800, (condition(),), path='Data\\Music\\Other.wav'))
        self.assertEqual(self.guards(*paths, patch), [])

    def test_deliberate_policy_removal_is_respected(self):
        paths = self.fixture()
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp')) + single_track(0x200))
        # The explicitly restored original is exempt; a second original still
        # supplies independent evidence for the scene default on new songs.
        self.assertEqual(self.guards(*paths, patch), ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304'])

    def test_padding_does_not_change_positive_gate_identity(self):
        paths = self.fixture()
        padded = bytearray(condition())
        padded[1:4] = b'xyz'
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp')) + single_track(0x01000800, (bytes(padded),)))
        self.assertEqual(len(self.guards(*paths, patch)), 1)

    def test_malformed_positive_reference_or_tail_is_rejected(self):
        for offset, value in ((24, 0x14), (28, 0)):
            bad = bytearray(condition())
            struct.pack_into('<I', bad, offset, value)
            self.assertEqual(self.guards(*self.fixture(replacement_conditions=(bytes(bad),))), [])

    def test_two_independent_regions_combine_exclusions_and_generic_guards(self):
        base, land, generic = self.fixture()
        second = self.write('Second.esp', land.read_bytes())
        output = self.scan(base, land, second, generic)
        self.assertEqual([s for s in output.splitlines() if s.startswith('CONDITION_GUARD ')],
                         ['CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304,1:second.esp:2304'])
        self.assertEqual(sum(s.startswith('CONDITION_UNION ') for s in output.splitlines()), 2)

    def test_existing_dependent_union_patch_is_preserved(self):
        base, land, generic = self.fixture()
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp', 'Pack.esp'))
                           + music(0x100, (0x200, 0x201, 0x01000800, 0x02000800), name='MUSCombat', priority=2))
        self.assertEqual(len(self.guards(base, land, generic, patch)), 1)

    def test_original_shared_policy_is_read_without_editing_shared_track(self):
        base, land, generic = self.fixture()
        base.write_bytes(base.read_bytes() + music(0x101, (0x200,), name='MUSSpecialBattle'))
        output = self.scan(base, land, generic)
        self.assertIn('CONDITION_GUARD pack.esp:2048', output)
        self.assertNotIn('CONDITION_UNION skyrim.esm:512', output)

    def test_shared_original_with_two_policies_is_local_to_ordinary_owner(self):
        base, land, generic = self.fixture()
        base.write_bytes(base.read_bytes() + music(0x101, (0x200,), name='MUSSpecialBattle'))
        second = self.write('Second.esp', land.read_bytes())
        output = self.scan(base, land, second, generic)
        self.assertIn('CONDITION_GUARD pack.esp:2048 <- 1:land.esp:2304,1:second.esp:2304', output)
        self.assertIn('ISOLATE_GUARD skyrim.esm:512 IN skyrim.esm:256', output)
        self.assertNotIn('CONDITION_UNION skyrim.esm:512', output)


if __name__ == '__main__':
    unittest.main()
