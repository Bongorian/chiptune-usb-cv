#!/usr/bin/env python3
"""Execute release ARM synthesis; count instructions/ROM calls, not hardware cycles."""
import argparse
import hashlib
import json
from pathlib import Path
from check_arm_boot import Image, BANK
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP, UC_ARM_REG_LR

def verify(elf, project, settings):
    image = Image(elf)
    image.boot(b'\xff' * (2 * BANK))
    uc = image.last_machine
    assert len(project.read_bytes()) >= 2116
    assert len(settings.read_bytes()) == image.symbols['settings'][1]
    uc.mem_write(image.symbols['settings'][0], settings.read_bytes())
    scratch = 0x20030000
    assert image.symbols['__bss_end__'][0] < scratch
    uc.mem_write(scratch, project.read_bytes())
    engine = image.symbols['engine'][0]
    returned = False
    def stop(m, address, size, data):
        nonlocal returned
        returned = True
        m.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop, begin=0x200, end=0x200)
    def call(name, *args):
        nonlocal returned
        returned = False
        for reg, value in zip((UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3), args):
            uc.reg_write(reg, value)
        uc.reg_write(UC_ARM_REG_SP, image.symbols['__StackTop'][0])
        uc.reg_write(UC_ARM_REG_LR, 0x201)
        uc.emu_start(image.symbols[name][0] | 1, 0, count=10_000_000)
        assert returned, f'{name} did not return'
        return uc.reg_read(UC_ARM_REG_R0)
    assert call('engine_load_project', engine, scratch) == 1
    for track in range(14):
        uc.mem_write(scratch, bytes((0x90 | track, 48 + track, 127)))
        call('engine_message', engine, 0, scratch, 3)
    assert call('engine_active', engine) == 14
    frames = []
    current = {'instructions': 0, 'min_sp': image.symbols['__StackTop'][0]}
    def count(m, address, size, data):
        current['instructions'] += 1
        current['min_sp'] = min(current['min_sp'], m.reg_read(UC_ARM_REG_SP))
    handle = uc.hook_add(UC_HOOK_CODE, count)
    for delta in [1, 1, 10, 100, 1000, 1]:
        current['instructions'] = 0
        current['min_sp'] = image.symbols['__StackTop'][0]
        image.runtime_counts.clear()
        call('engine_tick', engine, delta)
        frames.append({'elapsed_ms': delta, 'application_and_sdk_instructions': current['instructions'],
                       'stack_bytes_observed': image.symbols['__StackTop'][0] - current['min_sp'],
                       'stubbed_runtime_calls': dict(image.runtime_counts)})
    operations=[]
    for name,args in [('settings_valid',(image.symbols['settings'][0],)),
                      ('ui_buttons',(image.symbols['ui'][0],scratch,2000)),
                      ('ui_buttons',(image.symbols['ui'][0],scratch,2025)),
                      ('ui_buttons',(image.symbols['ui'][0],scratch,2725)),
                      ('ui_render',(image.symbols['ui'][0],scratch,2000))]:
        uc.mem_write(scratch,b'\0'*64)
        if name=='ui_buttons':uc.mem_write(scratch+1,b'\1')
        current['instructions']=0
        current['min_sp']=image.symbols['__StackTop'][0]
        image.runtime_counts.clear()
        result=call(name,*args)
        if name=='settings_valid':assert result==1
        operations.append({'function':name,'instructions':current['instructions'],
                           'stack_bytes_observed':image.symbols['__StackTop'][0]-current['min_sp']})
    uc.hook_del(handle)
    assert all(o['stack_bytes_observed']<4096 for o in operations)
    assert all(f['stack_bytes_observed'] < 4096 for f in frames)
    assert call('engine_active', engine) == 14
    return {'firmware': 'v0.11.0-alpha', 'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
            'arm_release_code_executed': True, 'logical_voices': 14, 'physical_circuits': 14,
            'vibrato_tremolo_pitch_envelope_glide_morph_enabled': True,
            'frames': frames, 'other_operations':operations, 'rp2040_wcet_certified': False,
            'limitations': ['Instruction counts exclude ROM arithmetic/memory and SIO divider implementation.',
                            'No XIP/ROM/peripheral/interrupt latency or physical audio is emulated.',
                            'Measure tick_max_us and overruns on hardware; instruction count is not a cycle estimate.']}

if __name__ == '__main__':
    p=argparse.ArgumentParser()
    p.add_argument('elf', type=Path)
    p.add_argument('project', type=Path)
    p.add_argument('settings', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a=p.parse_args()
    result=verify(a.elf,a.project,a.settings)
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
