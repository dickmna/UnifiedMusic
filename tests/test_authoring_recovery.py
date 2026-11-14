"""Layered authoring, actual area use and preserving each authored music role."""
import itertools
import struct

from test_scene_takeover import SceneFixture, rows, permits
from test_plugin_scan import header, music, record, sub
from test_auto_combat_scan import condition, single_track
from test_audio_coexist import xwm, bsa


def location(key,parent=0):
    return record('LCTN',key,sub('EDID',b'Location\0')+sub('PNAM',struct.pack('<I',parent)))


class AuthoringRecoveryTests(SceneFixture):
    def layered(self):
        ordinary=(condition(707,0x35D59,comparison=0),)
        dragon=(condition(707,0x35D59),)
        base=self.write('Skyrim.esm',header()+music(0x100,(0x200,),name='MUSCombat',priority=2)
            +music(0x101,(0x201,),name='MUSCombatBoss',priority=2)
            +single_track(0x200,ordinary)+single_track(0x201,dragon))
        framework=self.write('Framework.esm',header(('Skyrim.esm',))
            +music(0x100,(0x200,0x01000800,0x01000801),name='MUSCombat',priority=1)
            +music(0x101,(0x201,0x01000801),name='MUSCombatBoss',priority=2)
            +single_track(0x01000800,(condition(520,0),),path='')
            +single_track(0x01000801,(condition(520,0),),path='')
            +single_track(0x200,ordinary+(condition(359,0x01000900,comparison=0,run_on=2,reference=0x14),))
            +location(0x01000900)+location(0x01000901,0x01000900)+location(0x01000902,0x01000901))
        cue=(condition(562,0x02000A00,flags=1),
             condition(359,0x01000902,flags=1,run_on=2,reference=0x14),
             condition(117,0x02000A01))+ordinary
        land=self.write('Land.esp',header(('Skyrim.esm','Framework.esm'))
            +record('KYWD',0x02000A00,sub('EDID',b'LocationKeyword\0'))
            +record('REGN',0x02000A01,sub('EDID',b'LandRegion\0'))
            +single_track(0x01000800,cue,path=r'Data\Music\Land\scene.wav')
            +single_track(0x01000801,dragon,path=r'Data\Music\Land\dragon.wav'))
        pack=self.write('Pack.esp',header(('Skyrim.esm',))
            +music(0x100,(0x200,0x01000800),name='MUSCombat',priority=3)
            +music(0x101,(0x201,),name='MUSCombatBoss',priority=2)
            +single_track(0x200,ordinary,path=r'data\music\fixture\battle.wav')
            +single_track(0x01000800,ordinary,path=r'Data\Music\Pack\added.wav'))
        return base,framework,land,pack

    def test_layered_tracks_restore_membership_and_inherit_parent_location_policy(self):
        paths=self.layered();before={p:p.read_bytes() for p in paths}
        out=self.scan(*paths)
        self.assertIn('APPEND skyrim.esm:256 <- framework.esm:2048',out)
        self.assertIn('APPEND skyrim.esm:256 <- framework.esm:2049',out)
        self.assertIn('APPEND skyrim.esm:257 <- framework.esm:2049',out)
        self.assertNotIn('APPEND skyrim.esm:257 <- pack.esp:2048',out)
        guards=rows(out,'USE_RULES')
        self.assertEqual({x['track'] for x in guards},{'skyrim.esm:512','pack.esp:2048'})
        for guard in guards:
            self.assertEqual(guard['type'],'skyrim.esm:256')
            self.assertTrue(permits(guard['clauses'],{(359,'framework.esm:2304'):0}))
            self.assertFalse(permits(guard['clauses'],{(359,'framework.esm:2304'):1}))
        self.assertEqual(before,{p:p.read_bytes() for p in paths})

    def test_layered_authoring_is_independent_of_scan_order(self):
        paths=self.layered()
        relevant=lambda text:sorted(s for s in text.splitlines() if s.startswith(('APPEND ','AUTO_COMBAT ','USE_RULES ')))
        expected=relevant(self.scan(*paths))
        for order in itertools.permutations(paths):self.assertEqual(relevant(self.scan(*order)),expected)

    def test_competing_final_track_definitions_are_not_chosen_arbitrarily(self):
        paths=self.layered()
        other=self.write('Other.esp',header(('Skyrim.esm','Framework.esm'))
            +single_track(0x01000800,(condition(359,0x01000902,comparison=0),),path=r'Data\Music\Other\cue.wav'))
        out=self.scan(*paths,other)
        self.assertNotIn('APPEND skyrim.esm:256 <- framework.esm:2048',out)
        self.assertNotIn('USE_RULES ',out)

    def test_explicit_boss_removal_does_not_remove_the_authored_default_use(self):
        paths=self.layered()
        patch=self.write('Patch.esp',header(('Skyrim.esm','Framework.esm'))+music(0x101,(0x201,),name='MUSCombatBoss',priority=2))
        out=self.scan(*paths,patch)
        self.assertIn('APPEND skyrim.esm:256 <- framework.esm:2049',out)
        self.assertNotIn('APPEND skyrim.esm:257 <- framework.esm:2049',out)

    def test_missing_scene_resource_keeps_independent_dragon_memberships(self):
        paths=self.layered()
        self.write('Original.bsa',bsa([('music/fixture/battle.xwm',xwm(b'original'))]))
        (self.root/'visible').mkdir()
        out=self.scan(*paths,audio=True)
        self.assertIn('SCENE_RESOURCE_SKIP ',out)
        self.assertNotIn('APPEND skyrim.esm:256 <- framework.esm:2048',out)
        self.assertIn('APPEND skyrim.esm:256 <- framework.esm:2049',out)
        self.assertIn('APPEND skyrim.esm:257 <- framework.esm:2049',out)
        self.assertFalse(rows(out,'USE_RULES'))

    def test_ambiguous_or_cyclic_location_hierarchy_does_not_invent_scope(self):
        paths=self.layered()
        for parent in (0x01000902,0):
            patch=self.write('Patch.esp',header(('Skyrim.esm','Framework.esm'))+location(0x01000901,parent))
            self.assertNotIn('pack.esp:2048',{r['track'] for r in rows(self.scan(*paths,patch),'USE_RULES')})

    def test_test_named_town_is_ordinary_only_when_official_area_really_uses_it(self):
        for mapped in (False,True):
            assignment=record('CELL',0x400,sub('XCMO',struct.pack('<I',0x100))) if mapped else b''
            base=self.write('Skyrim.esm',header()+music(0x100,(0x200,),name='MUSTownTest')
                +single_track(0x200)+assignment)
            pack=self.write('Pack.esp',header(('Skyrim.esm',))+music(0x100,(0x200,0x01000800),name='MUSTownTest')
                +single_track(0x01000800))
            self.assertEqual('APPEND skyrim.esm:256 <- pack.esp:2048' in self.scan(base,pack),mapped)

    def test_mapping_does_not_turn_a_boss_or_one_shot_type_into_ambient(self):
        for name,flags in (('MUSTownBossTest',4),('MUSTownTest',5)):
            base=self.write('Skyrim.esm',header()+music(0x100,(0x200,),name=name,music_flags=flags)
                +single_track(0x200)+record('CELL',0x400,sub('XCMO',struct.pack('<I',0x100))))
            pack=self.write('Pack.esp',header(('Skyrim.esm',))+music(0x100,(0x200,0x01000800),name=name,music_flags=flags)
                +single_track(0x01000800))
            self.assertNotIn('APPEND ',self.scan(base,pack))

    def test_existing_original_track_can_be_an_authored_addition_to_another_list(self):
        base=self.write('Skyrim.esm',header()+music(0x100,(0x200,))+single_track(0x200)+single_track(0x201))
        pack=self.write('Pack.esp',header(('Skyrim.esm',))+music(0x100,(0x200,0x201)))
        later=self.write('Later.esp',header(('Skyrim.esm',))+music(0x100,(0x200,)))
        self.assertIn('APPEND skyrim.esm:256 <- skyrim.esm:513',self.scan(base,pack,later))
