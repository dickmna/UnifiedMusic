"""Synthetic acceptance tests for conservative geographic-exclusion recovery.

The native inspector shares its planner with the DLL. These tests do not invoke
Skyrim, mutate native condition chains, or prove audio playback behavior.
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

from test_auto_combat_scan import area, condition, single_track
from test_plugin_scan import group, header, music, record, script_reference, sub


def ordinary_condition() -> bytes:
    return condition(707, 0x700, comparison=0, run_on=2, reference=0x14)


def excluded(parameter: int = 0x01000900, *, function: int = 310,
             flags: int = 0, comparison: float = 0, **kwargs) -> bytes:
    return condition(function, parameter, flags=flags, comparison=comparison, **kwargs)


def reindex_base(value: bytes, index: int) -> bytes:
    """Fixture baseline fields are Skyrim-owned forms; remap their master slot."""
    result = bytearray(value)
    for offset in (12, 24):
        form = struct.unpack_from('<I', result, offset)[0]
        if form:
            struct.pack_into('<I', result, offset, (index << 24) | (form & 0xFFFFFF))
    return bytes(result)


class ConditionRepairTests(unittest.TestCase):
    TRACK = 'skyrim.esm:512'
    A_AREA = '1:a.esp:2304'
    B_AREA = '1:b.esp:2304'

    @classmethod
    def setUpClass(cls):
        candidate = os.environ.get('UNIFIED_MUSIC_INSPECT')
        if not candidate:
            raise RuntimeError('Pass --inspect PATH or set UNIFIED_MUSIC_INSPECT')
        cls.exe = Path(candidate).resolve()
        if not cls.exe.is_file():
            raise RuntimeError(f'Missing scanner: {cls.exe}')

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='UnifiedMusic-condition-fixtures-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.baseline = (ordinary_condition(),)

    def write(self, name: str, payload: bytes) -> Path:
        path = self.root / name
        path.write_bytes(payload)
        return path

    def base(self, *, baseline: tuple[bytes, ...] | None = None,
             tracks: tuple[int, ...] = (0x200,), extra: bytes = b'') -> Path:
        if baseline is not None:
            self.baseline = baseline
        payload = header() + group(music(0x100, tracks, name='MUSCombat', priority=2))
        payload += group(single_track(0x200, self.baseline), b'MUST')
        payload += record('KYWD', 0x700, sub('EDID', b'ActorTypeFixture\0'))
        return self.write('Skyrim.esm', payload + extra)

    def addon(self, name: str, *, masters: tuple[str, ...] = ('Skyrim.esm',),
              baseline: tuple[bytes, ...] | None = None,
              additions: tuple[bytes, ...] | None = None,
              signature: str = 'WRLD', include_area: bool = True,
              area_flags: int = 0, track_flags: int = 0,
              kind: int = 0x6ED7E048,
              path: str = 'Data\\Music\\Fixture\\battle.wav',
              compressed: bool = False, extra: bytes = b'') -> Path:
        skyrim_index = masters.index('Skyrim.esm')
        if baseline is None:
            baseline = tuple(reindex_base(c, skyrim_index) for c in self.baseline)
        own_area = (len(masters) << 24) | 0x900
        if additions is None:
            function = {'CELL': 67, 'WRLD': 310, 'LCTN': 359}.get(signature, 310)
            additions = (excluded(own_area, function=function),)
        payload = header(masters) + group(single_track(
            (skyrim_index << 24) | 0x200, baseline + additions,
            flags=track_flags, kind=kind, path=path, compressed=compressed), b'MUST')
        if include_area:
            payload += group(area(signature, own_area, flags=area_flags), signature.encode('ascii'))
        return self.write(name, payload + extra)

    def scan(self, *plugins: Path, allow_failure: bool = False):
        result = subprocess.run([str(self.exe), *(str(p) for p in plugins)],
                                capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=20)
        if not allow_failure:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        repairs = {}
        for line in result.stdout.splitlines():
            if line.startswith('CONDITION_UNION '):
                track, scopes = line[len('CONDITION_UNION '):].split(' <- ', 1)
                repairs[track] = tuple(scopes.split(','))
        return repairs, result

    def assert_union(self, *plugins: Path, expected=None):
        repairs, result = self.scan(*plugins)
        self.assertEqual(set(repairs), {self.TRACK}, result.stdout + result.stderr)
        self.assertCountEqual(repairs[self.TRACK], expected or (self.A_AREA, self.B_AREA))

    def assert_no_repair(self, *plugins: Path, allow_failure: bool = False):
        repairs, result = self.scan(*plugins, allow_failure=allow_failure)
        self.assertEqual(repairs, {}, result.stdout + result.stderr)

    def test_independent_negative_additions_form_union(self):
        self.assert_union(self.base(), self.addon('A.esp'), self.addon('B.esp'))

    def test_scan_order_does_not_change_proven_union(self):
        paths = (self.base(), self.addon('A.esp'), self.addon('B.esp'))
        for order in itertools.permutations(paths):
            with self.subTest(order=[p.name for p in order]):
                self.assert_union(*order)

    def test_cell_worldspace_location_exclusions_are_supported(self):
        self.assert_union(self.base(), self.addon('A.esp', signature='CELL'),
                          self.addon('B.esp'), self.addon('C.esp', signature='LCTN'),
                          expected=('0:a.esp:2304', self.B_AREA, '2:c.esp:2304'))

    def test_explicit_player_reference_exclusion_is_supported(self):
        self.assert_union(self.base(), self.addon('A.esp', additions=(excluded(run_on=2, reference=0x14),)),
                          self.addon('B.esp'))

    def test_single_extension_and_identical_extensions_need_no_repair(self):
        base, a = self.base(), self.addon('A.esp')
        self.assert_no_repair(base)
        self.assert_no_repair(base, a)
        patch = self.addon('Patch.esp', masters=('Skyrim.esm', 'A.esp'),
                           additions=(excluded(0x01000900),), include_area=False)
        self.assert_no_repair(base, a, patch)

    def test_equivalent_negative_operators_deduplicate(self):
        base, a, b = self.base(), self.addon('A.esp'), self.addon('B.esp')
        patch = self.addon('Patch.esp', masters=('Skyrim.esm', 'A.esp'),
                           additions=(excluded(0x01000900, flags=0x20, comparison=1),), include_area=False)
        self.assert_union(base, a, b, patch)

    def test_existing_union_patch_preserves_all_author_contributions(self):
        base, a, b = self.base(), self.addon('A.esp'), self.addon('B.esp')
        patch = self.addon('Patch.esp', masters=('Skyrim.esm', 'A.esp', 'B.esp'),
                           additions=(excluded(0x01000900), excluded(0x02000900)), include_area=False)
        self.assert_union(base, a, b, patch)

    def test_condition_padding_is_not_semantic(self):
        base, b = self.base(), self.addon('B.esp')
        padded = bytearray(ordinary_condition())
        padded[1:4] = b'\x5c\xb5\x18'
        self.assert_union(base, self.addon('A.esp', baseline=(bytes(padded),)), b)

    def test_master_reindexing_preserves_baseline_and_area_identity(self):
        base, b = self.base(), self.addon('B.esp')
        auxiliary = self.write('Aux.esm', header())
        a = self.addon('A.esp', masters=('Aux.esm', 'Skyrim.esm'))
        self.assert_union(base, auxiliary, a, b)

    def test_same_raw_parameter_with_different_master_identity_is_rejected(self):
        base, b = self.base(), self.addon('B.esp')
        auxiliary = self.write('Aux.esm', header())
        wrong = condition(707, 0x700, comparison=0, run_on=2, reference=0x01000014)
        a = self.addon('A.esp', masters=('Aux.esm', 'Skyrim.esm'), baseline=(wrong,))
        self.assert_no_repair(base, auxiliary, a, b)

    def test_base_condition_deletion_or_change_is_rejected(self):
        for baseline in [(), (condition(707, 0x700, comparison=1, run_on=2, reference=0x14),),
                         (condition(707, 0x701, comparison=0, run_on=2, reference=0x14),)]:
            with self.subTest(baseline=baseline):
                self.assert_no_repair(self.base(), self.addon('A.esp', baseline=baseline), self.addon('B.esp'))

    def test_original_condition_order_cannot_change(self):
        first = ordinary_condition()
        second = condition(74, 0x701, comparison=1)
        base = self.base(baseline=(first, second))
        self.assert_no_repair(base, self.addon('A.esp', baseline=(second, first)), self.addon('B.esp'))

    def test_or_in_baseline_or_added_conditions_is_rejected(self):
        original_or = bytearray(ordinary_condition())
        original_or[0] |= 1
        base = self.base(baseline=(bytes(original_or),))
        self.assert_no_repair(base, self.addon('A.esp'), self.addon('B.esp'))
        base = self.base(baseline=(ordinary_condition(),))
        self.assert_no_repair(base, self.addon('A.esp', additions=(excluded(flags=1),)), self.addon('B.esp'))

    def test_unsupported_baseline_function_is_rejected(self):
        base = self.base(baseline=(condition(77, 0, comparison=50),))
        self.assert_no_repair(base, self.addon('A.esp'), self.addon('B.esp'))

    def test_positive_or_quest_only_additions_are_not_exclusions(self):
        for added in [excluded(comparison=1), excluded(flags=0x20, comparison=0),
                      excluded(function=58, comparison=40), excluded(function=77),
                      excluded(function=117)]:
            with self.subTest(added=added):
                self.assert_no_repair(self.base(), self.addon('A.esp', additions=(added,)), self.addon('B.esp'))

    def test_alias_global_package_swap_and_unknown_flags_are_rejected(self):
        for flags in [2, 4, 8, 16, 0xC0, 0xE0]:
            with self.subTest(flags=flags):
                self.assert_no_repair(self.base(), self.addon('A.esp', additions=(excluded(flags=flags),)),
                                      self.addon('B.esp'))

    def test_invalid_run_on_reference_and_parameter_fields_are_rejected(self):
        cases = [excluded(run_on=1), excluded(run_on=2, reference=0x15),
                 excluded(reference=0x14), excluded(parameter2=1), excluded(0x09000900)]
        for added in cases:
            with self.subTest(added=added):
                self.assert_no_repair(self.base(), self.addon('A.esp', additions=(added,)), self.addon('B.esp'))

    def test_malformed_nonfinite_or_unknown_tail_conditions_are_rejected(self):
        wrong_tail = bytearray(excluded())
        struct.pack_into('<I', wrong_tail, 28, 0)
        cases = [excluded()[:-1], excluded(comparison=float('nan')),
                 excluded(comparison=float('inf')), bytes(wrong_tail)]
        for added in cases:
            with self.subTest(added=added):
                self.assert_no_repair(self.base(), self.addon('A.esp', additions=(added,)),
                                      self.addon('B.esp'), allow_failure=True)

    def test_missing_deleted_or_wrong_kind_area_is_rejected(self):
        for arguments in [dict(include_area=False), dict(area_flags=0x20),
                          dict(signature='CELL', additions=(excluded(function=310),))]:
            with self.subTest(arguments=arguments):
                self.assert_no_repair(self.base(), self.addon('A.esp', **arguments), self.addon('B.esp'))

    def test_audio_or_track_metadata_changes_are_rejected(self):
        for arguments in [dict(path='Data\\Music\\Fixture\\replacement.wav'),
                          dict(kind=0xA1A9C4D5), dict(track_flags=0x40)]:
            with self.subTest(arguments=arguments):
                self.assert_no_repair(self.base(), self.addon('A.esp', **arguments), self.addon('B.esp'))

    def test_compression_is_not_a_semantic_track_change(self):
        self.assert_union(self.base(), self.addon('A.esp', compressed=True), self.addon('B.esp'))

    def test_deleted_and_ignored_track_overrides_do_not_get_repaired(self):
        for flags in [0x20, 0x1000, 0x4000]:
            with self.subTest(flags=flags):
                self.assert_no_repair(self.base(), self.addon('A.esp', track_flags=flags), self.addon('B.esp'))

    def test_dependent_patch_removing_prior_exclusion_is_preserved(self):
        base, a, b = self.base(), self.addon('A.esp'), self.addon('B.esp')
        patch = self.addon('Patch.esp', masters=('Skyrim.esm', 'A.esp', 'B.esp'),
                           additions=(excluded(0x02000900),), include_area=False)
        self.assert_no_repair(base, a, b, patch)

    def test_transitive_dependency_removal_is_preserved_in_all_orders(self):
        base, a, b = self.base(), self.addon('A.esp'), self.addon('B.esp')
        bridge = self.write('Bridge.esp', header(('Skyrim.esm', 'A.esp')))
        patch = self.addon('Patch.esp', masters=('Skyrim.esm', 'Bridge.esp', 'B.esp'),
                           additions=(excluded(0x02000900),), include_area=False)
        for order in [(base, a, b, bridge, patch), (patch, bridge, b, a, base)]:
            with self.subTest(order=[p.name for p in order]):
                self.assert_no_repair(*order)

    def test_transitive_playlist_removal_blocks_condition_and_track_recovery(self):
        base = self.base()
        a = self.addon('A.esp', extra=(
            group(music(0x100, (0x200, 0x01000800), name='MUSCombat', priority=2))
            + group(single_track(0x01000800), b'MUST')))
        b = self.addon('B.esp')
        bridge = self.write('Bridge.esp', header(('Skyrim.esm', 'A.esp')))
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Bridge.esp'))
                           + group(music(0x100, (0x200,), name='MUSCombat', priority=2)))
        for order in [(base, a, b, bridge, patch), (patch, bridge, b, a, base)]:
            with self.subTest(order=[p.name for p in order]):
                repairs, result = self.scan(*order)
                self.assertEqual(repairs, {}, result.stdout)
                self.assertFalse(any(line.startswith('AUTO_COMBAT ') for line in result.stdout.splitlines()),
                                 result.stdout)

    def test_direct_script_track_reference_blocks_repair(self):
        self.assert_no_repair(self.base(), self.addon('A.esp', extra=script_reference(0x01000800, 0x200)),
                              self.addon('B.esp'))

    def test_custom_or_original_boss_type_sharing_blocks_repair(self):
        for custom in (True, False):
            with self.subTest(custom=custom):
                boss = music(0x01000800 if custom else 0x101, (0x200,), name='MUSCombatBoss', priority=2)
                base = self.base(extra=b'' if custom else group(boss))
                self.assert_no_repair(base, self.addon('A.esp', extra=group(boss) if custom else b''),
                                      self.addon('B.esp'))

    def test_palette_membership_blocks_repair(self):
        palette = record('MUST', 0x01000800,
                         sub('CNAM', struct.pack('<I', 0x23F678C3)) + sub('SNAM', struct.pack('<I', 0x200)))
        self.assert_no_repair(self.base(), self.addon('A.esp', extra=group(palette, b'MUST')), self.addon('B.esp'))

    def test_removed_or_one_shot_default_battle_list_is_preserved(self):
        for body in [music(0x100, (0x200,), name='MUSCombat', music_flags=5),
                     music(0x100, (), name='MUSCombat', priority=2)]:
            with self.subTest(body=body):
                self.assert_no_repair(self.base(), self.addon('A.esp', extra=group(body)), self.addon('B.esp'))

    def test_track_outside_original_default_list_is_not_repaired(self):
        self.assert_no_repair(self.base(tracks=(0x201,)), self.addon('A.esp'), self.addon('B.esp'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--inspect')
    options, remaining = parser.parse_known_args()
    if options.inspect:
        os.environ['UNIFIED_MUSIC_INSPECT'] = options.inspect
    unittest.main(argv=[__file__, *remaining])
