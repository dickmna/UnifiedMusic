"""Automatic combat inference acceptance tests; synthetic ESPs only.

Run with --inspect PATH or UNIFIED_MUSIC_INSPECT. These tests exercise static
eligibility and fail-closed handling, not Skyrim playback or native conditions.
"""
from __future__ import annotations

import argparse
import itertools
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_plugin_scan import group, header, music, record, script_reference, sub


def condition(function: int = 310, parameter: int = 0x01000900, *,
              flags: int = 0, comparison: float = 1.0, run_on: int = 0,
              reference: int = 0, parameter2: int = 0) -> bytes:
    body = bytearray(32)
    body[0] = flags
    struct.pack_into('<fIIIIII', body, 4, comparison, function, parameter,
                     parameter2, run_on, reference, 0xFFFFFFFF)
    return bytes(body)


def single_track(form_id: int, conditions: tuple[bytes, ...] = (), *,
                 path: str = 'Data\\Music\\Fixture\\battle.wav',
                 kind: int = 0x6ED7E048, flags: int = 0,
                 compressed: bool = False) -> bytes:
    body = (sub('EDID', b'FixtureBattleTrack\0') + sub('CNAM', struct.pack('<I', kind))
            + sub('ANAM', path.encode('ascii') + b'\0')
            + sub('CITC', struct.pack('<I', len(conditions))))
    body += b''.join(sub('CTDA', c) for c in conditions)
    return record('MUST', form_id, body, flags=flags, compressed=compressed)


def area(signature: str, form_id: int, *, flags: int = 0) -> bytes:
    return record(signature, form_id, sub('EDID', b'FixtureArea\0'), flags=flags)


class AutoCombatScanTests(unittest.TestCase):
    DEFAULT = 'skyrim.esm:256'
    TRACK = 'land.esp:2048'
    REGION = 'land.esp:2304'

    @classmethod
    def setUpClass(cls):
        value = os.environ.get('UNIFIED_MUSIC_INSPECT')
        if not value:
            raise RuntimeError('Pass --inspect PATH or set UNIFIED_MUSIC_INSPECT')
        cls.exe = Path(value).resolve()
        if not cls.exe.is_file():
            raise RuntimeError(f'Missing scanner: {cls.exe}')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='UnifiedMusic-combat-fixtures-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def write(self, name: str, payload: bytes) -> Path:
        path = self.root / name
        path.write_bytes(payload)
        return path

    def base(self, *, name: str = 'MUSCombat', script_bound: bool = False) -> Path:
        payload = header() + group(music(0x100, (0x200,), name=name, priority=2))
        payload += group(single_track(0x200), b'MUST')
        if script_bound:
            payload += script_reference(0x300, 0x100)
        return self.write('Skyrim.esm', payload)

    def land(self, *, conditions: tuple[bytes, ...] | None = None,
             signature: str = 'WRLD', area_id: int = 0x01000900,
             include_area: bool = True, area_flags: int = 0,
             tracks: tuple[int, ...] = (0x200, 0x01000800),
             kind: int = 0x6ED7E048, path: str = 'Data\\Music\\Fixture\\battle.wav',
             track_flags: int = 0, compressed: bool = False,
             priority: int = 2, extra: bytes = b'') -> Path:
        if conditions is None:
            function = {'CELL': 67, 'WRLD': 310, 'LCTN': 359, 'REGN': 117}.get(signature, 310)
            conditions = (condition(function, area_id),)
        payload = header(('Skyrim.esm',))
        payload += group(music(0x100, tracks, name='MUSCombat', priority=priority))
        payload += group(single_track(0x01000800, conditions, kind=kind, path=path,
                                     flags=track_flags, compressed=compressed), b'MUST')
        if include_area:
            payload += group(area(signature, area_id, flags=area_flags), signature.encode('ascii'))
        return self.write('Land.esp', payload + extra)

    def scan(self, *paths: Path, require_success: bool = True):
        result = subprocess.run([str(self.exe), *(str(p) for p in paths)],
                                capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=20)
        if require_success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        defaults, rules = set(), []
        for line in result.stdout.splitlines():
            if line.startswith('BATTLE_DEFAULT '):
                defaults.add(line[len('BATTLE_DEFAULT '):])
            elif line.startswith('AUTO_COMBAT '):
                regions, tracks = line[len('AUTO_COMBAT '):].split(' <- ', 1)
                rules.append((tuple(regions.split(',')), tuple(tracks.split(','))))
        return defaults, rules, result

    def assert_no_auto(self, *paths: Path, require_success: bool = True):
        _, rules, result = self.scan(*paths, require_success=require_success)
        self.assertEqual(rules, [], result.stdout + result.stderr)

    def test_explicit_worldspace_addition_creates_rule(self):
        defaults, rules, _ = self.scan(self.base(), self.land())
        self.assertEqual(defaults, {self.DEFAULT})
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])

    def test_cell_and_location_generate_exact_area_kinds(self):
        for signature, index in [('CELL', 0), ('LCTN', 2)]:
            with self.subTest(signature=signature):
                _, rules, _ = self.scan(self.base(), self.land(signature=signature))
                self.assertEqual(rules, [((f'{index}:' + self.REGION,), (self.TRACK,))])

    def test_supported_positive_comparisons(self):
        for flags, value in [(0, 1.0), (0x20, 0.0)]:
            with self.subTest(flags=flags, value=value):
                _, rules, _ = self.scan(self.base(), self.land(conditions=(condition(flags=flags, comparison=value),)))
                self.assertEqual(len(rules), 1)

    def test_explicit_player_reference_is_accepted(self):
        _, rules, _ = self.scan(self.base(), self.land(conditions=(condition(run_on=2, reference=0x14),)))
        self.assertEqual(len(rules), 1)

    def test_additional_safe_quest_condition_does_not_get_discarded_as_geo(self):
        stage = condition(58, 0x01000B00, flags=0x60, comparison=40.0)
        quest = record('QUST', 0x01000B00, sub('EDID', b'FixtureQuest\0'))
        _, rules, _ = self.scan(self.base(), self.land(conditions=(condition(), stage), extra=quest))
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])

    def test_supported_stable_quest_and_global_conditions_are_retained(self):
        for function in [56, 58, 59, 74]:
            with self.subTest(function=function):
                extra = condition(function, 0x01000B00, comparison=1.0)
                signature = 'GLOB' if function == 74 else 'QUST'
                definition = record(signature, 0x01000B00, sub('EDID', b'FixtureState\0'))
                _, rules, _ = self.scan(self.base(), self.land(conditions=(condition(), extra), extra=definition))
                self.assertEqual(len(rules), 1)

    def test_multiple_geographic_and_gates_are_retained(self):
        second = condition(67, 0x01000901)
        cell = area('CELL', 0x01000901)
        _, rules, _ = self.scan(self.base(), self.land(conditions=(condition(), second), extra=cell))
        self.assertEqual(len(rules), 1)
        self.assertCountEqual(rules[0][0], ['1:' + self.REGION, '0:land.esp:2305'])

    def test_duplicate_playlist_entry_is_inferred_once(self):
        _, rules, _ = self.scan(self.base(), self.land(tracks=(0x200, 0x01000800, 0x01000800)))
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])

    def test_two_tracks_with_same_area_share_rule_or_remain_equivalent(self):
        extra = single_track(0x01000801, (condition(),))
        _, rules, _ = self.scan(self.base(), self.land(tracks=(0x200, 0x01000800, 0x01000801), extra=extra))
        self.assertEqual({r for regions, _ in rules for r in regions}, {'1:' + self.REGION})
        self.assertCountEqual([t for _, tracks in rules for t in tracks], [self.TRACK, 'land.esp:2049'])

    def test_vanilla_script_reference_keeps_known_public_battle_role(self):
        _, rules, _ = self.scan(self.base(script_bound=True), self.land())
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])

    def test_new_mod_script_reference_keeps_public_battle_role(self):
        _, rules, result = self.scan(self.base(), self.land(extra=script_reference(0x01000C00, 0x100)))
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])
        self.assertNotIn('APPEND land.esp:', result.stdout)

    def assert_public_membership_restored(self, *paths):
        _, rules, result = self.scan(*paths)
        self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])
        self.assertNotIn('APPEND ', result.stdout)
        self.assertNotIn('USE_RULES ', result.stdout)

    def test_ordinary_negative_dragon_role_is_restored_without_boss_rule(self):
        for flags, comparison in [(0, 0.0), (0x20, 1.0)]:
            for run_on, reference in [(0, 0), (2, 0x14)]:
                with self.subTest(flags=flags, run_on=run_on):
                    ordinary = condition(707, 0x35D59, flags=flags,
                                         comparison=comparison, run_on=run_on,
                                         reference=reference)
                    _, rules, _ = self.scan(self.base(), self.land(conditions=(ordinary,)))
                    self.assertEqual(rules, [(('*',), (self.TRACK,))])
        boss = condition(707, 0x35D59)
        self.assert_no_auto(self.base(), self.land(conditions=(boss,)))

    def test_source_file_enumeration_order_does_not_change_rules(self):
        base, land = self.base(), self.land()
        first = self.scan(base, land)[:2]
        second = self.scan(land, base)[:2]
        self.assertEqual(first, second)

    def test_compressed_track_record_is_supported(self):
        _, rules, _ = self.scan(self.base(), self.land(compressed=True))
        self.assertEqual(len(rules), 1)

    def test_custom_battle_type_name_alone_is_not_authoring_evidence(self):
        custom = (header(('Skyrim.esm',))
                  + music(0x01000100, (0x01000800,), name='MyLandMUSCombat', priority=2)
                  + single_track(0x01000800, (condition(),))
                  + area('WRLD', 0x01000900))
        self.assert_no_auto(self.base(), self.write('Land.esp', custom))

    def test_missing_default_origin_does_not_infer(self):
        self.assert_no_auto(self.land())

    def test_music_type_priority_is_retained_without_blocking_membership(self):
        base,land=self.base(),self.land(priority=1)
        before=land.read_bytes()
        _,rules,_=self.scan(base,land)
        self.assertEqual(rules,[(('1:'+self.REGION,),(self.TRACK,))])
        self.assertEqual(land.read_bytes(),before)

    def test_base_track_removal_blocks_inference(self):
        self.assert_no_auto(self.base(), self.land(tracks=(0x01000800,)))

    def test_explicit_dependent_patch_removal_blocks_inference_in_all_orders(self):
        base, land = self.base(), self.land()
        patch = self.write('Removal.esp', header(('Skyrim.esm', 'Land.esp'))
                           + music(0x100, (0x200,), name='MUSCombat', priority=2))
        for paths in itertools.permutations((base, land, patch)):
            with self.subTest(order=[p.name for p in paths]):
                self.assert_no_auto(*paths)

    def test_plain_battle_contributions_survive_independent_playlist_overrides(self):
        base, land = self.base(), self.land(conditions=())
        other = self.write('Other.esp', header(('Skyrim.esm',))
                           + music(0x100, (0x200, 0x01000800), name='MUSCombat', priority=2)
                           + single_track(0x01000800))
        for earlier, later in [(land, other), (other, land)]:
            with self.subTest(order=[earlier.name, later.name]):
                defaults, rules, _ = self.scan(base, earlier, later)
                self.assertEqual(defaults, {self.DEFAULT})
                self.assertEqual({scope for scopes, _ in rules for scope in scopes}, {'*'})
                self.assertCountEqual([track for _, tracks in rules for track in tracks],
                                      [self.TRACK, 'other.esp:2048'])

    def test_explicit_dependent_patch_removal_of_plain_track_is_respected(self):
        base, land = self.base(), self.land(conditions=())
        patch = self.write('Removal.esp', header(('Skyrim.esm', 'Land.esp'))
                           + music(0x100, (0x200,), name='MUSCombat', priority=2))
        self.assert_no_auto(base, land, patch)

    def test_added_track_shared_with_script_bound_type_retains_authored_ordinary_membership(self):
        extra = (music(0x01000A00, (0x01000800,), name='QuestMusic', priority=2)
                 + script_reference(0x01000B00, 0x01000A00))
        self.assert_public_membership_restored(self.base(), self.land(extra=extra))

    def test_track_shared_with_unbound_custom_type_retains_authored_ordinary_membership(self):
        extra = music(0x01000A00, (0x01000800,), name='IndependentMusic', priority=2)
        self.assert_public_membership_restored(self.base(), self.land(extra=extra))

    def test_single_track_inside_special_palette_retains_authored_ordinary_membership(self):
        palette = (sub('EDID', b'BossPalette\0') + sub('CNAM', struct.pack('<I', 0x23F678C3))
                   + sub('SNAM', struct.pack('<I', 0x01000800)))
        # The custom MusicType owns the palette, not the candidate directly.
        extra = (record('MUST', 0x01000C00, palette)
                 + music(0x01000A00, (0x01000C00,), name='BossMusic', priority=2))
        self.assert_public_membership_restored(self.base(), self.land(extra=extra))

    def test_track_shared_with_native_boss_type_retains_authored_ordinary_membership(self):
        base = self.base()
        base.write_bytes(base.read_bytes() + group(music(0x400, (0x200,), name='MUSCombatBoss', priority=2)))
        extra = music(0x400, (0x200, 0x01000800), name='MUSCombatBoss', priority=2)
        _,rules,result=self.scan(base,self.land(extra=extra))
        self.assertEqual(rules,[(('1:'+self.REGION,),(self.TRACK,))])
        self.assertIn('APPEND skyrim.esm:1024 <- land.esp:2048',result.stdout)
        self.assertNotIn('USE_RULES ',result.stdout)

    def test_added_track_shared_with_one_shot_type_retains_authored_ordinary_membership(self):
        extra = music(0x01000A00, (0x01000800,), name='SpecialMusic', priority=2)
        extra = extra.replace(sub('FNAM', struct.pack('<I', 4)), sub('FNAM', struct.pack('<I', 5)))
        self.assert_public_membership_restored(self.base(), self.land(extra=extra))

    def test_track_used_by_authored_area_music_retains_authored_ordinary_membership(self):
        for signature, field in [('CELL', 'XCMO'), ('WRLD', 'ZNAM'),
                                 ('LCTN', 'NAM1'), ('REGN', 'RDMO')]:
            with self.subTest(signature=signature):
                extra = music(0x01000A00, (0x01000800,), name='AuthoredMusic', priority=2)
                extra += record(signature, 0x01000B00, sub(field, struct.pack('<I', 0x01000A00)))
                self.assert_public_membership_restored(self.base(), self.land(extra=extra))

    def test_direct_script_reference_to_track_is_protected(self):
        self.assert_no_auto(self.base(), self.land(extra=script_reference(0x01000B00, 0x01000800)))

    def test_added_track_deleted_is_protected(self):
        self.assert_no_auto(self.base(), self.land(track_flags=0x20))

    def test_ignored_track_cannot_supply_automatic_rule(self):
        self.assert_no_auto(self.base(), self.land(track_flags=0x1000))

    def test_ignored_geographic_form_cannot_supply_automatic_rule(self):
        self.assert_no_auto(self.base(), self.land(area_flags=0x1000))

    def test_track_condition_override_change_is_protected(self):
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp'))
                           + single_track(0x01000800, (condition(comparison=0.0),)))
        self.assert_no_auto(self.base(), self.land(), patch)

    def test_non_geographic_form_parameters_survive_master_reindexing(self):
        for function in [56, 58, 59, 74]:
            with self.subTest(function=function):
                signature = 'GLOB' if function == 74 else 'QUST'
                state = record(signature, 0x01000B00, sub('EDID', b'FixtureState\0'))
                base = self.base()
                land = self.land(conditions=(condition(), condition(function, 0x01000B00)), extra=state)
                other = self.write('Other.esp', header(('Skyrim.esm',)))
                # Land is master index 2 in this override, index 1 in the origin.
                patch = self.write('Patch.esp', header(('Skyrim.esm', 'Other.esp', 'Land.esp'))
                                   + single_track(0x02000800, (condition(parameter=0x02000900),
                                                             condition(function, 0x02000B00))))
                _, rules, _ = self.scan(base, land, other, patch)
                self.assertEqual(rules, [(('1:' + self.REGION,), (self.TRACK,))])

    def test_dependent_parameter_update_supplies_the_effective_authored_conditions(self):
        for function in [56, 58, 59, 74]:
            with self.subTest(function=function):
                signature = 'GLOB' if function == 74 else 'QUST'
                state = record(signature, 0x01000B00, sub('EDID', b'OriginalState\0'))
                state += record(signature, 0x01000B01, sub('EDID', b'DifferentState\0'))
                base = self.base()
                land = self.land(conditions=(condition(), condition(function, 0x01000B00)), extra=state)
                patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp'))
                                   + single_track(0x01000800, (condition(),
                                                             condition(function, 0x01000B01))))
                before=patch.read_bytes()
                _,rules,out=self.scan(base,land,patch)
                self.assertEqual(rules,[(('1:'+self.REGION,),(self.TRACK,))])
                self.assertNotIn('USE_RULES ',out.stdout)
                self.assertEqual(patch.read_bytes(),before)

    def test_geographic_form_must_exist(self):
        self.assert_no_auto(self.base(), self.land(include_area=False))

    def test_geographic_form_type_must_match_function(self):
        self.assert_no_auto(self.base(), self.land(signature='CELL', conditions=(condition(310),)))

    def test_geographic_form_cannot_be_deleted(self):
        self.assert_no_auto(self.base(), self.land(area_flags=0x20))

    def test_geographic_form_later_deletion_blocks_rule(self):
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp'))
                           + area('WRLD', 0x01000900, flags=0x20))
        self.assert_no_auto(self.base(), self.land(), patch)

    def test_region_condition_is_unsupported_not_guessed(self):
        self.assert_no_auto(self.base(), self.land(signature='REGN'))

    def test_plain_battle_addition_is_restored_without_an_invented_area_rule(self):
        defaults, rules, _ = self.scan(self.base(), self.land(conditions=()))
        self.assertEqual(defaults, {self.DEFAULT})
        self.assertEqual(rules, [(('*',), (self.TRACK,))])

    def test_conditions_without_geographic_gate_do_not_infer(self):
        self.assert_no_auto(self.base(), self.land(conditions=(condition(58, 0x01000B00, comparison=40.0),)))

    def test_negative_or_non_boolean_geographic_comparisons_do_not_infer(self):
        for flags, value in [(0, 0.0), (0x20, 1.0), (0, 2.0), (0x40, 0.0), (0, float('nan'))]:
            with self.subTest(flags=flags, value=value):
                self.assert_no_auto(self.base(), self.land(conditions=(condition(flags=flags, comparison=value),)))

    def test_or_alias_global_package_swap_flags_do_not_infer(self):
        for flag in [1, 2, 4, 8, 16]:
            with self.subTest(flag=flag):
                self.assert_no_auto(self.base(), self.land(conditions=(condition(flags=flag),)))

    def test_unsafe_additional_condition_blocks_whole_track(self):
        for flag in [1, 2, 4, 8, 16, 0xC0, 0xE0]:
            with self.subTest(flag=flag):
                unsafe = condition(58, 0x01000B00, flags=flag, comparison=40.0)
                self.assert_no_auto(self.base(), self.land(conditions=(condition(), unsafe)))

    def test_random_and_unknown_conditions_cannot_be_resampled_by_inference(self):
        for function in [77, 999, 0xFFFFFFFF]:
            with self.subTest(function=function):
                unsafe = condition(function, 0, flags=0x40, comparison=50.0)
                self.assert_no_auto(self.base(), self.land(conditions=(condition(), unsafe)))

    def test_nonfinite_comparisons_are_rejected_even_in_extra_conditions(self):
        for value in [float('nan'), float('inf'), float('-inf')]:
            with self.subTest(value=value):
                unsafe = condition(58, 0x01000B00, comparison=value)
                self.assert_no_auto(self.base(), self.land(conditions=(condition(), unsafe)))

    def test_geographic_conditions_cannot_target_nonplayer(self):
        for run_on, reference in [(1, 0), (2, 0x15), (3, 0), (5, 0), (7, 0)]:
            with self.subTest(run_on=run_on, reference=reference):
                self.assert_no_auto(self.base(), self.land(conditions=(condition(run_on=run_on, reference=reference),)))

    def test_null_and_unresolved_geographic_references_cannot_infer(self):
        for parameter in [0, 0xFF000900]:
            with self.subTest(parameter=parameter):
                self.assert_no_auto(self.base(), self.land(conditions=(condition(parameter=parameter),)),
                                    require_success=False)

    def test_malformed_condition_length_cannot_infer(self):
        for length in [0, 4, 28, 31, 33, 36]:
            with self.subTest(length=length):
                raw = condition()[:length] if length <= 32 else condition() + b'\0'*(length-32)
                self.assert_no_auto(self.base(), self.land(conditions=(raw,)), require_success=False)

    def test_condition_count_mismatch_cannot_infer(self):
        base, land = self.base(), self.land()
        payload = land.read_bytes().replace(sub('CITC', struct.pack('<I', 1)),
                                           sub('CITC', struct.pack('<I', 2)))
        land.write_bytes(payload)
        self.assert_no_auto(base, land, require_success=False)

    def test_silent_palette_and_empty_audio_path_do_not_infer(self):
        for kind, path in [(0xA1A9C4D5, 'Data\\Music\\test.wav'), (0x23F678C3, 'Data\\Music\\test.wav'), (0x6ED7E048, '')]:
            with self.subTest(kind=kind, path=path):
                self.assert_no_auto(self.base(), self.land(kind=kind, path=path))

    def test_file_parse_failure_does_not_emit_partial_automatic_rules(self):
        malformed = self.write('Malformed.esp', header(('Skyrim.esm',)) + b'broken')
        _, rules, result = self.scan(self.base(), self.land(), malformed, require_success=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(rules, [])
        self.assertNotIn('BATTLE_DEFAULT ', result.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--inspect')
    options, remaining = parser.parse_known_args()
    if options.inspect:
        os.environ['UNIFIED_MUSIC_INSPECT'] = options.inspect
    unittest.main(argv=[__file__, *remaining])
