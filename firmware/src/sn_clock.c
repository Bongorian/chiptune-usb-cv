#include "sn_clock.h"
#include "sound_math.h"
#include <math.h>
#include <string.h>
const uint8_t sn_clock_sync_clocks[9] = {0, 1, 2, 3, 4, 6, 8, 12, 24};
double sn_clock_max_hz(void) { return (double)SN_CLOCK_SYS_HZ / 10; }
double sn_clock_min_hz(void) {
    return (double)SN_CLOCK_SYS_HZ / (2.0 * ((double)UINT32_MAX + 5) * 65536);
}
SnClockPlan sn_clock_direct(uint32_t period, uint32_t div256) {
    SnClockPlan p = {.period = period, .div256 = div256};
    p.available = div256 >= SN_CLOCK_DIV_MIN && div256 <= SN_CLOCK_DIV_MAX;
    if (p.available) {
        p.hz = (double)SN_CLOCK_SYS_HZ * 256 / (2.0 * ((double)period + 5) * div256);
        /* TI SN76489AN specified input clock: DC to 4MHz.
           Fractional PIO divisors can alternate pulse lengths. */
        p.experimental = p.hz > 4000000;
    }
    return p;
}
SnClockPlan sn_clock_plan(double hz) {
    if (!isfinite(hz) || hz < sn_clock_min_hz() || hz > sn_clock_max_hz())
        return (SnClockPlan){0};
    /* Keep the loop short to make changes responsive. Increase X only once
       the SM divider runs out of range. Both backends use the same equation. */
    double half = (double)SN_CLOCK_SYS_HZ / (2 * hz);
    double cycles = ceil(half / 65536);
    if (cycles < 5)
        cycles = 5;
    uint32_t x = cycles - 5 >= UINT32_MAX ? UINT32_MAX : (uint32_t)(cycles - 5);
    double d = half * 256 / ((double)x + 5);
    uint32_t div = (uint32_t)floor(d + .5);
    if (div < SN_CLOCK_DIV_MIN)
        div = SN_CLOCK_DIV_MIN;
    if (div > SN_CLOCK_DIV_MAX)
        div = SN_CLOCK_DIV_MAX;
    SnClockPlan best = sn_clock_direct(x, div);
    double integer = floor(half + .5);
    if (integer >= 5 && integer <= (double)UINT32_MAX + 5) {
        SnClockPlan alt = sn_clock_direct((uint32_t)(integer - 5), SN_CLOCK_DIV_MIN);
        if (fabs(alt.hz - hz) < fabs(best.hz - hz))
            best = alt;
    }
    return best;
}
SnClockPlan sn_clock_base(const SnClockConfig *c) {
    return c->direct ? sn_clock_direct(c->period, c->div256) : sn_clock_plan(c->base_hz);
}
void sn_clock_default(SnClockConfig *c) {
    memset(c, 0, sizeof(*c));
    c->base_hz = 2000000;
    c->div256 = SN_CLOCK_DIV_MIN;
    c->period = 27;
    c->follow = SN_FOLLOW_BASE;
    for (unsigned i = 0; i < SN_CLOCK_SLOTS; i++) {
        c->mod[i].depth = 2400;
        c->mod[i].rate_millihz = 4000;
        c->mod[i].retrigger = 1;
    }
}
bool sn_clock_valid(const SnClockConfig *c) {
    if (c->direct > 1 || c->stopped > 1 || c->follow > SN_FOLLOW_FULL ||
        !sn_clock_base(c).available)
        return false;
    for (unsigned i = 0; i < SN_CLOCK_SLOTS; i++) {
        const SnModConfig *m = &c->mod[i];
        if (m->mode >= SN_CLOCK_MODES || m->domain > SN_DOMAIN_PERIOD || m->sync > 8 ||
            m->retrigger > 1 || m->curve > 2)
            return false;
    }
    return true;
}
static int32_t random_next(SnMod *m) {
    uint32_t n = m->random;
    n ^= n << 13;
    n ^= n >> 17;
    n ^= n << 5;
    m->random = n;
    return (int32_t)(n >> 16) - 32768;
}
void sn_clock_init(SnClock *s, const SnClockConfig *c) {
    memset(s, 0, sizeof(*s));
    sn_clock_default(&s->config);
    s->base_plan = sn_clock_base(&s->config);
    for (unsigned i = 0; i < 2; i++) {
        s->mod[i].random = 0x534e7648u + i * 997;
        s->mod[i].held = random_next(&s->mod[i]);
    }
    s->midi_interval_us = 20833;
    const uint8_t seq[] = {0, 50, 100, 25, 75, 100, 50, 0};
    sn_clock_sequence(s, seq);
    sn_clock_configure(s, c);
}
void sn_clock_configure(SnClock *s, const SnClockConfig *c) {
    if (!sn_clock_valid(c))
        return;
    for (unsigned i = 0; i < 2; i++)
        if (c->mod[i].mode != s->config.mod[i].mode || c->mod[i].sync != s->config.mod[i].sync)
            s->mod[i].phase = s->mod[i].sweep = s->mod[i].sync_bias = 0;
    s->config = *c;
    s->base_plan = sn_clock_base(c);
}
static uint32_t sync_phase(const SnClock *s, unsigned n, uint32_t now) {
    uint32_t elapsed = now - s->last_midi_us, interval = s->midi_interval_us;
    uint64_t whole = ((uint64_t)s->clocks + elapsed / interval) % n;
    uint64_t fraction = ((uint64_t)(elapsed % interval) << 32) / interval;
    return (uint32_t)(((whole << 32) + fraction) / n);
}
void sn_clock_trigger(SnClock *s, bool manual, uint32_t now) {
    for (unsigned i = 0; i < 2; i++)
        if (manual || s->config.mod[i].retrigger) {
            s->mod[i].phase = s->mod[i].sweep = s->mod[i].sync_bias = 0;
            unsigned sync = s->config.mod[i].sync;
            s->mod[i].sync_bias =
                sync && s->midi_seen ? sync_phase(s, sn_clock_sync_clocks[sync], now) : 0;
            s->mod[i].held = random_next(&s->mod[i]);
        }
}
void sn_clock_midi(SnClock *s, uint32_t clocks, uint32_t now) {
    uint32_t delta = clocks - s->clocks;
    if (!delta)
        return;
    if (s->midi_seen && delta <= 96) {
        uint32_t interval = (now - s->last_midi_us) / delta;
        if (interval)
            s->midi_interval_us = interval;
    }
    s->last_midi_us = now;
    s->clocks = clocks;
    s->midi_seen = true;
}
static int32_t wave(const SnClock *s, unsigned i) {
    const SnModConfig *c = &s->config.mod[i];
    const SnMod *m = &s->mod[i];
    uint32_t p = m->phase >> 16;
    int32_t tri = p < 32768 ? (int32_t)p * 2 - 32768 : 98303 - (int32_t)p * 2;
    uint32_t env = m->sweep >> 16;
    switch (c->mode) {
    case SN_CLOCK_SWEEP:
        return (int32_t)env - 32768;
    case SN_CLOCK_DIVE:
        return 32767 - (int32_t)env;
    case SN_CLOCK_TRIANGLE:
        return tri;
    case SN_CLOCK_SQUARE:
        return p < 32768 ? -32768 : 32767;
    case SN_CLOCK_RANDOM:
        return m->held;
    case SN_CLOCK_SEQUENCE:
        return (int32_t)s->sequence[m->phase >> 29] - 32768;
    case SN_CLOCK_SAW:
        return (int32_t)p - 32768;
    case SN_CLOCK_SAW_DOWN:
        return 32767 - (int32_t)p;
    case SN_CLOCK_WALK:
        return m->walk;
    case SN_CLOCK_SLEW:
        return m->smooth;
    case SN_CLOCK_CURVE: {
        double v = (double)env / 65535;
        v = c->curve == 1 ? v * v : c->curve == 2 ? v * v * (3 - 2 * v) : v;
        return (int32_t)(v * 65535) - 32768;
    }
    case SN_CLOCK_RATCHET:
        return ((p * 8) & 65535) < 8192 ? 32767 : -32768;
    case SN_CLOCK_BURST:
        return env >= 65535 ? 0 : (((p * 8) & 65535) < 8192 ? 32767 : -32768);
    case SN_CLOCK_WOW:
        return (int32_t)(fast_sine(m->phase) * 26214 + fast_sine(m->phase * 7) * 6553);
    case SN_CLOCK_FLUTTER:
        return (int32_t)(fast_sine(m->phase) * 19660 + fast_sine(m->phase * 3) * 13107);
    case SN_CLOCK_SINE:
    case SN_CLOCK_AUDIO:
        return (int32_t)(fast_sine(m->phase) * 32767);
    default:
        return 0;
    }
}
SnClockPlan sn_clock_sample(const SnClock *s) {
    SnClockPlan b = s->base_plan;
    double hz = b.hz;
    bool modulated = false;
    for (unsigned i = 0; i < 2; i++) {
        const SnModConfig *c = &s->config.mod[i];
        if (c->mode == SN_CLOCK_OFF)
            continue;
        double v = c->offset + (double)c->depth * wave(s, i) / 32768;
        modulated |= v != 0;
        if (c->domain == SN_DOMAIN_MUSIC) {
            double oct = floor(v / 1200);
            if (oct > 1023 || oct < -1023)
                hz = oct > 0 ? INFINITY : 0;
            else
                hz *= ldexp((double)fast_ratio((float)((v - oct * 1200) / 100)), (int)oct);
        } else if (c->domain == SN_DOMAIN_HZ)
            hz += v;
        else if (c->domain == SN_DOMAIN_RATIO)
            hz *= 1 + v / 1000000;
        else {
            double half = (double)SN_CLOCK_SYS_HZ / (2 * hz) + v * (double)b.div256 / 256;
            hz = half > 0 ? SN_CLOCK_SYS_HZ / (2 * half) : NAN;
        }
    }
    return modulated ? sn_clock_plan(hz) : b;
}
SnClockPlan sn_clock_tick(SnClock *s, uint32_t dt, uint32_t now) {
    for (unsigned i = 0; i < 2; i++) {
        SnMod *m = &s->mod[i];
        const SnModConfig *c = &s->config.mod[i];
        uint32_t before = m->phase;
        uint64_t product = (uint64_t)c->rate_millihz * dt;
        uint64_t step = ((product % 1000000000u) << 32) / 1000000000u;
        if (product >= 1000000000u)
            step |= 1ULL << 32;
        if (c->sync && s->midi_seen) {
            unsigned n = sn_clock_sync_clocks[c->sync];
            /* Predict between arriving MIDI clocks. Keep free-running after dropout;
               the next clock reanchors the phase, rather than freezing at 24PPQN. */
            m->phase = sync_phase(s, n, now) - m->sync_bias;
            step = ((uint64_t)dt << 32) / ((uint64_t)s->midi_interval_us * n);
        } else
            m->phase += (uint32_t)step;
        uint64_t sweep = (uint64_t)m->sweep + step;
        m->sweep = sweep > UINT32_MAX ? UINT32_MAX : (uint32_t)sweep;
        if (m->phase < before || (step >> 32)) {
            m->held = random_next(m);
            int32_t next = m->walk + m->held / 4;
            m->walk = next < -32768 ? -32768 : next > 32767 ? 32767 : next;
        }
        /* Slew over one nominal cycle, independent of callback jitter. */
        uint64_t fraction = step > UINT32_MAX ? UINT32_MAX : step;
        m->smooth +=
            (int32_t)(((int64_t)(m->held - m->smooth) * (int64_t)fraction) / 4294967296LL);
    }
    return sn_clock_sample(s);
}
void sn_clock_sequence(SnClock *s, const uint8_t percent[8]) {
    for (unsigned i = 0; i < 8; i++)
        s->sequence[i] = (uint16_t)((percent[i] > 100 ? 100 : percent[i]) * 65535u / 100);
}
void sn_counter_init(SnClockCounter *c, SnClockPlan p, uint64_t now) {
    memset(c, 0, sizeof(*c));
    c->plan = p;
    c->epoch_us = now;
    c->stopped = !p.available;
}
void sn_counter_advance(SnClockCounter *c, uint64_t now) {
    if (c->stopped || !c->plan.available || now < c->epoch_us)
        return;
    double clocks = (double)(now - c->epoch_us) * c->plan.hz / 1000000;
    uint64_t done = clocks >= 2 ? (uint64_t)floor(clocks) - 1 : 0;
    uint64_t total = c->epoch_cycles + done;
    if (total > c->cycles)
        c->cycles = total;   /* callback timestamps must never rewind credit */
    if (clocks >= 1000000) { /* bound rounding at very long uptimes */
        c->epoch_us = now;
        c->epoch_cycles = c->cycles;
    }
}
void sn_counter_change(SnClockCounter *c, SnClockPlan p, uint64_t now, bool stopped) {
    sn_counter_advance(c, now);
    c->epoch_cycles = c->cycles;
    c->epoch_us = now;
    c->plan = p;
    c->stopped = stopped;
    c->restarts++;
}
