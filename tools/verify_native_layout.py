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
