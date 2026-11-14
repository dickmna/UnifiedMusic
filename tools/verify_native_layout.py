"""Read-only ABI evidence for Steam SkyrimSE.exe 1.7.104.0.

Optional developer check: pip install pefile capstone
No executable bytes, disassembly, game assets or absolute paths enter the report.
This inspects disk code; it does not execute SKSE, resolve an Address Library,
prove native allocation succeeds, or validate playback/save behavior.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def verify(path):
    raw = path.read_bytes()
    pe = pefile.PE(data=raw)
    fixed = pe.VS_FIXEDFILEINFO[0]
    version = (fixed.FileVersionMS >> 16, fixed.FileVersionMS & 65535,
               fixed.FileVersionLS >> 16, fixed.FileVersionLS & 65535)
    if version != (1, 7, 104, 0):
        raise ValueError(f'This probe is limited to 1.7.104.0; found {version}')
    base = pe.OPTIONAL_HEADER.ImageBase
    decoder = Cs(CS_ARCH_X86, CS_MODE_64)
    checks = {}

    def instructions(rva, length):
        return [(i.mnemonic, i.op_str) for i in decoder.disasm(pe.get_data(rva, length), base+rva)]

    def require(name, value):
        if not value:
            raise AssertionError(name)
        checks[name] = True

    def entry(table, index):
        return struct.unpack('<Q', pe.get_data(table+8*index, 8))[0]-base

    require('single_factory_create_entry', entry(0x1833910, 1) == 0x325D50)
    require('single_factory_destroy_entry', entry(0x1833910, 2) == 0x325E60)
    create = instructions(0x325D50, 0x109)
    require('single_allocation_size_0x80', ('mov', 'edx, 0x80') in create)
    require('fresh_main_sound_handle', ('mov', 'dword ptr [rsi + 0x68], 0xffffffff') in create)
    require('fresh_finale_sound_handle', ('mov', 'dword ptr [rsi + 0x74], 0xffffffff') in create)
    require('fresh_loop_pointer', ('mov', 'qword ptr [rsi + 0x60], rbp') in create)
    require('single_can_play_entry', entry(0x1833C08, 7) == 0x326D00)
    require('conditions_are_pointer_at_0x10', instructions(0x326D00, 4) == [('mov', 'rcx, qword ptr [rcx + 0x10]')])
    load = instructions(0x326DF0, 0xA0)
    require('condition_holder_allocation_size_8', ('lea', 'edx, [r9 + 8]') in load)
    require('condition_holder_pointer_store', ('mov', 'qword ptr [rdi + 0x10], rax') in load)
    require('music_type_play_entry', entry(0x18346B8, 1) == 0x327C10)
    play = instructions(0x327C10, 0x230)
    require('type_uses_track_interface_can_play', ('call', 'qword ptr [rax + 0x38]') in play)
    require('type_uses_track_interface_play', ('call', 'qword ptr [rax + 0x10]') in play)
    update = instructions(0x3279B0, 0x84)
    require('type_uses_track_interface_status', ('call', 'qword ptr [rax + 0x40]') in update)
    require('type_uses_track_interface_update', ('jmp', 'qword ptr [rax + 8]') in update)

    # MTD-style Location/Region assignment touches only these native music
    # fields. Verify the manager's existing sound-entry cast rather than adding
    # another region interpretation or relying on an unchecked virtual layout.
    require('region_manager_sound_cast_entry', entry(0x1809918, 8) == 0x2519B0)
    region_cast = instructions(0x2519B0, 0x2D)
    require('region_cast_checks_native_type_seven',
            ('call', 'qword ptr [rax + 0x18]') in region_cast and ('cmp', 'eax, 7') in region_cast)
    require('region_sound_type_entry', entry(0x1809DB0, 3) == 0x253310)
    require('region_sound_type_is_seven', instructions(0x253310, 6) == [('mov', 'eax, 7'), ('ret', '')])
    require('region_sound_music_pointer_at_0x10',
            ('mov', 'qword ptr [rcx + 0x10], rdx') in instructions(0x253320, 0x10))
    require('data_handler_region_manager_at_0xdb0',
            ('mov', 'rcx, qword ptr [rax + 0xdb0]') in instructions(0x24F4CC, 0x18))
    require('location_music_pointer_at_0x58',
            instructions(0x299C80, 4) == [('mov', 'rax, qword ptr [rcx + 0x58]')])

    # These RVAs were independently located in the local executable. PDATA can
    # split one function into several ranges; its subrange starts are not the
    # Address Library function bases. Runtime also checks all six call sites.
    sites = ((46897, 0x854120, 0x20), (46870, 0x851A00, 0x1C1),
             (46870, 0x851A00, 0x22D), (18369, 0x29A9C0, 0x173),
             (51653, 0x937570, 0x29C), (46895, 0x853C70, 0x3B4))
    for index, (_, function, offset) in enumerate(sites):
        address = function+offset
        call = pe.get_data(address, 5)
        require(f'cme_entry_{index}_calls_default_object',
                call[0] == 0xE8 and address+5+struct.unpack('<i', call[1:])[0] == 0x154440)
    # TESFile adapter evidence. These addresses were located independently in
    # this executable. API forwarding through Address Library is not executed.
    # In particular, the pinned SDK omits SeekNextForm's third Boolean and
    # describes currentform without documenting its remapped header FormID.
    open_file = instructions(0x1CA460, 0x24)
    require('tesfile_open_uses_loaded_filename_and_path',
            ('lea', 'r8, [rcx + 0x58]') in open_file and
            ('lea', 'rdx, [rcx + 0x15c]') in open_file)
    next_form = instructions(0x1CD400, 0x1C5)
    require('tesfile_next_form_has_third_boolean', ('movzx', 'edi, r8b') in next_form)
    require('tesfile_skip_ignored_is_separate_from_subrecord_initialization',
            ('movzx', 'esi, dl') in next_form and
            ('test', 'dword ptr [rbx + 0x28c], 0x1000') in next_form)
    advance = instructions(0x1CD5F0, 0x186)
    require('tesfile_file_offset_at_0x2a8', ('add', 'dword ptr [rcx + 0x2a8], eax') in advance)
    require('tesfile_eof_compares_file_size_at_0x2a4', ('cmp', 'edx, dword ptr [rcx + 0x2a4]') in advance)
    require('tesfile_eof_clears_record_header', ('mov', 'qword ptr [rcx + 0x284], rax') in advance)
    require('tesfile_lazy_next_skips_chunk_initialization', ('test', 'sil, sil') in advance)
    header = instructions(0x1CF0F0, 0x1E1)
    require('tesfile_header_uses_master_pointer_table',
            ('mov', 'rdx, qword ptr [rbx + 0x468]') in header and
            ('cmp', 'eax, dword ptr [rbx + 0x460]') in header)
    require('tesfile_header_remaps_full_plugin_id',
            ('movzx', 'eax, byte ptr [rdi + 0x478]') in header and
            ('or', 'dword ptr [rbx + 0x290], eax') in header)
    require('tesfile_header_remaps_light_plugin_id',
            ('movzx', 'eax, word ptr [rdi + 0x47a]') in header and
            ('or', 'ecx, 0xfe000000') in header and ('and', 'ecx, 0xfff') in header)
    chunk_type = instructions(0x1CDC10, 0x2F)
    require('tesfile_current_subrecord_tag_at_0x29c', ('mov', 'eax, dword ptr [rbx + 0x29c]') in chunk_type)
    next_chunk = instructions(0x1CDC60, 0xA7)
    require('tesfile_subrecord_cursor_uses_six_byte_header',
            ('add', 'eax, 6') in next_chunk and ('add', 'dword ptr [rbx + 0x2ac], eax') in next_chunk)
    require('tesfile_compressed_subrecord_limit_at_0x4a8', ('mov', 'ecx, dword ptr [rbx + 0x4a8]') in next_chunk)
    chunk = instructions(0x1CF2E0, 0x1CD)
    require('tesfile_engine_expands_xxxx',
            ('cmp', 'ecx, 0x58585858') in chunk and
            ('mov', 'dword ptr [rbx + 0x2a0], eax') in chunk)
    read_data = instructions(0x1CDD10, 0x219)
    require('tesfile_read_data_uses_native_decompressed_buffer',
            ('mov', 'rax, qword ptr [rdi + 0x4a0]') in read_data and
            ('mov', 'r8d, dword ptr [rdi + 0x2a0]') in read_data)
    require('tesfile_read_data_counts_payload_bytes_at_0x2b0',
            ('mov', 'dword ptr [rdi + 0x2b0], eax') in read_data)
    close_file = instructions(0x1CAC40, 0x25D)
    require('tesfile_close_has_nonforcing_path', ('test', 'dl, dl') in close_file)
    return {'executable_version': '.'.join(map(str, version)),
            'executable_sha256': hashlib.sha256(raw).hexdigest(), 'checks': checks,
            'native_execution': False, 'address_library_resolution_executed': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = json.dumps(verify(args.exe), indent=2)+'\n'
    if args.output:
        args.output.write_text(report, encoding='utf-8')
    print(report)
