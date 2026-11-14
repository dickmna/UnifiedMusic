"""Explicit MTD ownership versus automatic repairs, with synthetic plugins."""
import itertools
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from test_plugin_scan import header, music, record, sub, track, script_reference
from test_auto_combat_scan import single_track, condition


def named_area(kind, fid, name, target):
    body=sub('EDID',name.encode()+b'\0')
    if kind=='LCTN':
        body+=sub('NAM1',struct.pack('<I',target))
    else:
        # Sound, weather and another sound block: assigning a REGN must not
        # reinterpret or flatten its unrelated entries into a single music list.
        body+=sub('RDAT',struct.pack('<IBB2x',7,1,80))+sub('RDMO',struct.pack('<I',target))
        body+=sub('RDAT',struct.pack('<IBB2x',3,0,42))
        body+=sub('RDAT',struct.pack('<IBB2x',7,0,30))+sub('RDMO',struct.pack('<I',target))
    return record(kind,fid,body)


class ConfigPrecedenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.exe=Path(os.environ['UNIFIED_MUSIC_INSPECT']).resolve()

    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='Harmonia-priority-')
        self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.base=self.write('Skyrim.esm',header()+music(0x100,(0x200,))+single_track(0x200)
            +music(0x101,(0x201,),name='MUSCombatBoss')+single_track(0x201)
            +named_area('LCTN',0x300,'TestInn',0x100)+named_area('REGN',0x301,'TestRegion',0x100))
        self.pack=self.write('Pack.esp',header(('Skyrim.esm',))+single_track(0x01000800)
            +single_track(0x01000801,(condition(58,0x01000900),))+record('QUST',0x01000900,b'')
            +music(0x01000100,(0x01000801,),name='AuthorStory')+track(0x01000802,'Silent'))

    def write(self,name,body):
        path=self.root/name;path.write_bytes(body if isinstance(body,bytes) else body.encode('utf-8'));return path

    def scan(self,*extra,configs=()):
        args=[str(self.exe),str(self.base),str(self.pack),*map(str,extra)]
        for path in configs: args+=['--config',str(path)]
        result=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)
        return result.stdout

    def test_location_and_region_use_exact_existing_authored_type(self):
        config=self.write('Land_MUS.ini','[Location]\ntestinn=authorstory\n[Region]\nTESTREGION=AuthorStory')
        before=[p.read_bytes() for p in (self.base,self.pack)]
        out=self.scan(configs=[config])
        self.assertIn('ASSIGN 2:skyrim.esm:768 <- pack.esp:256',out)
        self.assertIn('ASSIGN 3:skyrim.esm:769 <- pack.esp:256',out)
        self.assertNotIn('CONFIG_HANDOFF',out)
        self.assertNotIn('APPEND pack.esp:256',out)
        self.assertNotIn('CONDITION_GUARD pack.esp:',out)
        self.assertEqual(before,[p.read_bytes() for p in (self.base,self.pack)])

    def test_last_alphabetical_file_assignment_wins_independent_of_cli_order(self):
        first=self.write('A_MUS.ini','[Location]\nTestInn=AuthorStory\n[Region]\nTestRegion=AuthorStory')
        last=self.write('z_MUS.ini','[Location]\nTestInn=MUSCombatBoss\n[Region]\nTestRegion=MUSExploreForest')
        for configs in itertools.permutations((first,last)):
            out=self.scan(configs=configs)
            self.assertIn('ASSIGN 2:skyrim.esm:768 <- skyrim.esm:257',out)
            self.assertIn('ASSIGN 3:skyrim.esm:769 <- skyrim.esm:256',out)

    def test_last_repeated_area_key_wins(self):
        config=self.write('Land_MUS.ini','[Location]\nTestInn=AuthorStory\nTestInn=MUSCombatBoss')
        self.assertIn('ASSIGN 2:skyrim.esm:768 <- skyrim.esm:257',self.scan(configs=[config]))

    def test_explicit_assignment_replaces_automatic_mapping_repair(self):
        author=self.write('Author.esp',header(('Skyrim.esm','Pack.esp'))+named_area('LCTN',0x300,'TestInn',0x01000100))
        conflict=self.write('Lighting.esp',header(('Skyrim.esm',))+named_area('LCTN',0x300,'TestInn',0x100))
        self.assertIn('RESTORE_MAPPING 2:skyrim.esm:768 <- pack.esp:256',self.scan(author,conflict))
        config=self.write('Player_MUS.ini','[Location]\nTestInn=MUSCombatBoss')
        out=self.scan(author,conflict,configs=[config])
        self.assertIn('ASSIGN 2:skyrim.esm:768 <- skyrim.esm:257',out)
        self.assertNotIn('RESTORE_MAPPING 2:skyrim.esm:768',out)

    def test_wrong_kind_missing_or_ambiguous_area_cannot_assign_somewhere_else(self):
        for body in ('[Location]\nTestRegion=AuthorStory','[Region]\nTestInn=AuthorStory',
                     '[Location]\nMissing=AuthorStory','[Location]\nTestInn=Missing'):
            config=self.write('Land_MUS.ini',body)
            out=self.scan(configs=[config]);self.assertIn('CONFIG_HANDOFF',out);self.assertNotIn('ASSIGN ',out)
        duplicate=self.write('Other.esp',header(('Skyrim.esm',))+named_area('LCTN',0x01000300,'TESTINN',0x100))
        config.write_text('[Location]\nTestInn=AuthorStory',encoding='utf-8')
        self.assertIn('CONFIG_HANDOFF',self.scan(duplicate,configs=[config]))

    def test_clear_is_closed_to_restoration_and_keeps_only_configured_members(self):
        addition=self.write('Addition.esp',header(('Skyrim.esm','Pack.esp'))
            +music(0x100,(0x200,0x01000800)))
        config=self.write('OnlyStory_MUS.ini','[General]\nMUSExploreForest!=801~Pack.esp')
        out=self.scan(addition,configs=[config])
        self.assertIn('CLEAR skyrim.esm:256',out)
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esp:2049',out)
        self.assertNotIn('APPEND skyrim.esm:256',out)
        self.assertNotIn('ALLOW skyrim.esm:256',out)
        self.assertNotIn('USE_RULES ',out)
        self.assertNotIn('CLEAR skyrim.esm:257',out)

    def test_empty_clear_and_silence_track_are_both_explicit_instructions(self):
        for value,distributed in (('',False),('802~Pack.esp',True),('800~Absent.esp',False)):
            config=self.write('Silence_MUS.ini','[General]\nMUSExploreForest! = '+value)
            out=self.scan(configs=[config])
            self.assertIn('CLEAR skyrim.esm:256',out)
            self.assertEqual('DISTRIBUTE skyrim.esm:256' in out,distributed)
            self.assertNotIn('CONFIG_HANDOFF',out)

    def test_clear_and_add_rules_combine_configured_members_deterministically(self):
        clear=self.write('Clear_MUS.ini','[General]\nMUSExploreForest!=801~Pack.esp')
        add=self.write('Add_MUS.ini','[General]\nMUSExploreForest=800~Pack.esp,801~Pack.esp')
        for configs in itertools.permutations((clear,add)):
            out=self.scan(configs=configs)
            self.assertEqual(out.count('CLEAR '),1)
            self.assertEqual({r for r in out.splitlines() if r.startswith('DISTRIBUTE ')},
                {'DISTRIBUTE skyrim.esm:256 <- pack.esp:2048','DISTRIBUTE skyrim.esm:256 <- pack.esp:2049'})

    def test_clear_default_battle_does_not_suppress_independent_authored_boss_link(self):
        self.base.write_bytes(header()+music(0x100,(0x200,),name='MUSCombat')+single_track(0x200)
            +music(0x101,(0x201,),name='MUSCombatBoss')+single_track(0x201))
        addition=self.write('Addition.esp',header(('Skyrim.esm','Pack.esp'))
            +music(0x100,(0x200,0x01000800),name='MUSCombat')+music(0x101,(0x201,0x01000800),name='MUSCombatBoss'))
        config=self.write('Quiet_MUS.ini','[General]\nMUSCombat! =')
        out=self.scan(addition,configs=[config])
        self.assertIn('CLEAR skyrim.esm:256',out)
        self.assertNotIn('AUTO_COMBAT ',out)
        self.assertIn('APPEND skyrim.esm:257 <- pack.esp:2048',out)

    def test_clear_boss_library_is_explicit_and_does_not_change_its_story_track(self):
        config=self.write('Boss_MUS.ini','[General]\nMUSCombatBoss!=801~Pack.esp')
        out=self.scan(configs=[config])
        self.assertIn('CLEAR skyrim.esm:257',out)
        self.assertIn('DISTRIBUTE skyrim.esm:257 <- pack.esp:2049',out)
        self.assertNotIn('USE_RULES ',out)

    def test_ini_bom_crlf_inline_comments_multiline_and_unrelated_metadata(self):
        config=self.write('Documented_MUS.ini','\ufeff# Author notes\r\n[Metadata]\r\nAuthor=Composer\r\n'
            '[General] ; playlist\r\nMUSExploreForest! = <<<TRACKS\r\n'
            '800~Pack.esp, ; first\r\n801~Pack.esp # second\r\nTRACKS\r\n'
            '[Location]\r\nTestInn=AuthorStory ; keep quest conditions\r\n')
        out=self.scan(configs=[config])
        self.assertIn('CLEAR skyrim.esm:256',out)
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esp:2048',out)
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esp:2049',out)
        self.assertIn('ASSIGN 2:skyrim.esm:768 <- pack.esp:256',out)

    def test_invalid_rule_cannot_leave_a_partial_clear_or_mapping(self):
        good=self.write('Good_MUS.ini','[General]\nMUSExploreForest! =\n[Location]\nTestInn=AuthorStory')
        bad=self.write('Bad_MUS.ini','[General]\nMissing=800~Pack.esp')
        out=self.scan(configs=[good,bad])
        self.assertIn('CONFIG_HANDOFF',out)
        for prefix in ('CLEAR ','ASSIGN ','DISTRIBUTE ','APPEND '): self.assertNotIn(prefix,out)
