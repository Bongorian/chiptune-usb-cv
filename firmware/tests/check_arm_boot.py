#!/usr/bin/env python3
"""Execute the linked Cortex-M0+ code with SDK hardware boundaries stubbed.

This checks application startup ordering and the REAL SDK flash capacity guards.
It does not emulate USB enumeration, I2C hardware, ROM erase/program or audio.
Dependencies: unicorn==2.1.4, pyelftools==0.32.
"""
import argparse
import hashlib
import io
import json
import struct
import zlib
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_MCLASS, UC_MODE_THUMB, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2

FLASH = 0x10000000
CAPACITY = 2 * 1024 * 1024
BANK = 32768
STORE = FLASH + CAPACITY - 2 * BANK


class Image:
    def __init__(self, path):
        elf = ELFFile(io.BytesIO(path.read_bytes()))
        self.segments = [(s['p_vaddr'], s.data()) for s in elf.iter_segments()
                         if s['p_type'] == 'PT_LOAD']
        self.symbols = {s.name: (s['st_value'] & ~1, s['st_size'])
                        for s in elf.get_section_by_name('.symtab').iter_symbols()}

    def machine(self):
        uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        for start, size in [(0, 0x10000), (FLASH, CAPACITY), (0x20000000, 0x42000),
                            (0x40000000, 0x100000), (0x50000000, 0x100000),
                            (0xd0000000, 0x1000), (0xe0000000, 0x100000)]:
            uc.mem_map(start, size)
        uc.mem_write(FLASH, b'\xff' * CAPACITY)
        for address, data in self.segments:
            if data:
                uc.mem_write(address, data)
        uc.reg_write(UC_ARM_REG_SP, self.symbols['__StackTop'][0])
        uc.reg_write(UC_ARM_REG_LR, 0x201)
        return uc

    def hook(self, uc, name, fn):
        address = self.symbols[name][0]
        uc.hook_add(UC_HOOK_CODE, fn, begin=address, end=address)

    def flash_guard(self, name, offset, count):
        uc = self.machine()
        result = []

        def stop(label):
            def hook(uc, _address, _size, _data):
                result.append(label)
                uc.emu_stop()
            return hook

        # Reaching the first ROM lookup means the actual ARM bounds check passed.
        uc.mem_write(0x14, struct.pack('<HHH', 0x2000, 0, 0x101))
        uc.hook_add(UC_HOOK_CODE, stop('accepted'), begin=0x100, end=0x100)
        self.hook(uc, 'hard_assertion_failure', stop('rejected'))
        uc.reg_write(UC_ARM_REG_R0, offset)
        uc.reg_write(UC_ARM_REG_R1, count if name.endswith('erase') else 0x20010000)
        if name.endswith('program'):
            uc.reg_write(UC_ARM_REG_R2, count)
        uc.emu_start(self.symbols[name][0] | 1, 0, count=10000)
        assert len(result) == 1, f'{name}: no guard result'
        return result[0]

    def boot(self, storage, defaults=False, expected_sequence=0, expected_bank=-1):
        uc = self.machine()
        uc.mem_write(STORE, storage)
        uc.mem_write(0xd0000004, struct.pack('<I', 0xffffffff ^ ((1 << 21) if defaults else 0)))
        trace = []
        initialized = False

        def ret(uc, value=1):
            uc.reg_write(UC_ARM_REG_R0, value)
            uc.reg_write(UC_ARM_REG_R1, 0)
            uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

        # Skip ONLY platform setup/delays. All application storage, migration,
        # engine and UI code below executes from the actual linked ELF.
        def stub(uc, _address, _size, _data):
            ret(uc)

        # The SDK redirects memcpy/memset and single-precision arithmetic to
        # RP2040 ROM tables filled by CRT startup (we enter directly at main).
        def memory(copy):
            def hook(uc, _address, _size, _data):
                dest, source, count = (uc.reg_read(r) for r in
                                       (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2))
                data = bytes(uc.mem_read(source, count)) if copy else bytes([source & 255]) * count
                if count:
                    uc.mem_write(dest, data)
                ret(uc, dest)
            return hook

        self.hook(uc, '__wrap_memcpy', memory(True))
        self.hook(uc, '__wrap_memset', memory(False))

        def divide(signed):
            def hook(uc, _address, _size, _data):
                a, b = (uc.reg_read(r) for r in (UC_ARM_REG_R0, UC_ARM_REG_R1))
                if signed:
                    a, b = (x if x < 0x80000000 else x - 0x100000000 for x in (a, b))
                assert b != 0, 'Unexpected divide by zero during startup'
                quotient = abs(a) // abs(b)
                if (a < 0) != (b < 0):
                    quotient = -quotient
                ret(uc, quotient & 0xffffffff)
                uc.reg_write(UC_ARM_REG_R1, (a - quotient * b) & 0xffffffff)
            return hook

        # These wrappers use the SIO hardware divider rather than ROM.
        self.hook(uc, '__wrap___aeabi_uidivmod', divide(False))
        self.hook(uc, '__wrap___aeabi_idivmod', divide(True))

        def float_compare(operation):
            def hook(uc, _address, _size, _data):
                a, b = (struct.unpack('<f', struct.pack('<I', uc.reg_read(r)))[0]
                        for r in (UC_ARM_REG_R0, UC_ARM_REG_R1))
                ret(uc, int(operation(a, b)))
            return hook

        for suffix, operation in [('eq', lambda a, b: a == b), ('lt', lambda a, b: a < b),
                                   ('le', lambda a, b: a <= b), ('gt', lambda a, b: a > b),
                                   ('ge', lambda a, b: a >= b)]:
            self.hook(uc, '__wrap___aeabi_fcmp' + suffix, float_compare(operation))

        stubs = ('gpio_init', 'gpio_set_function', 'gpio_set_pulls',
                 'gpio_set_irq_enabled_with_callback', 'gpio_set_irq_enabled',
                 'check_sys_clock_khz', 'set_sys_clock_pll', 'sleep_ms',
                 'i2c_init', 'adc_init', 'uart_init', 'uart_set_format',
                 'uart_set_fifo_enabled', 'irq_set_exclusive_handler', 'irq_set_enabled',
                 'pio_claim_unused_sm', 'pio_add_program')
        for name in stubs:
            self.hook(uc, name, stub)

        def clock(uc, _address, _size, _data):
            ret(uc, 100000)

        self.hook(uc, 'time_us_64', clock)

        def usb_init(uc, _address, _size, _data):
            trace.append('usb_init')
            ret(uc)

        self.hook(uc, 'tusb_rhport_init', usb_init)

        def task(uc, _address, _size, _data):
            trace.append('usb_task')
            if initialized:
                uc.emu_stop()
            else:
                ret(uc)

        self.hook(uc, 'tud_task_ext', task)

        def record(name):
            def hook(_uc, _address, _size, _data):
                nonlocal initialized
                trace.append(name)
                if name == 'oled_init':
                    initialized = True
            return hook

        for name in ('store_restore', 'engine_init', 'ui_init', 'oled_init'):
            self.hook(uc, name, record(name))

        def forbidden(_uc, _address, _size, _data):
            raise AssertionError('Boot reached flash erase/program, panic or hard assertion')

        for name in ('flash_range_erase', 'flash_range_program', 'hard_assertion_failure', 'panic'):
            self.hook(uc, name, forbidden)
        try:
            uc.emu_start(self.symbols['main'][0] | 1, 0, count=20_000_000)
        except Exception as error:
            raise AssertionError(f'ARM boot PC={uc.reg_read(UC_ARM_REG_PC):#x} LR={uc.reg_read(UC_ARM_REG_LR):#x} trace={trace}') from error
        assert initialized and trace[-1] == 'usb_task', trace
        assert trace.index('usb_init') < trace.index('usb_task') < trace.index('store_restore'), trace
        assert bytes(uc.mem_read(STORE, 2 * BANK)) == storage, 'Boot modified flash'
        assert struct.unpack('<I', uc.mem_read(self.symbols['store_sequence'][0], 4))[0] == expected_sequence
        assert struct.unpack('<i', uc.mem_read(self.symbols['store_bank'][0], 4))[0] == expected_bank
        settings_address, settings_size = self.symbols['settings']
        return bytes(uc.mem_read(settings_address, settings_size)), trace


def verify(path):
    image = Image(path)
    guards = []
    for name in ('flash_range_erase', 'flash_range_program'):
        cases = [(CAPACITY - 2 * BANK, BANK), (CAPACITY - BANK, BANK)]
        if name.endswith('program'):
            cases += [(CAPACITY - 2 * BANK, 256), (CAPACITY - BANK, 256), (CAPACITY - 256, 256)]
        for offset, count in cases:
            assert image.flash_guard(name, offset, count) == 'accepted', (name, hex(offset))
            guards.append({'function': name, 'offset': hex(offset), 'bytes': count, 'accepted': True})
        assert image.flash_guard(name, CAPACITY, BANK) == 'rejected', name
    erased = b'\xff' * (2 * BANK)
    current, trace = image.boot(erased)
    header = struct.pack('<5I', 0x43545031, 6, 12, len(current), 0)
    record = bytearray(header + current)
    struct.pack_into('<I', record, 16, zlib.crc32(record[4:]))
    area = record + bytearray(erased[len(record):])
    image.boot(bytes(area), expected_sequence=12, expected_bank=0)
    area[100] ^= 0x40
    image.boot(bytes(area))
    v5 = (Path(__file__).parent / 'fixtures/settings-v5.bin').read_bytes()
    image.boot(v5 + erased[len(v5):], expected_sequence=93, expected_bank=0)
    image.boot(bytes(record) + erased[len(record):], defaults=True)
    return {'firmware': 'v0.8.1-alpha', 'elf_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'hardware_emulated': False, 'arm_code_executed': True,
            'flash_guard_cases': guards, 'out_of_capacity_rejected': True,
            'boot_cases': ['erased', 'valid v6', 'corrupt v6', 'legacy v5', 'OK held'],
            'boot_trace': trace, 'boot_flash_writes': 0,
            'limitations': ['SDK setup, USB task, SIO division, ROM memory and float comparisons are stubbed',
                            'no physical USB/I2C/flash/audio test']}


def compare_v08(path):
    old = Image(path)
    machine = old.machine()
    binary = (Path(__file__).parents[1] / 'dist/chiptune-v0.8.0-alpha.bin').read_bytes()
    result = {'published_bin_sha256': hashlib.sha256(binary).hexdigest(), 'cases': []}
    for name in ('flash_range_erase', 'flash_range_program'):
        address, size = old.symbols[name]
        body = bytes(machine.mem_read(address, size))
        assert binary.count(body) == 1, 'Negative control must match the published ARM function'
        for offset in (CAPACITY - 2 * BANK, CAPACITY - BANK):
            assert old.flash_guard(name, offset, BANK) == 'rejected'
            result['cases'].append({'function': name, 'offset': hex(offset), 'bytes': BANK,
                                    'result': 'rejected', 'matches_published_arm_code': True})
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('elf', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--v08-elf', type=Path, help='Optional v0.8.0 rebuild for a negative control')
    args = p.parse_args()
    result = verify(args.elf)
    if args.v08_elf:
        result['v08_negative_control'] = compare_v08(args.v08_elf)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
