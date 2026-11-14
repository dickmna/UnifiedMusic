"""Counterexamples to song-count heuristics and role-local fallback."""
import itertools

from test_scene_takeover import SceneFixture, rows, permits
from test_plugin_scan import header
from test_auto_combat_scan import area, condition, single_track
from test_audio_coexist import audio_track


class InheritanceTests(SceneFixture):
    def test_two_matching_restrictions_without_scene_arrangement_are_song_edits(self):
        extra=single_track(0x201,(condition(comparison=0),),path=r'Data\Music\Fixture\other.wav')
        out=self.scan(*self.fixture(scope=(),extra=extra))
        self.assertNotIn('SCENE_POLICY ',out)
        self.assertFalse(rows(out,'USE_RULES'))

    def test_same_area_in_unrelated_sources_does_not_establish_takeover(self):
        paths=self.fixture(exclude=False,scope=(condition(parameter=0x900),),base_extra=area('WRLD',0x900))
        other=self.write('Unrelated.esp',header(('Skyrim.esm',))+audio_track(terms=(condition(parameter=0x900,comparison=0),)))
        self.assertNotIn('SCENE_POLICY ',self.scan(*paths,other))

    def test_partial_evidence_with_related_scene_needs_no_second_song(self):
        out=self.scan(*self.fixture())
        self.assertTrue(self.pack_rule(out)['clauses'])

    def test_source_night_condition_does_not_become_destination_day_condition(self):
        night=(condition(18,0,flags=0x61,comparison=20),condition(18,0,flags=0x80,comparison=6))
        day=(condition(18,0,flags=0x60,comparison=8),condition(18,0,flags=0x80,comparison=18))
        paths=self.fixture(ambient=True,role=night)
        original=audio_track(0x01000800,path=r'Data\Music\Pack\added.wav',terms=night)
        replacement=audio_track(0x01000800,path=r'Data\Music\Pack\added.wav',terms=day)
        self.assertIn(original,paths[2].read_bytes())
        paths[2].write_bytes(paths[2].read_bytes().replace(original,replacement))
        before=paths[2].read_bytes()
        rule=self.pack_rule(self.scan(*paths))
        self.assertEqual([t['function'] for c in rule['clauses'] for t in c],[310])
        self.assertEqual(paths[2].read_bytes(),before)

    def test_new_standalone_clock_restriction_is_not_a_category_ban(self):
        restriction=(condition(comparison=0),condition(18,0,flags=0x60,comparison=20))
        out=self.scan(*self.fixture(ambient=True,restriction=restriction))
        self.assertEqual([t['function'] for c in self.pack_rule(out)['clauses'] for t in c],[310])

    def test_only_common_added_clauses_are_propagated(self):
        restriction=(condition(comparison=0),condition(58,0x01000A00,flags=0x20,comparison=40))
        extra=area('QUST',0x01000A00)+single_track(0x201,(condition(comparison=0),),path=r'Data\Music\Fixture\other.wav')
        out=self.scan(*self.fixture(restriction=restriction,extra=extra))
        self.assertEqual([t['function'] for c in self.pack_rule(out)['clauses'] for t in c],[310])

    def test_different_conditional_restrictions_are_not_flattened_to_common_area(self):
        first=(condition(comparison=0,flags=1),condition(58,0x01000A00,flags=0x80,comparison=40))
        second=(condition(comparison=0,flags=1),condition(58,0x01000A00,flags=0x60,comparison=50))
        extra=area('QUST',0x01000A00)+single_track(0x201,second,path=r'Data\Music\Fixture\other.wav')
        out=self.scan(*self.fixture(restriction=first,extra=extra))
        self.assertFalse(rows(out,'USE_RULES'))

    def test_cue_quest_gates_do_not_replace_the_authored_ordinary_restriction(self):
        scope=(condition(),condition(58,0x01000A00,flags=0x60,comparison=40),condition(58,0x01000A00,flags=0x80,comparison=50))
        out=self.scan(*self.fixture(scope=scope,extra=area('QUST',0x01000A00)))
        clauses=self.pack_rule(out)['clauses']
        for inside in (0,1):
            self.assertEqual(permits(clauses,{(310,'land.esp:2304'):inside}),not inside)

    def test_terminal_or_on_read_only_cue_is_preserved(self):
        paths=self.fixture(scope=(condition(flags=1),))
        before=paths[1].read_bytes()
        out=self.scan(*paths)
        self.assertTrue(self.pack_rule(out)['clauses'])
        self.assertNotIn('land.esp:2048',{r['track'] for r in rows(out,'USE_RULES')})
        self.assertEqual(paths[1].read_bytes(),before)

    def with_dragon_distribution(self,**kwargs):
        paths=self.fixture(pack_extra=single_track(0x01000801,(condition(707,0x35D59),)),**kwargs)
        config=self.write('Pack_MUS.ini',b'[General]\nMUSCombat=800~Pack.esp,801~Pack.esp\n')
        return paths,config

    def assert_dragon_survives(self,out):
        self.assertIn('DISTRIBUTE skyrim.esm:256 <- pack.esp:2049',out)
        self.assertNotIn('DISTRIBUTE skyrim.esm:256 <- pack.esp:2048',out)
        self.assertNotIn('AUTO_COMBAT ',out)
        self.assertFalse(rows(out,'USE_RULES'))

    def test_unreadable_cue_pauses_ordinary_use_but_retains_dragon_distribution(self):
        paths,config=self.with_dragon_distribution(scope=(condition(),condition(999,0)))
        out=self.scan(*paths,configs=(config,))
        self.assertIn('SCENE_UNSUPPORTED ',out)
        self.assert_dragon_survives(out)

    def test_missing_scene_audio_drops_only_dependent_uses(self):
        paths,config=self.with_dragon_distribution()
        self.resources(scene=False)
        out=self.scan(*paths,configs=(config,),audio=True)
        self.assertIn('SCENE_RESOURCE_SKIP ',out)
        self.assert_dragon_survives(out)

    def test_reordered_same_source_witnesses_produce_identical_common_rules(self):
        restriction=(condition(comparison=0),condition(58,0x01000A00,flags=0x20,comparison=40))
        extra=area('QUST',0x01000A00)+single_track(0x201,tuple(reversed(restriction)),path=r'Data\Music\Fixture\other.wav')
        paths=self.fixture(restriction=restriction,extra=extra)
        expected=self.pack_rule(self.scan(*paths))
        self.assertEqual(len(expected['clauses']),2)
        for order in itertools.permutations(paths):
            self.assertEqual(self.pack_rule(self.scan(*order)),expected)
