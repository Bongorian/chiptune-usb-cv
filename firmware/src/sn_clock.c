#include "sn_clock.h"
#include <string.h>
const uint32_t sn_clock_rate_millihz[SN_CLOCK_RATES] = {
    250u,   500u,   1000u,   2000u,   4000u,   8000u,   16000u,  32000u,
    50000u, 80000u, 100000u, 160000u, 250000u, 400000u, 600000u, 800000u};
const uint8_t sn_clock_sync_clocks[9] = {0, 1, 2, 3, 4, 6, 8, 12, 24};
/* Q32 cycles per 250us. No floating point, division or GPIO waits in the timer IRQ. */
static const uint32_t phase_step[SN_CLOCK_RATES] = {
    268435u,   536871u,   1073742u,   2147484u,   4294967u,   8589935u,   17179869u,  34359738u,
    53687091u, 85899346u, 107374182u, 171798692u, 268435456u, 429496730u, 644245094u, 858993459u};
/* Q16 half-periods = 16 * 2^(semitones/12), with fractional interpolation. */
static const uint32_t half_table[49] = {
    1048576u,  1110928u,  1176987u,  1246974u,  1321123u,  1399681u,  1482910u,
    1571089u,  1664511u,  1763488u,  1868350u,  1979448u,  2097152u,  2221855u,
    2353974u,  2493948u,  2642246u,  2799362u,  2965821u,  3142178u,  3329021u,
    3526975u,  3736700u,  3958896u,  4194304u,  4443710u,  4707947u,  4987896u,
    5284492u,  5598724u,  5931642u,  6284355u,  6658043u,  7053950u,  7473400u,
    7917792u,  8388608u,  8887421u,  9415894u,  9975792u,  10568984u, 11197448u,
    11863283u, 12568711u, 13316085u, 14107901u, 14946800u, 15835583u, 16777216u};
bool sn_clock_valid(const SnClockConfig *c) {
    return c->mode < SN_CLOCK_MODES && c->base <= SN_CLOCK_MAX_DROP &&
           c->depth <= SN_CLOCK_MAX_DROP && c->rate < SN_CLOCK_RATES && c->sync <= 8 &&
           c->retrigger <= 1;
}
static uint16_t random_next(SnClock *s) {
    uint32_t n = s->random;
    n ^= n << 13;
    n ^= n >> 17;
    n ^= n << 5;
    s->random = n;
    return (uint16_t)(n >> 16);
}
void sn_clock_trigger(SnClock *s) {
    s->phase = 0;
    s->sync_step = 0;
    s->sweep = UINT32_MAX;
    s->held = random_next(s);
}
void sn_clock_init(SnClock *s, const SnClockConfig *c) {
    memset(s, 0, sizeof(*s));
    s->random = 0x534e7648u;
    s->held = random_next(s);
    if (sn_clock_valid(c))
        s->config = *c;
}
void sn_clock_configure(SnClock *s, const SnClockConfig *c) {
    if (!sn_clock_valid(c))
        return;
    if (s->config.mode != c->mode || s->config.sync != c->sync) {
        s->phase = 0;
        s->sync_step = 0;
        s->sweep = 0;
        s->held = random_next(s);
    }
    s->config = *c;
}
void sn_clock_midi(SnClock *s, uint32_t clocks) {
    uint32_t delta = clocks - s->clocks;
    s->clocks = clocks;
    if (!s->config.sync || !delta)
        return;
    if (delta > 96) { /* MIDI Start resets the counter; also reject an implausible jump. */
        s->sync_step = 0;
        s->phase = 0;
        s->held = random_next(s);
        return;
    }
    unsigned cycle = sn_clock_sync_clocks[s->config.sync];
    unsigned step = s->sync_step + delta;
    if (step >= cycle)
        s->held = random_next(s);
    s->sync_step = (uint8_t)(step % cycle);
    s->phase = (uint32_t)(((uint64_t)s->sync_step << 32) / cycle);
}
uint16_t sn_clock_half(uint16_t drop_q8) {
    if (drop_q8 >= SN_CLOCK_MAX_DROP * 256)
        return SN_CLOCK_MAX_HALF;
    unsigned note = drop_q8 >> 8, fraction = drop_q8 & 255;
    uint32_t q16 = half_table[note] + ((half_table[note + 1] - half_table[note]) * fraction >> 8);
    return (uint16_t)((q16 + 32768) >> 16);
}
uint32_t sn_clock_hz(uint16_t half_cycles) {
    if (half_cycles < SN_CLOCK_MIN_HALF || half_cycles > SN_CLOCK_MAX_HALF)
        return 0;
    return SN_CLOCK_SYS_HZ / (2u * half_cycles);
}
uint16_t sn_clock_sample(const SnClock *s) {
    const SnClockConfig *c = &s->config;
    uint32_t modulation = 0, phase = s->phase >> 16;
    switch (c->mode) {
    case SN_CLOCK_OFF:
        return SN_CLOCK_MIN_HALF;
    case SN_CLOCK_SWEEP:
        modulation = s->sweep >> 16;
        break;
    case SN_CLOCK_DIVE:
        modulation = 65535 - (s->sweep >> 16);
        break;
    case SN_CLOCK_TRIANGLE:
        modulation = phase < 32768 ? phase * 2 : (65535 - phase) * 2;
        break;
    case SN_CLOCK_SQUARE:
        modulation = phase < 32768 ? 0 : 65535;
        break;
    case SN_CLOCK_RANDOM:
        modulation = s->held;
        break;
    case SN_CLOCK_SEQUENCE: {
        static const uint16_t sequence[8] = {0, 32768, 65535, 16384, 49152, 65535, 32768, 0};
        modulation = sequence[s->phase >> 29];
        break;
    }
    default:
        break;
    }
    uint32_t drop = (uint32_t)c->base * 256 + (c->depth * modulation >> 8);
    return sn_clock_half((uint16_t)drop);
}
uint16_t sn_clock_tick(SnClock *s) {
    if (s->config.mode == SN_CLOCK_SWEEP || s->config.mode == SN_CLOCK_DIVE) {
        uint32_t decrement = phase_step[s->config.rate];
        s->sweep = s->sweep > decrement ? (s->sweep - decrement) : 0;
    } else if (!s->config.sync) {
        uint32_t before = s->phase;
        s->phase += phase_step[s->config.rate];
        if (s->phase < before)
            s->held = random_next(s);
    }
    return sn_clock_sample(s);
}
unsigned sn_clock_write_us(const SnClockConfig *c) {
    uint16_t drop = c->mode == SN_CLOCK_OFF ? 0 : (uint16_t)(c->base * 256);
    if (c->mode >= SN_CLOCK_SWEEP)
        drop = (uint16_t)(drop + c->depth * 256);
    /* 32 clocks at the slowest setting, plus 8us for phase/old PIO cycle/timing margin. */
    return (sn_clock_half(drop) + 1u) / 2u + 8u;
}
