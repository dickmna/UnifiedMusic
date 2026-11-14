"""Additive MTD configurations and copied-track guards; no downloaded assets."""
import itertools
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_plugin_scan import header, music, record, script_reference, sub, track
from test_auto_combat_scan import area, condition, single_track


class DistributionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = Path(os.environ['UNIFIED_MUSIC_INSPECT']).resolve()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='Harmonia-config-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def write(self, name, data):
        path = self.root / name
        path.write_bytes(data if isinstance(data, bytes) else data.encode('utf-8'))
        return path

    def scan(self, *paths, configs=(), config_dir=None):
        args = [str(self.exe), *map(str, paths)]
        for path in configs:
            args += ['--config', str(path)]
        if config_dir:
            args += ['--config-dir', str(config_dir)]
        result = subprocess.run(args, capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout

    def simple(self, *, extra=b'', pack_extra=b'', name='MUSExploreForest', flags=0):
        base = self.write('Skyrim.esm', header() + music(0x100, (0x200,), name=name)
                          + single_track(0x200) + extra)
        pack = self.write('Pack.esl', header(('Skyrim.esm',)) + single_track(0x01000800, flags=flags) + pack_extra)
        config = self.write('Pack_MUS.ini', '[General]\n' + name + ' = 0x800~Pack.esl\n')
        return base, pack, config

    def regional(self, *, battle=True, role=None, copy_role=None, replacement=None,
                 pack_extra=b'', base_extra=b'', land_extra=b'', all_excluded=True):
        name = 'MUSCombat' if battle else 'MUSExploreForest'
        if role is None:
            role = (condition(707, 0x35D59, comparison=0, run_on=2, reference=0x14),) if battle else ()
        if copy_role is None:
            copy_role = role
        base = self.write('Skyrim.esm', header() + music(0x100, (0x200, 0x201), name=name)
                          + single_track(0x200, role) + single_track(0x201, role) + base_extra)
        exclusion = (condition(comparison=0),)
        land = self.write('Land.esp', header(('Skyrim.esm',)) + area('WRLD', 0x01000900)
                          + music(0x100, (0x200, 0x201, 0x01000800), name=name)
                          + single_track(0x200, role + exclusion)
                          + single_track(0x201, role + (exclusion if all_excluded else ()))
                          + single_track(0x01000800, (condition(),) if replacement is None else replacement)
                          + land_extra)
        pack = self.write('Pack.esl', header(('Skyrim.esm',)) + single_track(0x01000800, copy_role) + pack_extra)
        config = self.write('Pack_MUS.ini', '[General]\n' + name + '=0x800~Pack.esl\n')
        return base, land, pack, config

    def assert_guard(self, paths, expected=True, extra=(), configs=(), distributed=True):
        output = self.scan(*paths[:-1], *extra, configs=(paths[-1], *configs))
        self.assertEqual('CONDITION_GUARD pack.esl:2048' in output, expected, output)
        self.assertEqual('DISTRIBUTE skyrim.esm:256 <- pack.esl:2048' in output, distributed, output)
        return output

    def assert_isolated(self, paths, extra=(), configs=()):
        out = self.assert_guard(paths, False, extra=extra, configs=configs)
        self.assertIn('ISOLATE_GUARD pack.esl:2048 IN skyrim.esm:256 <- 1:land.esp:2304', out)
        return out

    def test_config_only_pack_distributes_without_music_override(self):
        base, pack, config = self.simple()
        out = self.scan(base, pack, configs=(config,))
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esl:2048', out)
        self.assertNotIn('CONDITION_GUARD ', out)

    def test_multiple_values_keys_files_are_deduplicated_and_order_independent(self):
        base, pack, first = self.simple(pack_extra=single_track(0x01000801))
        first.write_text('[General]\nMUSExploreForest=0x800~Pack.esl,0X000800~PACK.ESL\n'
                         'MUSExploreForest=801~Pack.esl\n', encoding='utf-8')
        second = self.write('Other_MUS.ini', '[General]\nMUSExploreForest=800~Pack.esl\n')
        expected = {'DISTRIBUTE skyrim.esm:256 <- pack.esl:2048', 'DISTRIBUTE skyrim.esm:256 <- pack.esl:2049'}
        for plugins in itertools.permutations((base, pack)):
            for configs in itertools.permutations((first, second)):
                self.assertEqual({s for s in self.scan(*plugins, configs=configs).splitlines() if s.startswith('DISTRIBUTE ')}, expected)

    def test_utf8_bom_whitespace_comments_and_case_insensitive_suffix(self):
        base, pack, config = self.simple()
        config.unlink()
        self.write('Case_mUs.InI', '\ufeff; comment\r\n# comment\r\n[general]\r\n MUSExploreForest = 0X800 ~ PACK.ESL \r\n')
        out = self.scan(base, pack, config_dir=self.root)
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esl:2048', out)

    def test_only_root_mus_configs_are_discovered(self):
        base, pack, config = self.simple()
        self.write('Settings.ini', '[Location]\nInvalid=ignored')
        (self.root / 'subdir').mkdir()
        self.write('subdir/Nested_MUS.ini', '[Location]\nInvalid=ignored')
        self.assertNotIn('CONFIG_HANDOFF', self.scan(base, pack, config_dir=self.root))

    def test_existing_custom_and_boss_types_accept_explicit_authored_distribution(self):
        base, pack, config = self.simple(name='MUSCombatBoss')
        out = self.scan(base, pack, configs=(config,))
        self.assertIn('DISTRIBUTE skyrim.esm:256', out)
        self.assertNotIn('CONDITION_GUARD ', out)
        custom = self.write('Custom.esp', header(('Skyrim.esm',)) + music(0x01000100, (), name='AuthorStoryMusic'))
        config.write_text('[General]\nAuthorStoryMusic=800~Pack.esl', encoding='utf-8')
        self.assertIn('DISTRIBUTE custom.esp:256', self.scan(base, pack, custom, configs=(config,)))

    def test_silent_and_palette_distributions_do_not_get_conditions(self):
        for body in (track(0x01000800, 'Silent'), record('MUST', 0x01000800,
                    sub('CNAM', struct.pack('<I', 0x23F678C3)) + sub('SNAM', struct.pack('<I', 0x200)))):
            base, pack, config = self.simple()
            pack.write_bytes(header(('Skyrim.esm',)) + body)
            out = self.scan(base, pack, configs=(config,))
            self.assertIn('DISTRIBUTE skyrim.esm:256', out)
            self.assertNotIn('CONDITION_GUARD ', out)

    def test_clear_new_type_location_region_and_unknown_section_pause_all_writes(self):
        base, land, pack, config = self.regional()
        for body in ('[General]\nMUSCombat!=800~Pack.esl', '[General]\nMissingType=800~Pack.esl',
                     '[Location]\nNewLand=MUSCombat', '[Region]\nNewLand=MUSCombat', '[Unknown]\n'):
            other = self.write('Other_MUS.ini', body)
            out = self.scan(base, land, pack, configs=(config, other))
            self.assertIn('CONFIG_HANDOFF', out)
            for action in ('DISTRIBUTE ', 'APPEND ', 'AUTO_COMBAT ', 'CONDITION_', 'RESTORE_MAPPING '):
                self.assertFalse(any(s.startswith(action) for s in out.splitlines()), out)

    def test_bad_tokens_do_not_partially_distribute_valid_prefix(self):
        base, pack, config = self.simple()
        for token in ('', '~Pack.esl', '0x~Pack.esl', '0~Pack.esl', '0xFE000800~Pack.esl', '0x1000000~Pack.esl', '-800~Pack.esl',
                      '800junk~Pack.esl', '800~../Pack.esl', '800~Pack.esl~extra',
                      '800~Pack.dll', '800~Pack.esl ; inline comment'):
            config.write_text('[General]\nMUSExploreForest=800~Pack.esl,' + token, encoding='utf-8')
            out = self.scan(base, pack, configs=(config,))
            self.assertIn('CONFIG_HANDOFF', out, token)
            self.assertNotIn('DISTRIBUTE ', out, token)

    def test_malformed_and_utf16_files_are_reported(self):
        base, pack, config = self.simple()
        for body in (b'[General\n', b'MUSExploreForest=800~Pack.esl', b'[General]\nMUSExploreForest',
                     '[General]'.encode('utf-16'), b'[General]\nMUSExploreForest=800~Pack.esl\0'):
            config.write_bytes(body)
            self.assertIn('CONFIG_HANDOFF', self.scan(base, pack, configs=(config,)))

    def test_deleted_invalid_missing_track_or_renamed_target_is_not_distributed(self):
        base, pack, config = self.simple()
        for body in (b'', single_track(0x01000800, flags=0x20), single_track(0x01000800, (b'bad',))):
            pack.write_bytes(header(('Skyrim.esm',)) + body)
            out = self.scan(base, pack, configs=(config,))
            self.assertIn('CONFIG_SKIP', out)
            self.assertNotIn('DISTRIBUTE ', out)
        base, pack, config = self.simple()
        patch = self.write('Patch.esp', header(('Skyrim.esm',)) + music(0x100, (0x200,), name='Renamed'))
        self.assertIn('CONFIG_HANDOFF', self.scan(base, pack, patch, configs=(config,)))

    def test_optional_or_stale_references_skip_only_missing_tracks(self):
        base, pack, config = self.simple()
        config.write_text('[General]\nMUSExploreForest=800~Pack.esl,801~Pack.esl,800~Optional.esl', encoding='utf-8')
        out = self.scan(base, pack, configs=(config,))
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esl:2048', out)
        self.assertEqual(out.count('CONFIG_SKIP '), 2)
        self.assertNotIn('CONFIG_HANDOFF', out)

    def test_configuration_does_not_resurrect_deliberately_removed_track(self):
        base, pack, config = self.simple()
        author = self.write('Author.esp', header(('Skyrim.esm', 'Pack.esl'))
                            + music(0x100, (0x200, 0x01000800), name='MUSExploreForest'))
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Author.esp'))
                           + music(0x100, (0x200,), name='MUSExploreForest'))
        out = self.scan(base, pack, author, patch, configs=(config,))
        self.assertIn('dependent playlist removal', out)
        self.assertNotIn('DISTRIBUTE ', out)

    def test_ambiguous_editor_id_does_not_choose_an_arbitrary_type(self):
        base, pack, config = self.simple(extra=music(0x101, (), name='MUSExploreForest'))
        self.assertIn('CONFIG_HANDOFF', self.scan(base, pack, configs=(config,)))

    def test_editor_id_case_resolves_to_the_existing_type(self):
        base, pack, config = self.simple()
        config.write_text('[General]\nmusexploreforest=800~Pack.esl', encoding='utf-8')
        out=self.scan(base,pack,configs=(config,))
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esl:2048',out)
        self.assertNotIn('CONFIG_HANDOFF',out)

    def test_case_folded_target_collision_remains_ambiguous(self):
        base,pack,config=self.simple(extra=music(0x101,(),name='musexploreforest'))
        self.assertIn('CONFIG_HANDOFF',self.scan(base,pack,configs=(config,)))

    def test_empty_comment_only_config_is_noop(self):
        base, pack, config = self.simple()
        config.write_text('; no rules\n', encoding='utf-8')
        self.assertNotIn('DISTRIBUTE ', self.scan(base, pack, configs=(config,)))

    def test_copied_ordinary_battle_predicate_gets_region_guard(self):
        self.assert_guard(self.regional())

    def test_equivalent_negative_dragon_predicate_is_preserved(self):
        self.assert_guard(self.regional(copy_role=(condition(707, 0x35D59, flags=0x20, comparison=1),)))

    def test_unconditional_config_track_gets_region_guard(self):
        self.assert_guard(self.regional(copy_role=()))

    def test_boss_positive_dragon_and_quest_conditions_are_never_guarded(self):
        for terms in ((condition(707, 0x35D59),), (condition(58, 0x200),),
                      (condition(707, 0x35D59, comparison=0, flags=1),)):
            self.assert_guard(self.regional(copy_role=terms), False)

    def test_config_boss_sharing_isolates_only_the_ordinary_owner(self):
        paths = self.regional(base_extra=music(0x101, (), name='MUSCombatBoss'))
        other = self.write('Boss_MUS.ini', '[General]\nMUSCombatBoss=800~Pack.esl')
        self.assert_isolated(paths, configs=(other,))

    def test_config_civil_war_sharing_preserves_dedicated_use(self):
        paths = self.regional(base_extra=music(0x101, (), name='MUSCombatCivilWar'))
        other = self.write('War_MUS.ini', '[General]\nMUSCombatCivilWar=800~Pack.esl')
        self.assert_isolated(paths, configs=(other,))

    def test_plugin_boss_sharing_and_direct_script_reference_block_guard(self):
        self.assert_isolated(self.regional(pack_extra=music(0x01000101, (0x01000800,), name='Boss')))
        out = self.assert_guard(self.regional(pack_extra=script_reference(0x01000A00, 0x01000800)), False, distributed=False)
        self.assertNotIn('ISOLATE_GUARD ', out)

    def test_palette_child_is_not_globally_restricted(self):
        palette = record('MUST', 0x01000801, sub('CNAM', struct.pack('<I', 0x23F678C3))
                         + sub('SNAM', struct.pack('<I', 0x01000800)))
        self.assert_isolated(self.regional(pack_extra=palette))

    def test_partial_policy_is_used_but_unresolved_quest_pauses_distribution(self):
        self.assert_guard(self.regional(all_excluded=False))
        out = self.assert_guard(self.regional(replacement=(condition(), condition(58, 0x200))), False, distributed=False)
        self.assertIn('SCENE_UNSUPPORTED ', out)

    def test_missing_replacement_keeps_distribution_without_silencing_copy(self):
        paths = self.regional(replacement=())
        self.assert_guard(paths, False)

    def test_changed_copied_audio_or_conditions_blocks_guard(self):
        for changed in (single_track(0x01000800, path='Data\\Music\\Other.wav'),
                        single_track(0x01000800, (condition(58, 0x200),))):
            paths = self.regional()
            patch = self.write('Patch.esp', header(('Skyrim.esm', 'Pack.esl')) + changed)
            self.assert_guard(paths, False, extra=(patch,), distributed=False)

    def test_ambient_config_copy_uses_related_scene_evidence(self):
        self.assert_guard(self.regional(battle=False))

    def test_ambient_shared_copy_is_not_changed_for_another_biome(self):
        paths = self.regional(battle=False, base_extra=music(0x101, (), name='MUSExploreSnow'))
        other = self.write('Other_MUS.ini', '[General]\nMUSExploreSnow=800~Pack.esl')
        self.assert_isolated(paths, configs=(other,))

    def test_ambient_original_policies_are_restored_locally(self):
        paths = self.regional(battle=False)
        second = self.write('Second.esp', paths[1].read_bytes())
        out = self.assert_guard(paths, extra=(second,))
        self.assertEqual(sum(s.startswith('CONDITION_UNION ') for s in out.splitlines()), 2)

    def test_conditional_ambient_copy_does_not_lose_time_or_quest_semantics(self):
        self.assert_guard(self.regional(battle=False, copy_role=(condition(74, 0x200),)), False)

    def test_unknown_original_baseline_prevents_inferred_guards(self):
        self.assert_guard(self.regional(role=(condition(58, 0x200),), copy_role=()), False)

    def test_configured_positive_replacement_can_supply_coverage(self):
        paths = self.regional()
        # The land supplies exclusions and its MUST, but no TNAM addition.
        raw = paths[1].read_bytes()
        old = music(0x100, (0x200, 0x201, 0x01000800), name='MUSCombat')
        paths[1].write_bytes(raw.replace(old, music(0x100, (0x200, 0x201), name='MUSCombat')))
        other = self.write('Land_MUS.ini', '[General]\nMUSCombat=800~Land.esp')
        self.assert_guard(paths, configs=(other,))

    def test_scoped_replacement_can_keep_a_separate_configured_boss_use(self):
        paths = self.regional(base_extra=music(0x101, (), name='MUSCombatBoss'))
        other = self.write('Boss_MUS.ini', '[General]\nMUSCombatBoss=800~Land.esp')
        out = self.assert_guard(paths, configs=(other,))
        self.assertIn('DISTRIBUTE skyrim.esm:257 <- land.esp:2048', out)
        self.assertNotIn('CONDITION_GUARD land.esp:', out)
        self.assertNotIn('ISOLATE_GUARD land.esp:', out)

    def test_guard_result_is_independent_of_plugin_and_config_argument_order(self):
        base, land, pack, config = self.regional()
        for plugins in itertools.permutations((base, land, pack)):
            out = self.scan(*plugins, configs=(config,))
            self.assertIn('CONDITION_GUARD pack.esl:2048 <- 1:land.esp:2304', out)

    def test_two_shared_ambient_uses_receive_different_local_policies(self):
        base_extra = music(0x101, (0x202, 0x203), name='MUSExploreSnow') + single_track(0x202) + single_track(0x203)
        land_extra = (area('WRLD', 0x01000901) + music(0x101, (0x202, 0x203, 0x01000801), name='MUSExploreSnow')
                      + single_track(0x202, (condition(parameter=0x01000901, comparison=0),))
                      + single_track(0x203, (condition(parameter=0x01000901, comparison=0),))
                      + single_track(0x01000801, (condition(parameter=0x01000901),)))
        paths = self.regional(battle=False, base_extra=base_extra, land_extra=land_extra)
        other = self.write('Snow_MUS.ini', '[General]\nMUSExploreSnow=800~Pack.esl')
        out = self.assert_isolated(paths, configs=(other,))
        self.assertIn('ISOLATE_GUARD pack.esl:2048 IN skyrim.esm:257 <- 1:land.esp:2305', out)
        self.assertEqual(out.count('ISOLATE_GUARD pack.esl:2048'), 2)

    def test_a_b_and_land_c_inherit_policy_without_any_distribution_config(self):
        for battle in (False, True):
            base, land, pack, config = self.regional(battle=battle, copy_role=())
            name = 'MUSCombat' if battle else 'MUSExploreForest'
            a = self.write('MusicA.esp', header(('Skyrim.esm',)) + music(0x100, (0x200, 0x201, 0x01000800), name=name)
                           + single_track(0x01000800))
            b = self.write('MusicB.esp', a.read_bytes())
            out = self.scan(base, a, b, land)
            for plugin in ('musica.esp', 'musicb.esp'):
                self.assertIn(f'CONDITION_GUARD {plugin}:2048 <- 1:land.esp:2304', out)
            self.assertNotIn('CONDITION_GUARD land.esp:', out)


if __name__ == '__main__':
    unittest.main()
