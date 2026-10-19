#!/usr/bin/env python3
"""Check the actual ELF/UF2, USB descriptors and reserved settings area without hardware."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

FLASH_BASE = 0x10000000
STORE_START = FLASH_BASE + 2 * 1024 * 1024 - 65536
FAMILY = 0xE48BFF56


def verify(build: Path, nm: str):
    elf = (build / 'chiptune.elf').read_bytes()
    binary = (build / 'chiptune.bin').read_bytes()
    uf2 = (build / 'chiptune.uf2').read_bytes()
    assert elf[:7] == b'\x7fELF\x01\x01\x01', 'Expected a little-endian ELF32'
    assert struct.unpack_from('<H', elf, 18)[0] == 40, 'Expected ARM'
    assert len(uf2) % 512 == 0 and uf2, 'Malformed UF2'
    count = len(uf2) // 512
    numbers = set()
    addresses = set()
    for offset in range(0, len(uf2), 512):
        block = uf2[offset:offset + 512]
        m0, m1, flags, addr, size, number, total, family = struct.unpack_from('<8I', block)
        assert (m0, m1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack_from('<I', block, 508)[0] == 0x0AB16F30
        assert flags == 0x2000 and family == FAMILY, 'Expected RP2040 flash UF2'
        assert size == 256 and total == count and number < total
        assert FLASH_BASE <= addr and addr + size <= STORE_START, 'UF2 overlaps settings'
        assert addr % 256 == 0 and addr not in addresses and number not in numbers
        numbers.add(number)
        addresses.add(addr)
        start = addr - FLASH_BASE
        expected = binary[start:start + size]
        assert block[32:32 + len(expected)] == expected, 'BIN/UF2 mismatch'
        assert start < len(binary), 'UF2 has extra payload beyond the application'
    assert numbers == set(range(count))
    assert addresses == set(range(FLASH_BASE, FLASH_BASE + len(binary), 256))
    nm_output = subprocess.check_output([nm, '-S', str(build / 'chiptune.elf')], text=True)
    symbols = {}
    for line in nm_output.splitlines():
        fields = line.split()
        if len(fields) in (3, 4):
            try:
                symbols[fields[-1]] = (int(fields[0], 16), int(fields[1], 16) if len(fields) == 4 else 0)
            except ValueError:
                pass
    flash_end = symbols['__flash_binary_end'][0]
    assert flash_end <= STORE_START and flash_end - FLASH_BASE == len(binary)
    def symbol(name):
        addr, size = symbols[name]
        assert size > 0 and FLASH_BASE <= addr < flash_end
        return binary[addr-FLASH_BASE:addr-FLASH_BASE+size]
    config = symbol('config')
    assert config[:2] == bytes([9, 2]) and struct.unpack_from('<H', config, 2)[0] == len(config)
    assert config[4] == 2 and config[7] == 0x80 and config[8] == 250, 'USB 500mA config required'
    interfaces, endpoints = [], []
    pos = 0
    while pos < len(config):
        length, kind = config[pos:pos+2]
        assert length >= 2 and pos + length <= len(config)
        desc = config[pos:pos+length]
        if kind == 4:
            interfaces.append((desc[5], desc[6]))
        if kind == 5:
            assert desc[3] == 2 and struct.unpack_from('<H', desc, 4)[0] == 64
            endpoints.append(desc[2])
        pos += length
    assert interfaces == [(1, 1), (1, 3)] and endpoints == [1, 0x81], 'Expected USB MIDI 1.0'
    device = symbol('device')
    assert len(device) == 18 and device[0:2] == bytes([18, 1]) and device[7] == 64
    assert struct.unpack_from('<H', device, 2)[0] == 0x0200
    assert struct.unpack_from('<H', device, 12)[0] == 0x0080, 'Expected firmware USB revision 0.8'
    bss = symbols['__bss_end__'][0] - symbols['__bss_start__'][0]
    assert bss < 200 * 1024, 'Insufficient RP2040 RAM margin'
    stack = symbols['__StackTop'][0] - symbols['__StackBottom'][0]
    assert stack == 4096, 'Expected the dedicated 4KiB core-0 scratch stack'
    return {
        'firmware': 'v0.8.0-alpha', 'target': 'RP2040 dedicated Chiptune USB/CV board',
        'hardware_tested': False, 'checks_passed': True,
        'application_bytes': len(binary), 'bss_bytes': bss, 'stack_bytes': stack, 'uf2_bytes': len(uf2),
        'uf2_blocks': count, 'uf2_family_id': hex(FAMILY),
        'application_end': hex(flash_end), 'settings_start': hex(STORE_START),
        'settings_reserved_bytes': 65536, 'settings_region_untouched': True,
        'settings_region_untouched_by_uf2': True,
        'legacy_boot_behavior': {'layout_revision': 2, 'musical_settings': 'reset and commit once',
                                 'cv_calibration': 'retain', 'brightness': 'retain'},
        'usb': {'vid': hex(struct.unpack_from('<H', device, 8)[0]),
                'pid': hex(struct.unpack_from('<H', device, 10)[0]),
                'power_ma': config[8] * 2, 'midi_version': '1.0',
                'interfaces': 2, 'bulk_endpoints': endpoints},
        'sha256': {name: hashlib.sha256((build / name).read_bytes()).hexdigest()
                   for name in ('chiptune.bin', 'chiptune.uf2')},
        'untested': ['physical USB enumeration/resume/current', 'sound IC output and timing',
                     'CV accuracy/gate latency', 'OLED module display', 'flash retention on hardware',
                     'first-boot layout commit and calibration retention on hardware']
    }


def main():
    p = argparse.ArgumentParser()
    p.add_argument('build', type=Path)
    p.add_argument('--nm', default='arm-none-eabi-nm')
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    result = verify(args.build, args.nm)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
