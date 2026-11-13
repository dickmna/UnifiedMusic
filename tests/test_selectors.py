"""Author-provided CME rules: form resolution and fail-closed ownership."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_plugin_scan import header, music, record, sub
from test_auto_combat_scan import single_track


class SelectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = Path(os.environ['UNIFIED_MUSIC_INSPECT']).resolve()

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='Harmonia-selector-')
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.base = self.write('Skyrim.esm', header() + music(0x100, (0x200,), name='MUSCombat') + single_track(0x200))
        payload = header(('Skyrim.esm',)) + music(0x01000800, (0x01000810,), name='LandBattle') + single_track(0x01000810)
        for sig, key, name in [('WRLD', 0x900, 'LandWorld'), ('CELL', 0x901, 'LandCell'), ('LCTN', 0x902, 'LandLocation'),
                               ('KYWD', 0x903, 'LandKeyword'), ('NPC_', 0x904, 'LandBoss')]:
            payload += record(sig, 0x01000000 | key, sub('EDID', name.encode() + b'\0'))
        self.land = self.write('Land.esp', payload)
        self.rule = {'isCombatMusic': True, 'newMusic': 'LandBattle',
                     'worldspaces': {'AND': True, 'forms': ['LandWorld']}}

    def write(self, name, data):
        path = self.root / name;path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data if isinstance(data, bytes) else data.encode())
        return path

    def scan(self, rules=None, *, raw=None, extra=(), directory=False):
        path = self.write('selectors/Rule.json', raw if raw is not None else json.dumps({'combatMusic': [self.rule] if rules is None else rules}))
        args = [str(self.exe), str(self.base), str(self.land), *map(str, extra),
                '--selector-dir' if directory else '--selector', str(path.parent if directory else path)]
        p = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', timeout=30)
        self.assertEqual(p.returncode, 0, p.stderr)
        return p.stdout

    def test_worldspace_rule_resolves_to_loaded_forms(self):
        self.assertIn('SELECTOR battle -> land.esp:2048 0 AND[land.esp:2304,]', self.scan())

    def test_formatted_form_ids_do_not_require_editor_id_framework(self):
        self.rule['newMusic'] = 'LAND.ESP|0X800'
        self.rule['worldspaces']['forms'] = ['Land.esp|0x900']
        self.assertIn('SELECTOR battle -> land.esp:2048', self.scan())

    def test_all_six_filter_types_resolve(self):
        for kind, name in [('cells', 'LandCell'), ('locations', 'LandLocation'), ('locationKeywords', 'LandKeyword'),
                           ('combatTarget', 'LandBoss'), ('combatTargetKeywords', 'LandKeyword')]:
            self.rule[kind] = {'AND': False, 'forms': [name]}
        out = self.scan()
        for kind in range(6):
            self.assertIn(f' {kind} ' + ('AND' if kind == 0 else 'OR'), out)

    def test_clear_location_rule_is_a_separate_channel(self):
        self.rule['isCombatMusic'] = False
        self.assertIn('SELECTOR cleared -> land.esp:2048', self.scan())

    def test_directory_only_reads_json_files(self):
        self.write('selectors/ignore.txt', 'bad')
        self.assertIn('SELECTOR battle', self.scan(directory=True))

    def test_bad_json_unknown_fields_duplicate_keys_or_trailing_content_pause(self):
        for raw in ('{', '{"combatMusic":[],"combatMusic":[]}', '{"combatMusic":[]} junk',
                    '{"combatMusic":[],"typo":true}', json.dumps({'combatMusic': [dict(self.rule, musicType='Typo')]})):
            self.assertIn('CONFIG_HANDOFF', self.scan(raw=raw))

    def test_unresolved_or_wrong_type_filter_does_not_broaden_rule(self):
        for name in ('MissingWorld', 'LandBoss', 'Land.esp|0xFE000900', 'Land.esp|0x900garbage', '../Land.esp|0x900'):
            self.rule['worldspaces']['forms'] = [name]
            out = self.scan()
            self.assertIn('CONFIG_HANDOFF', out)
            self.assertNotIn('SELECTOR ', out)

    def test_missing_target_does_not_keep_other_partial_rules(self):
        bad = dict(self.rule, newMusic='MissingMusic')
        out = self.scan([self.rule, bad])
        self.assertIn('CONFIG_HANDOFF', out)
        self.assertNotIn('SELECTOR ', out)

    def test_ambiguous_editor_id_does_not_pick_arbitrary_mod(self):
        duplicate = self.write('Other.esp', header(('Skyrim.esm',)) + record('WRLD', 0x01000900, sub('EDID', b'LandWorld\0')))
        self.assertIn('CONFIG_HANDOFF', self.scan(extra=[duplicate]))

    def test_deleted_or_renamed_filter_requires_author_update(self):
        for flags, name in [(0x20, 'LandWorld'), (0, 'ChangedWorld')]:
            patch = self.write('Patch.esp', header(('Skyrim.esm', 'Land.esp')) +
                               record('WRLD', 0x01000900, sub('EDID', name.encode() + b'\0'), flags=flags))
            self.assertIn('CONFIG_HANDOFF', self.scan(extra=[patch]))

    def test_empty_filters_and_missing_required_fields_are_not_global_rules(self):
        for rule in ({'isCombatMusic': True, 'newMusic': 'LandBattle'},
                     dict(self.rule, worldspaces={'AND': True, 'forms': []}),
                     dict(self.rule, worldspaces={'forms': ['LandWorld']}),
                     dict(self.rule, isCombatMusic='true')):
            self.assertIn('CONFIG_HANDOFF', self.scan([rule]))

    def test_explicit_selector_target_is_not_automatically_merged(self):
        self.base.write_bytes(header() + music(0x100, (0x200,), name='MUSExploreForest') + single_track(0x200))
        addition = self.write('Pack.esp', header(('Skyrim.esm',)) + music(0x100, (0x200, 0x01000800)) + single_track(0x01000800))
        self.rule['newMusic'] = 'MUSExploreForest'
        out = self.scan(extra=[addition])
        self.assertIn('SELECTOR battle -> skyrim.esm:256', out)
        self.assertNotIn('APPEND ', out)

    def test_invalid_selector_pauses_playlist_restoration_too(self):
        addition = self.write('Pack.esp', header(('Skyrim.esm',)) + music(0x100, (0x200, 0x01000800), name='MUSCombat') + single_track(0x01000800))
        self.rule['newMusic'] = 'Missing'
        out = self.scan(extra=[addition])
        for prefix in ('AUTO_COMBAT ', 'APPEND ', 'DISTRIBUTE ', 'CONDITION_', 'SELECTOR '):
            self.assertNotIn(prefix, out)


if __name__ == '__main__':
    unittest.main()
