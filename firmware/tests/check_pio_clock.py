#!/usr/bin/env python3
"""Execute pioasm's machine words to verify pulse lengths and empty-FIFO behavior.
This checks PIO instruction timing, not physical GPIO or IC behavior.
"""
import argparse
import json
import re
from collections import deque
from pathlib import Path


def trace(words, count, end, update=None):
    x = y = osr = pc = cycles = pin = 0
    fifo = deque([count])
    edges = []
    sent = update is None
    while cycles < end:
        if not sent and cycles >= update[0]:
            fifo.append(update[1])
            sent = True
        w = words[pc]
        op, delay, target = w >> 13, (w >> 8) & 31, pc + 1
        if op == 4:
            assert w & 0xff == 0x80, 'Expected unconditional PULL NOBLOCK'
            osr = fifo.popleft() if fifo else x
        elif op == 5:
            assert (w >> 3) & 3 == 0, 'Only unmodified MOV supported'
            value = {1: x, 2: y, 7: osr}[w & 7]
            dest = (w >> 5) & 7
            if dest == 1:
                x = value
            elif dest == 2:
                y = value
            else:
                raise AssertionError('Unexpected MOV destination')
        elif op == 7:
            assert (w >> 5) & 7 == 0, 'Expected SET PINS'
            value = w & 31
            assert value in (0, 1)
            if value != pin:
                edges.append((cycles, value))
                pin = value
        elif op == 0:
            assert (w >> 5) & 7 == 4, 'Expected JMP Y--'
            before = y
            y = (y - 1) & 0xffffffff
            if before:
                target = w & 31
        else:
            raise AssertionError(f'Unexpected instruction {w:04x}')
        cycles += delay + 1
        pc = 0 if target == len(words) else target
    return edges


def verify(header):
    source = header.read_text()
    array = re.search(r'sn_clock_program_instructions\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    words = [int(h, 16) for h in re.findall(r'^\s*(0x[0-9a-f]+),', array, re.M)]
    assert len(words) == 9
    assert '#define sn_clock_wrap_target 0' in source
    assert '#define sn_clock_wrap 8' in source
    static_count = transitions = 0
    for half in range(16, 257):
        edges = trace(words, half - 5, half * 20)
        spans = [b[0] - a[0] for a, b in zip(edges, edges[1:])]
        assert len(spans) >= 12 and all(n == half for n in spans), (half, spans)
        static_count += 1
    for old, new in [(16, 256), (256, 16), (32, 64), (64, 32)]:
        for when in range(2 * old + 5):
            edges = trace(words, old - 5, 12 * max(old, new), (when, new - 5))
            spans = [b[0] - a[0] for a, b in zip(edges, edges[1:])]
            assert all(n in (old, new) for n in spans), (old, new, when, spans)
            assert spans[-4:] == [new] * 4, (old, new, when, spans)
            transitions += 1
    return {'assembled_pio_verified': True, 'static_periods': static_count,
            'transition_phases': transitions, 'duty_percent': 50,
            'clock_hz_range': [250000, 4000000], 'physical_tested': False}


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('header', type=Path)
    args = p.parse_args()
    print(json.dumps(verify(args.header), indent=2))
