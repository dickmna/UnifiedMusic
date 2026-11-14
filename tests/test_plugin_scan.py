"""Native scanner acceptance tests using synthetic TES5 plugins only.

Run: python tests/test_plugin_scan.py --inspect PATH/UnifiedMusicInspect.exe -v
or set UNIFIED_MUSIC_INSPECT and use python -m unittest discover -s tests -v.
No Skyrim installation, paid assets, or third-party Python modules are needed.
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
import zlib


def sub(signature: str, body: bytes, *, extended: bool = False) -> bytes:
    if extended:
        return b"XXXX" + struct.pack("<HI", 4, len(body)) + signature.encode("ascii") + b"\0\0" + body
    return signature.encode("ascii") + struct.pack("<H", len(body)) + body


def record(signature: str, form_id: int, body: bytes, *, flags: int = 0,
           compressed: bool = False) -> bytes:
    if compressed:
        body = struct.pack("<I", len(body)) + zlib.compress(body)
        flags |= 0x40000
    # Signature, data length, flags, file-local form ID, revision, version, unknown.
    return struct.pack("<4sIIIIHH", signature.encode("ascii"), len(body), flags,
                       form_id, 0, 44, 0) + body


def group(children: bytes, label: bytes = b"MUSC") -> bytes:
    return struct.pack("<4sI4sIHHHH", b"GRUP", 24 + len(children), label, 0, 0, 0, 0, 0) + children


def header(masters: tuple[str, ...] = ()) -> bytes:
    body = sub("HEDR", struct.pack("<fII", 1.7, 1, 0x800))
    for master in masters:
        body += sub("MAST", master.encode("ascii") + b"\0") + sub("DATA", b"\0" * 8)
    return record("TES4", 0, body)


def music(form_id: int, tracks: tuple[int, ...], *, name: str = "MUSExploreForest",
          priority: int = 50, flags: int = 0, compressed: bool = False,
          extended: bool = False, music_flags: int = 4) -> bytes:
    body = (sub("EDID", name.encode("ascii") + b"\0")
            + sub("FNAM", struct.pack("<I", music_flags))
            + sub("PNAM", struct.pack("<HH", priority, 0))
            + sub("WNAM", struct.pack("<f", 4.0))
            + sub("TNAM", b"".join(struct.pack("<I", t) for t in tracks), extended=extended))
    return record("MUSC", form_id, body, flags=flags, compressed=compressed)


def track(form_id: int, name: str) -> bytes:
    # A valid silent Music Track, avoiding all audio path dependencies.
    body = (sub("EDID", name.encode("ascii") + b"\0")
            + sub("CNAM", struct.pack("<I", 0xA1A9C4D5))
            + sub("FLTV", struct.pack("<f", 1.0)))
    return record("MUST", form_id, body)


def script_reference(form_id: int, referenced_music: int) -> bytes:
    def wstring(value: str) -> bytes:
        payload = value.encode("ascii")
        return struct.pack("<H", len(payload)) + payload
    vmad = (struct.pack("<HHH", 5, 2, 1)
            + wstring("QuestMusicController") + b"\0" + struct.pack("<H", 1)
            + wstring("ProtectedMusic") + bytes([1, 1])
            + struct.pack("<HHI", 0, 0xFFFF, referenced_music))
    return record("QUST", form_id, sub("EDID", b"MusicQuest\0") + sub("VMAD", vmad))


class PluginScanTests(unittest.TestCase):
    TYPE = "skyrim.esm:256"
    BASE_TRACK = 0x200
    A_TRACK = 0x01000800
    B_TRACK = 0x01000900

    @classmethod
    def setUpClass(cls):
        candidate = os.environ.get("UNIFIED_MUSIC_INSPECT")
        if not candidate:
            raise RuntimeError("Set UNIFIED_MUSIC_INSPECT or pass --inspect PATH to the native scanner executable")
        cls.exe = Path(candidate).resolve()
        if not cls.exe.is_file():
            raise RuntimeError(f"Native scanner not found: {cls.exe}")

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="UnifiedMusic-fixtures-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write(self, name: str, body: bytes) -> Path:
        path = self.root / name
        path.write_bytes(body)
        return path

    def base(self, *, name: str = "MUSExploreForest", compressed: bool = False) -> Path:
        return self.write("Skyrim.esm", header() + group(music(0x100, (self.BASE_TRACK,), name=name,
                                                           compressed=compressed))
                          + group(track(self.BASE_TRACK, "BaseTrack"), b"MUST"))

    def addon(self, name: str = "A.esp", *, tracks: tuple[int, ...] | None = None,
              priority: int = 50, masters: tuple[str, ...] = ("Skyrim.esm",),
              editor_id: str = "MUSExploreForest", compressed: bool = False,
              extended: bool = False, flags: int = 0) -> Path:
        if tracks is None:
            tracks = (self.BASE_TRACK, self.A_TRACK)
        records = music(0x100, tracks, priority=priority, name=editor_id,
                        compressed=compressed, extended=extended, flags=flags)
        # Populate own new track records, so fixtures are not dangling references.
        own_index = len(masters)
        owned = sorted({value for value in tracks if value >> 24 == own_index})
        must = b"".join(track(value, "AddedTrack" + str(value & 0xFFFFFF)) for value in owned)
        return self.write(name, header(masters) + group(records) + (group(must, b"MUST") if must else b""))

    def run_scan(self, *plugins: Path, ok: bool = True):
        proc = subprocess.run([str(self.exe), *(str(p) for p in plugins)],
                              capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20)
        if ok:
            self.assertEqual(proc.returncode, 0, proc.stdout + "\n" + proc.stderr)
        else:
            self.assertNotEqual(proc.returncode, 0, "Malformed input was accepted:\n" + proc.stdout)
            self.assertFalse(any(line.startswith(("ALLOW ", "APPEND ")) for line in proc.stdout.splitlines()),
                             "Failure leaked a partial actionable merge plan")
            return proc
        allowed, additions, preserved = set(), {}, []
        for line in proc.stdout.splitlines():
            if line.startswith("ALLOW "):
                allowed.add(line[6:].strip())
            elif line.startswith("APPEND "):
                target, added = line[7:].split(" <- ", 1)
                additions.setdefault(target.strip(), []).append(added.strip())
            elif line.startswith("PRESERVE "):
                preserved.append(line)
        return allowed, additions, preserved

    def assert_preserved(self, *plugins: Path):
        allowed, additions, _ = self.run_scan(*plugins)
        self.assertNotIn(self.TYPE, allowed)
        self.assertNotIn(self.TYPE, additions)

    def test_two_independent_addons_union_even_when_last_override_hides_first(self):
        allowed, additions, _ = self.run_scan(self.base(), self.addon(),
            self.addon("B.esp", tracks=(self.BASE_TRACK, self.B_TRACK)))
        self.assertIn(self.TYPE, allowed)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_original_record_read_after_overrides_still_merges(self):
        base, a = self.base(), self.addon()
        b = self.addon("B.esp", tracks=(self.BASE_TRACK, self.B_TRACK))
        for order in itertools.permutations((base, a, b)):
            with self.subTest(order=[p.name for p in order]):
                allowed, additions, _ = self.run_scan(*order)
                self.assertIn(self.TYPE, allowed)
                self.assertCountEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_duplicate_additions_are_appended_once_with_master_remapping(self):
        a = self.addon(tracks=(self.BASE_TRACK, self.A_TRACK, self.A_TRACK))
        b = self.addon("B.esp", masters=("Skyrim.esm", "A.esp"),
                       tracks=(self.BASE_TRACK, self.A_TRACK, 0x02000900))
        _, additions, _ = self.run_scan(self.base(), a, b)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_form_ids_are_resolved_by_master_order_not_raw_integer(self):
        base = self.base()
        a = self.addon()
        reordered = self.write("B.esp", header(("A.esp", "Skyrim.esm"))
            + group(music(0x01000100, (0x01000200, 0x00000800, 0x02000900))))
        _, additions, _ = self.run_scan(base, a, reordered)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_metadata_change_protects_type(self):
        self.assert_preserved(self.base(), self.addon(), self.addon("B.esp", priority=10))

    def test_removed_base_track_protects_type(self):
        self.assert_preserved(self.base(), self.addon(tracks=(self.A_TRACK,)))

    def test_deleted_record_protects_type(self):
        self.assert_preserved(self.base(), self.addon(flags=0x20))

    def test_late_script_reference_keeps_public_type_role(self):
        quest = self.write("Quest.esp", header(("Skyrim.esm",))
                           + script_reference(0x01000900, 0x100))
        _, additions, _ = self.run_scan(self.base(), self.addon(), quest)
        self.assertEqual(additions, {self.TYPE: ['a.esp:2048']})

    def test_vmad_reference_respects_master_order(self):
        quest = self.write("Quest.esp", header(("A.esp", "Skyrim.esm"))
                           + script_reference(0x02000900, 0x01000100))
        _, additions, _ = self.run_scan(self.base(), self.addon(), quest)
        self.assertEqual(additions, {self.TYPE: ['a.esp:2048']})

    def test_unrelated_vmad_same_local_id_does_not_protect_other_plugin(self):
        quest = self.write("Quest.esp", header(("Skyrim.esm", "Other.esm"))
                           + script_reference(0x02000900, 0x01000100))
        allowed, additions, _ = self.run_scan(self.base(), self.addon(), quest)
        self.assertIn(self.TYPE, allowed)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048"])

    def test_vanilla_boss_music_is_never_merged(self):
        self.assert_preserved(self.base(name="MUSCombatBoss"), self.addon(editor_id="MUSCombatBoss"))

    def test_custom_exploration_and_boss_types_are_never_merged(self):
        for editor_id in ("MyLandMUSCombatBoss", "MyLandMUSExploreForest"):
            with self.subTest(editor_id=editor_id):
                land = self.write("Land.esm", header(("Skyrim.esm",))
                                  + group(music(0x01000100, (0x01000200,), name=editor_id)))
                override = self.write("Addon.esp", header(("Skyrim.esm", "Land.esm"))
                                      + group(music(0x01000100, (0x01000200, 0x02000800), name=editor_id)))
                allowed, additions, _ = self.run_scan(self.base(), land, override)
                self.assertNotIn("land.esm:256", allowed)
                self.assertNotIn("land.esm:256", additions)

    def test_silence_and_event_named_types_are_never_merged(self):
        for name in ("MUSTownSILENCE", "MUSExploreReveal", "MUSDungeonFinale", "MUSTavernTest"):
            with self.subTest(name=name):
                self.assert_preserved(self.base(name=name), self.addon(editor_id=name))

    def test_compressed_records_produce_same_additions(self):
        allowed, additions, _ = self.run_scan(self.base(compressed=True), self.addon(compressed=True))
        self.assertIn(self.TYPE, allowed)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048"])

    def test_valid_extended_tnam_is_accepted(self):
        _, additions, _ = self.run_scan(self.base(), self.addon(extended=True))
        self.assertEqual(additions[self.TYPE], ["a.esp:2048"])

    def test_partial_form_placeholder_does_not_erase_playlist(self):
        placeholder = self.write("Placeholder.esp", header(("Skyrim.esm",))
                                 + record("MUSC", 0x100, sub("EDID", b"MUSExploreForest\0"), flags=0x4000))
        allowed, additions, _ = self.run_scan(self.base(), self.addon(), placeholder)
        self.assertIn(self.TYPE, allowed)
        self.assertEqual(additions[self.TYPE], ["a.esp:2048"])

    def test_missing_original_record_is_not_treated_as_baseline(self):
        self.assert_preserved(self.addon())

    def test_dependent_patch_intentionally_removing_addition_is_preserved(self):
        patch = self.addon("RemoveA.esp", masters=("Skyrim.esm", "A.esp"), tracks=(self.BASE_TRACK,))
        self.assert_preserved(self.base(), self.addon(), patch)

    def test_transitive_dependent_patch_removing_addition_is_preserved(self):
        base, addon = self.base(), self.addon()
        bridge = self.write('Bridge.esp', header(('Skyrim.esm', 'A.esp')))
        patch = self.write('Patch.esp', header(('Skyrim.esm', 'Bridge.esp'))
                           + group(music(0x100, (self.BASE_TRACK,))))
        for paths in [(base, addon, bridge, patch), (patch, bridge, addon, base)]:
            with self.subTest(order=[p.name for p in paths]):
                self.assert_preserved(*paths)

    def test_dependent_removal_patch_read_before_dependency_still_protects(self):
        base, a = self.base(), self.addon()
        patch = self.addon("RemoveA.esp", masters=("Skyrim.esm", "A.esp"), tracks=(self.BASE_TRACK,))
        for order in itertools.permutations((base, a, patch)):
            with self.subTest(order=[p.name for p in order]):
                self.assert_preserved(*order)

    def conflicting_addons(self):
        """Independent B overrides the ambient list and hides A's contribution."""
        return (self.base(), self.addon(),
                self.addon("B.esp", tracks=(self.BASE_TRACK, self.B_TRACK)))

    def assert_only_b_restored(self, *plugins: Path):
        allowed, additions, _ = self.run_scan(*plugins)
        self.assertIn(self.TYPE, allowed, "Protecting one track must not disable the safe target")
        self.assertEqual(additions.get(self.TYPE), ["b.esp:2304"])

    def assert_shared_membership_restored(self, *plugins: Path):
        allowed, additions, _ = self.run_scan(*plugins)
        self.assertIn(self.TYPE, allowed)
        self.assertEqual(set(additions), {self.TYPE}, 'Dedicated owners receive no additions')
        self.assertCountEqual(additions[self.TYPE], ['a.esp:2048', 'b.esp:2304'])

    def test_shared_custom_music_track_restores_ordinary_membership_to_ambient(self):
        base, a, b = self.conflicting_addons()
        a.write_bytes(a.read_bytes() + group(music(0x01000A00, (self.A_TRACK,), name="QuestFinale")))
        self.assert_shared_membership_restored(base, a, b)

    def test_shared_custom_track_membership_is_independent_of_scan_order(self):
        base, a, b = self.conflicting_addons()
        protection = self.write("Story.esp", header(("Skyrim.esm", "A.esp"))
            + group(music(0x02000A00, (self.A_TRACK,), name="IndependentStoryMusic")))
        for order in itertools.permutations((base, a, b, protection)):
            with self.subTest(order=[p.name for p in order]):
                self.assert_shared_membership_restored(*order)

    def test_direct_script_track_reference_is_not_restored_to_ambient(self):
        base, a, b = self.conflicting_addons()
        quest = self.write("Story.esp", header(("Skyrim.esm", "A.esp"))
                           + script_reference(0x02000A00, self.A_TRACK))
        self.assert_only_b_restored(base, a, b, quest)

    def test_direct_script_track_reference_resolves_reordered_masters(self):
        base, a, b = self.conflicting_addons()
        quest = self.write("Story.esp", header(("A.esp", "Skyrim.esm"))
                           + script_reference(0x02000A00, 0x00000800))
        self.assert_only_b_restored(base, a, b, quest)

    def test_unrelated_script_track_local_id_does_not_block_ambient_addition(self):
        base, a, b = self.conflicting_addons()
        quest = self.write("Story.esp", header(("Skyrim.esm", "Other.esp"))
                           + script_reference(0x02000A00, self.A_TRACK))
        _, additions, _ = self.run_scan(base, a, b, quest)
        self.assertCountEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_track_shared_with_script_bound_base_type_restores_ordinary_membership(self):
        base, a, b = self.conflicting_addons()
        base.write_bytes(base.read_bytes() + group(music(0x300, (self.BASE_TRACK,), name="MUSTownScripted")))
        a.write_bytes(a.read_bytes() + group(music(0x300, (self.BASE_TRACK, self.A_TRACK), name="MUSTownScripted")))
        quest = self.write("Story.esp", header(("Skyrim.esm",))
                           + script_reference(0x01000A00, 0x300))
        _, additions, _ = self.run_scan(base, a, b, quest)
        self.assertEqual(additions, {self.TYPE: ['a.esp:2048', 'b.esp:2304'], 'skyrim.esm:768': ['a.esp:2048']})

    def test_track_shared_with_one_shot_base_type_restores_ordinary_membership(self):
        base, a, b = self.conflicting_addons()
        base.write_bytes(base.read_bytes() + group(music(0x300, (self.BASE_TRACK,),
                                                       name="MUSSpecial", music_flags=1)))
        a.write_bytes(a.read_bytes() + group(music(0x300, (self.BASE_TRACK, self.A_TRACK),
                                                  name="MUSSpecial", music_flags=1)))
        self.assert_shared_membership_restored(base, a, b)

    def test_track_shared_with_native_boss_type_restores_ordinary_membership(self):
        base, a, b = self.conflicting_addons()
        base.write_bytes(base.read_bytes() + group(music(0x300, (self.BASE_TRACK,), name="MUSCombatBoss")))
        a.write_bytes(a.read_bytes() + group(music(0x300, (self.BASE_TRACK, self.A_TRACK), name="MUSCombatBoss")))
        self.assert_shared_membership_restored(base, a, b)

    def test_track_shared_with_other_mapped_type_restores_ordinary_membership(self):
        for signature, field in (("CELL", "XCMO"), ("WRLD", "ZNAM"),
                                 ("LCTN", "NAM1"), ("REGN", "RDMO")):
            with self.subTest(signature=signature):
                base, a, b = self.conflicting_addons()
                base.write_bytes(base.read_bytes() + group(music(0x300, (self.BASE_TRACK,), name="MUSSpecial")))
                a.write_bytes(a.read_bytes() + group(music(0x300, (self.BASE_TRACK, self.A_TRACK), name="MUSSpecial"))
                              + group(record(signature, 0x01000B00,
                                      sub(field, struct.pack("<I", 0x300))), signature.encode("ascii")))
                self.assert_shared_membership_restored(base, a, b)

    def test_ambient_target_own_map_assignment_does_not_block_its_additions(self):
        base, a, b = self.conflicting_addons()
        a.write_bytes(a.read_bytes() + group(record("CELL", 0x01000B00,
                      sub("XCMO", struct.pack("<I", 0x100))), b"CELL"))
        _, additions, _ = self.run_scan(base, a, b)
        self.assertCountEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])

    def test_ordinary_ambient_lists_can_share_tracks_despite_map_assignments(self):
        for signature, field in (("CELL", "XCMO"), ("WRLD", "ZNAM"),
                                 ("LCTN", "NAM1"), ("REGN", "RDMO")):
            with self.subTest(signature=signature):
                base, a, b = self.conflicting_addons()
                base.write_bytes(base.read_bytes() + group(music(0x300, (self.BASE_TRACK,), name="MUSTownRegional")))
                a.write_bytes(a.read_bytes() + group(music(0x300, (self.BASE_TRACK, self.A_TRACK), name="MUSTownRegional"))
                              + group(record(signature, 0x01000B00,
                                      sub(field, struct.pack("<I", 0x300))), signature.encode("ascii")))
                _, additions, _ = self.run_scan(base, a, b)
                self.assertCountEqual(additions[self.TYPE], ["a.esp:2048", "b.esp:2304"])
                self.assertEqual(additions["skyrim.esm:768"], ["a.esp:2048"])

    def test_deleted_ambient_track_is_not_resurrected(self):
        base, a, b = self.conflicting_addons()
        patch = self.write("RemoveTrack.esp", header(("Skyrim.esm", "A.esp"))
                           + group(record("MUST", self.A_TRACK, b"", flags=0x20), b"MUST"))
        self.assert_only_b_restored(base, a, b, patch)

    def test_invalid_ambient_track_conditions_are_not_restored(self):
        base, a, b = self.conflicting_addons()
        body = track(self.A_TRACK, "InvalidTrack")[24:] + sub("CITC", struct.pack("<I", 1))
        patch = self.write("InvalidTrack.esp", header(("Skyrim.esm", "A.esp"))
                           + group(record("MUST", self.A_TRACK, body), b"MUST"))
        self.assert_only_b_restored(base, a, b, patch)

    def test_malformed_ambient_track_condition_is_not_restored(self):
        base, a, b = self.conflicting_addons()
        body = (track(self.A_TRACK, "InvalidTrack")[24:] + sub("CITC", struct.pack("<I", 1))
                + sub("CTDA", b"\0" * 31))
        patch = self.write("InvalidTrack.esp", header(("Skyrim.esm", "A.esp"))
                           + group(record("MUST", self.A_TRACK, body), b"MUST"))
        self.assert_only_b_restored(base, a, b, patch)

    def test_track_referenced_by_palette_restores_ordinary_membership_to_ambient(self):
        base, a, b = self.conflicting_addons()
        palette = (sub("EDID", b"StoryPalette\0") + sub("CNAM", struct.pack("<I", 0x23F678C3))
                   + sub("SNAM", struct.pack("<I", 0x00000800)))
        # A is index 0 in this plugin, unlike index 1 in A itself.
        other = self.write("Story.esp", header(("A.esp", "Skyrim.esm"))
                           + group(record("MUST", 0x02000C00, palette), b"MUST"))
        self.assert_shared_membership_restored(base, a, b, other)

    def malformed(self, body: bytes):
        return self.write("Malformed.esp", header(("Skyrim.esm",)) + body)

    def test_truncated_record_header_is_rejected(self):
        self.run_scan(self.base(), self.malformed(b"MUSC" + b"\0" * 19), ok=False)

    def test_record_length_beyond_file_is_rejected(self):
        malformed = struct.pack("<4sIIIIHH", b"MUSC", 0xFFFFFFFF, 0, 0x100, 0, 44, 0)
        self.run_scan(self.base(), self.malformed(malformed), ok=False)

    def test_group_length_smaller_than_header_is_rejected(self):
        malformed = struct.pack("<4sI4sIHHHH", b"GRUP", 23, b"MUSC", 0, 0, 0, 0, 0)
        self.run_scan(self.base(), self.malformed(malformed), ok=False)

    def test_group_length_beyond_file_is_rejected(self):
        malformed = struct.pack("<4sI4sIHHHH", b"GRUP", 1024, b"MUSC", 0, 0, 0, 0, 0)
        self.run_scan(self.base(), self.malformed(malformed), ok=False)

    def test_group_depth_limit_is_enforced(self):
        payload = music(0x100, (self.BASE_TRACK,))
        for _ in range(40):
            payload = group(payload)
        self.run_scan(self.base(), self.malformed(payload), ok=False)

    def test_bad_xxxx_boundaries_are_rejected(self):
        cases = {
            "orphan": b"XXXX" + struct.pack("<HI", 4, 10),
            "short_payload": b"XXXX" + struct.pack("<H", 4) + b"\0\0",
            "wrong_size": b"XXXX" + struct.pack("<H", 2) + b"\0\0",
            "double": (b"XXXX" + struct.pack("<HI", 4, 4)) * 2,
            "overrun": b"XXXX" + struct.pack("<HI", 4, 0xFFFFFFFE) + b"TNAM\0\0" + b"\0" * 4,
        }
        for label, body in cases.items():
            with self.subTest(label=label):
                self.run_scan(self.base(), self.malformed(record("MUSC", 0x100, body)), ok=False)

    def test_unaligned_music_reference_array_is_rejected(self):
        self.run_scan(self.base(), self.malformed(record("MUSC", 0x100, sub("TNAM", b"abc"))), ok=False)

    def test_invalid_master_index_is_rejected(self):
        self.run_scan(self.base(), self.addon(tracks=(self.BASE_TRACK, 0xFF000800)), ok=False)

    def test_invalid_compressed_payload_is_rejected(self):
        self.run_scan(self.base(), self.malformed(record("MUSC", 0x100,
                      struct.pack("<I", 100) + b"not-zlib", flags=0x40000)), ok=False)

    def test_compressed_size_mismatch_is_rejected(self):
        raw = sub("EDID", b"MUSExploreForest\0")
        payload = struct.pack("<I", len(raw) + 1) + zlib.compress(raw)
        self.run_scan(self.base(), self.malformed(record("MUSC", 0x100, payload, flags=0x40000)), ok=False)

    def test_decompression_allocation_limit_is_enforced(self):
        payload = struct.pack("<I", 0xFFFFFFFF) + zlib.compress(b"")
        self.run_scan(self.base(), self.malformed(record("MUSC", 0x100, payload, flags=0x40000)), ok=False)

    def test_second_tes4_header_is_rejected(self):
        self.run_scan(self.base(), self.malformed(header(("Skyrim.esm",))), ok=False)

    def test_master_path_traversal_is_rejected(self):
        plugin = self.write("Traversal.esp", header(("..\\Skyrim.esm",))
                            + group(music(0x100, (self.BASE_TRACK,))))
        self.run_scan(self.base(), plugin, ok=False)

    def test_failure_after_valid_addon_emits_no_actionable_partial_plan(self):
        self.run_scan(self.base(), self.addon(), self.malformed(b"broken"), ok=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--inspect")
    options, rest = parser.parse_known_args()
    if options.inspect:
        os.environ["UNIFIED_MUSIC_INSPECT"] = options.inspect
    unittest.main(argv=[__file__, *rest])
