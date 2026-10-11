#include "engine.h"
#include <math.h>
#include <string.h>
#include <limits.h>
const Patch patches[PATCHES] = {{"CHIP LEAD", 3, 70, 90, 85, 0, 0, 0, 0, 10, 0},
                                {"PLUCK", 1, 230, 120, 0, 0, 0, 0, 0, 0, 0},
                                {"WARM BASS", 3, 100, 65, 70, 0, 0, 0, 0, 0, 0},
                                {"HW BELL", 1, 450, 180, 0, 0, 0, 2, 0, 0, 0},
                                {"HW TRIANGLE", 8, 150, 160, 85, 0, 0, 5, 0, 0, 1},
                                {"STEREO PAD", 450, 300, 700, 65, 0, 0, 0, 0, 15, 1},
                                {"TONE NOISE", 2, 90, 110, 60, 2, 1, 0, 0, 0, 0},
                                {"WHITE SNARE", 1, 130, 60, 0, 1, 1, 0, 0, 0, 0},
                                {"HI HAT", 1, 45, 20, 0, 1, 0, 0, 0, 0, 0},
                                {"PERIODIC", 2, 80, 70, 80, 1, 3, 0, 0, 0, 0},
                                {"KICK", 1, 160, 40, 0, 0, 0, 0, 24, 0, 0},
                                {"LASER", 1, 260, 80, 0, 0, 0, 0, 36, 0, 1},
                                {"CHORUS", 8, 120, 200, 80, 0, 0, 0, 0, 20, 1},
                                {"SLOW ATTACK", 800, 250, 650, 85, 0, 0, 0, 0, 6, 0},
                                {"NOISE PAD", 180, 200, 420, 65, 1, 2, 0, 0, 0, 1},
                                {"DRY SQUARE", 1, 1, 12, 100, 0, 0, 0, 0, 0, 0}};
static float limit(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}
void settings_default(Settings *s) {
    memset(s, 0, sizeof(*s));
    s->version = SETTINGS_VERSION;
    for (int i = 0; i < PRESETS; ++i) {
        Preset *p = &s->preset[i];
        p->mix[0] = 70;
        p->mix[1] = 80;
        p->mix[2] = 70;
        p->mix[3] = 70;
        p->route[0] = (Route){0, TARGET_AUTO, (uint8_t)(i * 2), 0};
        p->route[1] = (Route){0, TARGET_SAA, (uint8_t)(i * 2 + 1), 0};
        p->cv[0] = (CvConfig){1, 0, 1, 48};
        p->cv[1] = (CvConfig){1, 6, 2, 48};
        p->reserve_cv = 1;
        p->arp_rate = 4;
    }
    s->cv_codes_per_volt[0] = s->cv_codes_per_volt[1] = 1.0f / 0.0006875f;
}
static bool preset_valid(const Preset *p) {
    for (int i = 0; i < 4; i++)
        if (p->mix[i] > 100)
            return false;
    for (int i = 0; i < 2; i++) {
        if (p->route[i].channel > 16 || p->route[i].target >= TARGET_COUNT ||
            p->route[i].program >= PATCHES || p->route[i].forward > 1)
            return false;
        if (p->cv[i].enabled > 1 || p->cv[i].voice >= VOICES || p->cv[i].program >= PATCHES ||
            p->cv[i].base_note > 127)
            return false;
    }
    if (p->cv[0].enabled && p->cv[1].enabled && p->cv[0].voice == p->cv[1].voice)
        return false;
    return p->reserve_cv <= 1 && p->arp_mode <= 3 && p->arp_rate >= 1 && p->arp_rate <= 16 &&
           p->clock_source < SOURCES;
}
bool settings_valid(const Settings *s) {
    if (s->version != SETTINGS_VERSION || s->selected >= PRESETS)
        return false;
    for (int i = 0; i < PRESETS; i++)
        if (!preset_valid(&s->preset[i]))
            return false;
    for (int i = 0; i < 2; i++)
        if (!isfinite(s->cv_zero[i]) || fabsf(s->cv_zero[i]) > 16000 ||
            !isfinite(s->cv_codes_per_volt[i]) || s->cv_codes_per_volt[i] < 500 ||
            s->cv_codes_per_volt[i] > 3000)
            return false;
    return true;
}
void saa_frequency(float hz, uint8_t *oct, uint8_t *reg, bool *clamped) {
    float min = 8000000.0f / (512.0f * 511), max = 8000000.0f * 128 / (512 * 256);
    *clamped = !isfinite(hz) || hz < min || hz > max;
    hz = isfinite(hz) ? limit(hz, min, max) : min;
    float best = INFINITY;
    *oct = 0;
    *reg = 0;
    for (int o = 0; o < 8; o++) {
        int f = (int)lroundf(511.0f - 8000000.0f * (1u << o) / (512 * hz));
        f = f < 0 ? 0 : f > 255 ? 255 : f;
        float actual = 8000000.0f * (1u << o) / (float)(512 * (511 - f));
        float error = fabsf(actual - hz);
        if (error < best) {
            best = error;
            *oct = (uint8_t)o;
            *reg = (uint8_t)f;
        }
    }
}
uint16_t sn_divider(float hz, bool *clamped) {
    const float min = 4000000.0f / (32 * 1023), max = 4000000.0f / 32;
    *clamped = !isfinite(hz) || hz < min || hz > max;
    hz = isfinite(hz) ? limit(hz, min, max) : min;
    return (uint16_t)limit(roundf(4000000.0f / (32 * hz)), 1, 1023);
}
uint8_t sn_attenuation(float gain) {
    static const float threshold[15] = {0.891251f, 0.707946f, 0.562341f, 0.446684f, 0.354813f,
                                        0.281838f, 0.223872f, 0.177828f, 0.141254f, 0.112202f,
                                        0.089125f, 0.070795f, 0.056234f, 0.044668f, 0.035481f};
    if (!isfinite(gain))
        return 15;
    for (uint8_t attenuation = 0; attenuation < 15; attenuation++)
        if (gain >= threshold[attenuation])
            return attenuation;
    return 15;
}
static void saa(Engine *e, uint8_t r, uint8_t v) {
    if (e->saa_cache[r] != v) {
        e->saa_cache[r] = v;
        if (e->sink.saa)
            e->sink.saa(e->sink.ctx, r, v);
    }
}
static void sn(Engine *e, int chip, uint8_t v) {
    if (e->sink.sn)
        e->sink.sn(e->sink.ctx, (uint8_t)chip, v);
}
static void channels_reset(Engine *e) {
    for (int s = 0; s < SOURCES; s++)
        for (int c = 0; c < 16; c++)
            e->channel[s][c] = (Channel){100, 127, 64, 0, 0, e->preset.route[s].program, 2, 0};
}
void engine_init(Engine *e, const Preset *p, SoundSink sink) {
    memset(e, 0, sizeof(*e));
    e->preset = *p;
    e->sink = sink;
    e->midi_hz = 48;
    e->clock_running = true;
    memset(e->saa_cache, 0xff, sizeof(e->saa_cache));
    memset(e->sn_div, 0xff, sizeof(e->sn_div));
    memset(e->sn_atten, 0xff, sizeof(e->sn_atten));
    memset(e->sn_noise, 0xff, sizeof(e->sn_noise));
    channels_reset(e);
    saa(e, 0x1c, 2);
    for (int i = 0; i < 6; i++)
        saa(e, (uint8_t)i, 0);
    saa(e, 0x14, 0);
    saa(e, 0x15, 0);
    saa(e, 0x18, 0);
    saa(e, 0x19, 0);
    saa(e, 0x1c, 1);
    for (int c = 0; c < 2; c++)
        for (int v = 0; v < 4; v++) {
            sn(e, c, (uint8_t)(0x90 | (v << 5) | 15));
            e->sn_atten[c][v] = 15;
        }
}
void engine_panic(Engine *e, int source) {
    for (int i = 0; i < VOICES; i++)
        if (source < 0 || e->voice[i].source == source)
            e->voice[i].active = false;
    for (int s = 0; s < SOURCES; s++)
        if (source < 0 || s == source) {
            memcpy(e->released[s], e->pressed[s], sizeof(e->released[s]));
            for (int c = 0; c < 16; c++)
                e->channel[s][c].sustain = 0;
        }
    engine_tick(e, 0);
}
void engine_preset(Engine *e, const Preset *p) {
    engine_panic(e, -1);
    e->preset = *p;
    channels_reset(e);
}
static int chip_for(int v) {
    return v < 6 ? 0 : v < 9 || v == 12 ? 1 : 2;
}
static int noise_for(int chip) {
    return chip == 1 ? 12 : 13;
}
static int companion(int v) {
    return v == 12 ? 8 : v == 13 ? 11 : v == 2 ? 1 : v == 5 ? 4 : -1;
}
static bool cv_reserved(const Engine *e, int v) {
    return e->preset.reserve_cv && ((e->preset.cv[0].enabled && e->preset.cv[0].voice == v) ||
                                    (e->preset.cv[1].enabled && e->preset.cv[1].voice == v));
}
static bool locked(const Engine *e, int v) {
    if (cv_reserved(e, v))
        return true;
    if (v == 1 || v == 4) {
        const Voice *owner = &e->voice[v + 1];
        if (owner->active && patches[owner->program].hw_env)
            return true;
    }
    if (v == 8 || v == 11) {
        const Voice *owner = &e->voice[noise_for(v == 8 ? 1 : 2)];
        if (owner->active && patches[owner->program].noise_rate == 3)
            return true;
    }
    return false;
}
static bool fits(const Engine *e, int v, const Patch *p, int target) {
    int chip = chip_for(v);
    if (target == TARGET_SAA && chip != 0)
        return false;
    if (target == TARGET_SN1 && chip != 1)
        return false;
    if (target == TARGET_SN2 && chip != 2)
        return false;
    if (p->hw_env && v != 2 && v != 5)
        return false;
    if (v < 6) {
        if (p->noise_rate == 3 && p->noise)
            return false; /* tone-derived noise reserved for SN */
        if (p->noise)
            for (int n = (v / 3) * 3; n < (v / 3) * 3 + 3; n++)
                if (n != v && e->voice[n].active && patches[e->voice[n].program].noise &&
                    patches[e->voice[n].program].noise_rate != p->noise_rate)
                    return false;
    } else if ((p->noise != 0) != (v >= 12) || p->noise == 2)
        return false;
    int buddy = companion(v);
    if ((p->hw_env || (v >= 12 && p->noise_rate == 3)) &&
        (buddy < 0 || cv_reserved(e, buddy) ||
         (e->voice[buddy].active && e->voice[buddy].source >= SOURCES)))
        return false;
    return !locked(e, v);
}
static void release_voice(Voice *v) {
    v->held = false;
    v->sustained = false;
    v->releasing = true;
    v->release_elapsed = 0;
    v->release_level = v->level;
}
static void start(Engine *e, int v, int source, int ch, int note, int vel, int program,
                  uint16_t token, float pitch) {
    if (e->voice[v].active)
        e->steals++;
    e->voice[v] = (Voice){.active = true,
                          .held = true,
                          .source = (uint8_t)source,
                          .channel = (uint8_t)ch,
                          .note = (uint8_t)note,
                          .velocity = (uint8_t)vel,
                          .program = (uint8_t)program,
                          .token = token,
                          .age = ++e->age,
                          .pitch = pitch};
    const Patch *p = &patches[program];
    int buddy = companion(v);
    if ((p->hw_env || (v >= 12 && p->noise_rate == 3)) && buddy >= 0 && e->voice[buddy].active) {
        e->voice[buddy].active = false;
        e->steals++;
        if (buddy < 6)
            saa(e, (uint8_t)buddy, 0);
        else {
            int chip = chip_for(buddy) - 1;
            sn(e, chip, 0xdf);
            e->sn_atten[chip][2] = 15;
        }
    }
    if (v == 2 || v == 5)
        saa(e, (uint8_t)(0x18 + (v == 5)), 0);
    if (p->hw_env && (v == 2 || v == 5)) {
        saa(e, (uint8_t)(0x18 + (v == 5)),
            (uint8_t)(0x80 | (p->hw_env << 1) | (p->pan_motion ? 1 : 0)));
    }
    if (v >= 12) { /* retrigger noise shift register, including identical consecutive notes */
        int chip = v - 12;
        uint8_t noise = (uint8_t)((program == 9 ? 0 : 4) | p->noise_rate);
        sn(e, chip, (uint8_t)(0xe0 | noise));
        e->sn_noise[chip] = noise;
    }
}
static void allocate(Engine *e, int source, int ch, int note, int vel, int program, uint16_t token,
                     int target) {
    const Patch *p = &patches[program];
    int best = -1;
    uint64_t score_best = UINT64_MAX;
    for (int v = 0; v < VOICES; v++)
        if (fits(e, v, p, target)) {
            const Voice *x = &e->voice[v];
            uint64_t score =
                x->active ? (x->held ? UINT64_C(1) << 40 : UINT64_C(1) << 39) + x->age : 0;
            int buddy = companion(v);
            if ((p->hw_env || (v >= 12 && p->noise_rate == 3)) && buddy >= 0 &&
                e->voice[buddy].active) {
                const Voice *b = &e->voice[buddy];
                uint64_t cost = (b->held ? UINT64_C(1) << 40 : UINT64_C(1) << 39) + b->age;
                if (cost > score)
                    score = cost;
                score += UINT64_C(1) << 38;
            }
            /* At AUTO low notes prefer SAA (SN minimum is about B2). */
            if (target == TARGET_AUTO && !p->noise && note < 47 && v >= 6)
                score += (UINT64_C(1) << 42);
            if (target == TARGET_AUTO && p->noise == 1 && !p->pan_motion && v < 6)
                score += 1024;
            if (score < score_best) {
                score_best = score;
                best = v;
            }
        }
    if (best < 0) {
        e->refused++;
        return;
    }
    start(e, best, source, ch, note, vel, program, token, (float)note);
}
static void off(Engine *e, int s, int c, int n) {
    if (e->released[s][c][n] == e->pressed[s][c][n])
        return;
    uint16_t token = ++e->released[s][c][n];
    for (int v = 0; v < VOICES; v++) {
        Voice *x = &e->voice[v];
        if (x->active && x->source == s && x->channel == c && x->note == n && x->token == token) {
            x->held = false;
            if (e->channel[s][c].sustain)
                x->sustained = true;
            else
                release_voice(x);
        }
    }
}
void engine_message(Engine *e, uint8_t s, const uint8_t *m, uint8_t len) {
    if (s >= SOURCES || !len)
        return;
    uint8_t status = m[0];
    if (status >= 0xf8) {
        if (status <= 0xfc && s != e->preset.clock_source)
            return;
        if (status == 0xf8) {
            uint32_t dt = e->now - e->last_clock;
            if (e->last_clock && dt >= 5 && dt <= 500)
                e->midi_hz = e->midi_hz * 0.85f + (1000.0f / (float)dt) * 0.15f;
            e->last_clock = e->now;
            e->clock_seen = true;
            if (e->clock_running)
                e->clocks++;
        }
        if (status == 0xfa) {
            e->clocks = 0;
            e->clock_seen = true;
            e->clock_running = true;
        }
        if (status == 0xfb)
            e->clock_running = true;
        if (status == 0xfc) {
            e->clock_seen = true;
            e->clock_running = false;
        }
        if (status == 0xff)
            engine_panic(e, s);
        return;
    }
    if (status < 0x80 || status >= 0xf0)
        return;
    int c = status & 15, kind = status >> 4;
    if (e->preset.route[s].channel && c + 1 != e->preset.route[s].channel)
        return;
    if (len < (kind == 0xc || kind == 0xd ? 2 : 3))
        return;
    int a = m[1] & 127, b = len > 2 ? m[2] & 127 : 0;
    Channel *ch = &e->channel[s][c];
    if (kind == 8 || (kind == 9 && b == 0)) {
        off(e, s, c, a);
        return;
    }
    if (kind == 9) {
        uint16_t t = ++e->pressed[s][c][a];
        int program = ch->program, target = e->preset.route[s].target;
        if (c == 9)
            program = a == 36 ? 10 : a == 38 ? 7 : a == 42 || a == 44 ? 8 : program;
        if (target == TARGET_LAYER) {
            allocate(e, s, c, a, b, program, t, TARGET_SAA);
            if (!patches[program].hw_env && patches[program].noise != 2)
                allocate(e, s, c, a, b, program, t, (a & 1) ? TARGET_SN2 : TARGET_SN1);
        } else
            allocate(e, s, c, a, b, program, t, target);
        return;
    }
    if (kind == 0xc) {
        ch->program = (uint8_t)(a % PATCHES);
        return;
    }
    if (kind == 0xe) {
        ch->bend = (int16_t)((a | (b << 7)) - 8192);
        return;
    }
    if (kind != 0xb)
        return;
    if (a == 7)
        ch->volume = (uint8_t)b;
    if (a == 11)
        ch->expression = (uint8_t)b;
    if (a == 10)
        ch->pan = (uint8_t)b;
    if (a == 1)
        ch->modulation = (uint8_t)b;
    if (a == 64) {
        ch->sustain = (uint8_t)(b >= 64);
        if (!ch->sustain)
            for (int v = 0; v < VOICES; v++) {
                Voice *x = &e->voice[v];
                if (x->active && x->source == s && x->channel == c && x->sustained)
                    release_voice(x);
            }
    }
    if (a == 120 || a == 123) {
        for (int v = 0; v < VOICES; v++) {
            Voice *x = &e->voice[v];
            if (x->active && x->source == s && x->channel == c) {
                if (a == 120)
                    x->active = false;
                else if (ch->sustain) {
                    x->held = false;
                    x->sustained = true;
                } else
                    release_voice(x);
            }
        }
        memcpy(e->released[s][c], e->pressed[s][c], sizeof(e->released[s][c]));
    }
    if (a == 121) {
        ch->volume = 100;
        ch->expression = 127;
        ch->pan = 64;
        ch->modulation = 0;
        ch->bend = 0;
        ch->sustain = 0;
        for (int v = 0; v < VOICES; v++) {
            Voice *x = &e->voice[v];
            if (x->active && x->source == s && x->channel == c && x->sustained)
                release_voice(x);
        }
    }
}
void engine_cv(Engine *e, unsigned input, float note, bool gate) {
    if (input >= 2 || !isfinite(note))
        return;
    CvConfig *cv = &e->preset.cv[input];
    int v = cv->voice, owner = (int)input + 2;
    if (!cv->enabled)
        return;
    Voice *x = &e->voice[v];
    if (gate) {
        if (x->active && x->source == owner && x->held) {
            x->pitch = limit(note, 0, 127);
            return;
        }
        /* CV also obeys companion and shared-noise locks. Exempt its own reservation only. */
        Engine *mutable = e;
        uint8_t old = mutable->preset.reserve_cv;
        mutable->preset.reserve_cv = 0;
        bool ok = fits(e, v, &patches[cv->program],
                       chip_for(v) == 0   ? TARGET_SAA
                       : chip_for(v) == 1 ? TARGET_SN1
                                          : TARGET_SN2);
        mutable->preset.reserve_cv = old;
        int buddy = companion(v);
        if (buddy >= 0 &&
            (patches[cv->program].hw_env || (v >= 12 && patches[cv->program].noise_rate == 3)) &&
            cv_reserved(e, buddy))
            ok = false;
        if (!ok) {
            e->refused++;
            return;
        }
        start(e, v, owner, 0, (int)limit(note, 0, 127), 110, cv->program, 0, limit(note, 0, 127));
    } else if (x->active && x->source == owner && x->held)
        release_voice(x);
}
void engine_cv_pitch(Engine *e, unsigned input, float note) {
    if (input >= 2 || !isfinite(note))
        return;
    Voice *v = &e->voice[e->preset.cv[input].voice];
    if (v->active && v->held && v->source == input + 2)
        v->pitch = limit(note, 0, 127);
}
static float envelope(Voice *x, const Patch *p, uint32_t dt) {
    x->elapsed += dt;
    if (x->releasing) {
        x->release_elapsed += dt;
        if (x->release_elapsed >= p->release) {
            x->active = false;
            return 0;
        }
        return x->release_level * (1.0f - (float)x->release_elapsed / p->release);
    }
    if (x->elapsed < p->attack)
        return (float)x->elapsed / p->attack;
    uint32_t t = x->elapsed - p->attack;
    if (t < p->decay)
        return 1 - (1 - p->sustain / 100.0f) * (float)t / p->decay;
    if (!p->sustain) {
        x->active = false;
        return 0;
    }
    return p->sustain / 100.0f;
}
void engine_tick(Engine *e, uint32_t dt) {
    if (dt > 100)
        dt = 100;
    e->now += dt;
    uint8_t tone = 0, noise = 0, colour = 0, oct[3] = {0};
    uint8_t amplitude[6] = {0};
    for (int v = 0; v < VOICES; v++) {
        Voice *x = &e->voice[v];
        if (!x->active)
            continue;
        const Patch *p = &patches[x->program];
        x->level = envelope(x, p, dt);
        if (!x->active)
            continue;
        Channel *ch = x->source < SOURCES ? &e->channel[x->source][x->channel] : NULL;
        float pitch = x->pitch + (ch ? ch->bend / 8192.0f * ch->bend_range : 0);
        float sweep = 1 - limit((float)x->elapsed / p->decay, 0, 1);
        pitch += p->sweep * sweep;
        float depth = (p->vibrato + (ch ? ch->modulation * 0.8f : 0)) / 100.0f;
        if (x->elapsed > 180)
            pitch += depth * sinf((float)(e->now % 200000) * 0.032f + (float)v * 0.3f);
        if (e->preset.arp_mode) {
            static const int8_t arp[3][4] = {{0, 4, 7, 12}, {0, 3, 7, 12}, {0, 12, 0, 12}};
            uint32_t phase =
                (e->clock_seen && (!e->clock_running || e->now - e->last_clock < 600))
                    ? e->clocks / e->preset.arp_rate
                    : (uint32_t)((float)e->now * e->midi_hz / (1000 * e->preset.arp_rate));
            pitch += arp[e->preset.arp_mode - 1][phase % 4];
        }
        float hz = 440 * exp2f((pitch - 69) / 12), gain = x->level * x->velocity / 127.0f;
        gain *= e->preset.mix[0] / 100.0f * e->preset.mix[chip_for(v) + 1] / 100.0f;
        if (ch)
            gain *= ch->volume / 127.0f * ch->expression / 127.0f;
        bool clamp = false;
        if (v < 6) {
            uint8_t o, f;
            saa_frequency(hz, &o, &f, &clamp);
            saa(e, (uint8_t)(8 + v), f);
            oct[v / 2] |= (uint8_t)(o << ((v & 1) * 4));
            if (p->noise != 1)
                tone |= (uint8_t)(1u << v);
            if (p->noise) {
                noise |= (uint8_t)(1u << v);
                colour |= (uint8_t)(p->noise_rate << ((v / 3) * 4));
            }
            float pan = ch ? ch->pan / 127.0f : 0.5f;
            if (p->pan_motion)
                pan = limit(pan + 0.35f * sinf((float)(e->now % 200000) * 0.004f + (float)v), 0, 1);
            int left = (int)lroundf(15 * gain * fminf(1, 2 * (1 - pan))),
                right = (int)lroundf(15 * gain * fminf(1, 2 * pan));
            amplitude[v] = (uint8_t)((right << 4) | left);
            if (p->hw_env) { /* envelopes on channels 2/5; generator 1/4 is a silent dedicated clock
                              */
                int buddy = companion(v);
                uint8_t bo, bf;
                saa_frequency(70.0f, &bo, &bf, &clamp);
                saa(e, (uint8_t)(8 + buddy), bf);
                oct[buddy / 2] |= (uint8_t)(bo << ((buddy & 1) * 4));
            }
        } else {
            int chip = chip_for(v) - 1, chidx = v >= 12 ? 3 : (v - (chip == 0 ? 6 : 9));
            if (chidx < 3 || p->noise_rate == 3) {
                uint16_t divider =
                    sn_divider(hz * (v >= 12 && x->program == 9 ? 15.0f : 1.0f), &clamp);
                int dest = chidx < 3 ? chidx : 2;
                if (e->sn_div[chip][dest] != divider) {
                    sn(e, chip, (uint8_t)(0x80 | (dest << 5) | (divider & 15)));
                    sn(e, chip, (uint8_t)((divider >> 4) & 63));
                    e->sn_div[chip][dest] = divider;
                }
            }
            uint8_t atten = sn_attenuation(gain);
            if (e->sn_atten[chip][chidx] != atten) {
                sn(e, chip, (uint8_t)(0x90 | (chidx << 5) | atten));
                e->sn_atten[chip][chidx] = atten;
            }
        }
        if (clamp && x->elapsed == dt)
            e->pitch_clamps++;
    }
    for (int v = 0; v < 6; v++)
        saa(e, (uint8_t)v, amplitude[v]);
    for (int n = 0; n < 3; n++)
        saa(e, (uint8_t)(0x10 + n), oct[n]);
    saa(e, 0x14, tone);
    saa(e, 0x15, noise);
    saa(e, 0x16, colour);
    for (int n = 0; n < 2; n++)
        if (!e->voice[n ? 5 : 2].active)
            saa(e, (uint8_t)(0x18 + n), 0);
    for (int chip = 0; chip < 2; chip++)
        for (int ch = 0; ch < 4; ch++) {
            int v = ch == 3 ? 12 + chip : 6 + chip * 3 + ch;
            if (!e->voice[v].active && e->sn_atten[chip][ch] != 15) {
                sn(e, chip, (uint8_t)(0x90 | (ch << 5) | 15));
                e->sn_atten[chip][ch] = 15;
            }
        }
}
unsigned engine_active(const Engine *e) {
    unsigned count = 0;
    for (int i = 0; i < VOICES; i++)
        count += e->voice[i].active;
    return count;
}
