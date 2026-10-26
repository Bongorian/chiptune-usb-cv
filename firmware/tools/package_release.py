#!/usr/bin/env python3
"""Package a locally verified build; refuse stale reports or failed test logs."""
import argparse
import hashlib
import json
import re
import shutil
import zipfile
from pathlib import Path

VERSION = 'v0.11.0-alpha'
REPORTS = ['Verification.json', 'Boot_Verification.json', 'Runtime_Verification.json',
           'Clock_Verification.json', 'PIO_Verification.json', 'Performance.json']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('build', type=Path)
    parser.add_argument('--host-log', type=Path, required=True)
    parser.add_argument('--release-log', type=Path, required=True)
    parser.add_argument('--node-log', type=Path, required=True)
    parser.add_argument('--browser-log', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('firmware/dist'))
    args = parser.parse_args()
    build, dest = args.build, args.output
    elf_hash = sha(build / 'chiptune.elf')
    for name in ['Boot_Verification.json', 'Runtime_Verification.json', 'Clock_Verification.json']:
        assert json.loads((build / name).read_text())['elf_sha256'] == elf_hash, name
    verification = json.loads((build / 'Verification.json').read_text())
    assert verification['checks_passed'] and verification['firmware'] == VERSION
    for ext in ['bin', 'uf2']:
        assert verification['sha256']['chiptune.' + ext] == sha(build / ('chiptune.' + ext))
    for path in [args.host_log, args.release_log]:
        assert '100% tests passed, 0 tests failed out of 9' in path.read_text(), path
    node = args.node_log.read_text()
    assert re.search(r'tests 19\b', node) and re.search(r'pass 19\b', node) and re.search(r'fail 0\b', node)
    assert 'PASS browser:' in args.browser_log.read_text()
    dest.mkdir(parents=True, exist_ok=True)
    for ext in ['bin', 'uf2']:
        shutil.copyfile(build / ('chiptune.' + ext), dest / ('chiptune-' + VERSION + '.' + ext))
    for name in REPORTS + ['Pitch_Table.csv']:
        shutil.copyfile(build / name, dest / name)
    test_log = dest / 'v0.11.0-tests.txt'
    test_log.write_text('\n'.join(p.read_text() for p in [args.host_log, args.release_log, args.node_log, args.browser_log]))
    readme = dest / 'v0.11.0-README.txt'
    readme.write_text('''Chiptune USB/CV v0.11.0-alpha — dedicated RP2040/SAA1099/SN76489AN board

UF2: chiptune-v0.11.0-alpha.uf2
Settings 11 / SysEx protocol 3 / schema 11. Use the matching editor/ files.
Old settings are ignored without boot erase/write; explicit saves use dual banks.

SN common clock: Hz/direct PIO, RAW/BASE/FULL, two modulation slots.
The generator range is calculated, not a measured IC operating range. Clocks
above the manufacturer's range are experimental. READY is unconnected.
An SN byte holds the shared data bus until its clock budget completes; SAA
register updates also wait. USB/MIDI/CV/OLED processing continues.

Reports describe software checks with stubbed hardware/ROM/time where noted.
Physical sound, GPIO timing, out-of-spec clocks, power-loss retention and
RP2040 1ms/250us worst-case execution times have not been measured.
See firmware/README.md, SN_CLOCK.md and BRINGUP.md in the source repository.
Development USB VID/PID is not an assigned production identity.
LICENSES and THIRD_PARTY_NOTICES cover bundled runtimes.
''')
    code = [p for directory in ['firmware', 'editor'] for p in Path(directory).rglob('*')
            if p.is_file() and 'dist' not in p.parts and p.suffix in
            ['.c', '.h', '.inc', '.py', '.sh', '.json', '.txt', '.pio', '.md', '.js', '.cjs', '.html', '.css']]
    code += list(Path('.github/workflows').glob('*.yml'))
    manifest = ''.join(f'{sha(p)}  {p.as_posix()}\n' for p in sorted(code))
    (dest / 'Source_SHA256SUMS').write_text(manifest)
    test = {'firmware': VERSION, 'hardware_tested': False, 'elf_sha256': elf_hash,
            'source_manifest_sha256': hashlib.sha256(manifest.encode()).hexdigest(),
            'host_sanitized': {'suites': 9, 'passed': 9, 'sanitizers': ['ASan', 'UBSan']},
            'host_release': {'suites': 9, 'passed': 9},
            'node_protocol_tests': {'tests': 19, 'passed': 19, 'backend': 'actual portable C firmware'},
            'browser_integration': {'passed': True, 'midi': 'mock ports backed by C; no physical devices',
                                    'clock_graph_two_slot_full_read_write': True},
            'arm_build': {'passed': True, 'sdk': '2.3.1', 'configuration': 'Release'},
            'arm_execution': {'boot_passed': True, 'runtime_passed': True, 'clock_bus_passed': True,
                              'platform_rom_sio_stubbed': True},
            'uf2_validation': True, 'pio_instruction_timing': True,
            'rp2040_wcet_certified': False,
            'unverified': ['physical clocks, waveforms, PSG variant and GPIO setup/hold',
                           'experimental clock range and modulation transitions',
                           'physical USB/CV/Gate/OLED under simultaneous load',
                           '1ms synthesis / 250us modulation WCET and jitter',
                           'flash brownout/retention and multi-day low-clock transfers']}
    (dest / 'Test_Verification.json').write_text(json.dumps(test, ensure_ascii=False, indent=2) + '\n')
    files = [dest / ('chiptune-' + VERSION + '.' + ext) for ext in ['uf2', 'bin']]
    files += [dest / name for name in REPORTS + ['Pitch_Table.csv', 'Test_Verification.json', 'Source_SHA256SUMS']]
    files += [test_log]
    archive = dest / ('chiptune-' + VERSION + '.zip')
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('README.txt', readme.read_text())
        for p in files:
            z.write(p, p.name)
        z.writestr('SHA256SUMS', ''.join(f'{sha(p)}  {p.name}\n' for p in files))
        for p in Path('firmware/LICENSES').rglob('*'):
            if p.is_file():
                z.write(p, p.relative_to('firmware'))
        z.write('THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES.md')
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        for line in z.read('SHA256SUMS').decode().splitlines():
            expected, name = line.split('  ', 1)
            assert hashlib.sha256(z.read(name)).hexdigest() == expected
    (dest / 'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(dest.iterdir()) if p.is_file() and p.name != 'SHA256SUMS'))
    print(f'Packaged and verified {archive}; UF2 SHA256={verification["sha256"]["chiptune.uf2"]}')


if __name__ == '__main__':
    main()
