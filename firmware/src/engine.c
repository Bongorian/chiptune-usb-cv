#include "engine.h"
#include "studio.h"
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
uint32_t engine_sn_reference(const Preset *p) {
    static const uint32_t hz[] = {4000000, 2000000, 1000000};
    return p->sn_reference < 3 ? hz[p->sn_reference] : 4000000;
}
SnClockConfig engine_clock_config(const Preset *p) {
    SnClockConfig c = p->sn_clock;
    unsigned base = c.mode == SN_CLOCK_OFF ? 0 : c.base;
    base += p->sn_reference * 12u;
    c.base = (uint8_t)(base > 48 ? 48 : base);
    if (c.mode == SN_CLOCK_OFF) {
        c.mode = SN_CLOCK_MANUAL;
        c.depth = 0;
    }
    return c;
}
static const uint16_t scale_masks[CV_SCALES] = {0xfff, 0xfff, 0xab5, 0x5ad,
                                                0x6ad, 0x4a9, 0x295, 0x555};
/* Nearest allowed note; ties select the lower note, including negative octaves. */
float engine_quantize(float note, unsigned scale, unsigned root) {
    if (!isfinite(note) || scale >= CV_SCALES || root >= 12)
        return NAN;
    note = limit(note, 0, 127);
    if (!scale)
        return note;
    float best = 0, distance = INFINITY;
    int center = (int)floorf(note);
    int first = center > 6 ? center - 6 : 0;
    int last = center < 121 ? center + 6 : 127;
    for (int n = first; n <= last; n++) {
        unsigned pc = (unsigned)((n + 12 - (int)root) % 12);
        float d = fabsf(note - (float)n);
        if ((scale_masks[scale] & (1u << pc)) && d < distance) {
            distance = d;
            best = (float)n;
        }
    }
    return best;
}
unsigned engine_time_scale(unsigned v) {
    return v < 64 ? 64 + 3 * v : 256 + (v - 64) * 12;
}
void preset_performance_default(Preset *p) {
    for (unsigned source = 0; source < SOURCES; source++)
        for (unsigned ch = 0; ch < 16; ch++)
            p->performance[source][ch] = (PartPerformance){24, 0, 127, 0};
    p->tone[0] = p->tone[1] = (ToneControl){64, 64, 64, 127, 0, 3};
    memset(p->cv_scale, 0, sizeof(p->cv_scale));
    memset(p->cv_root, 0, sizeof(p->cv_root));
    memset(p->cv_glide, 0, sizeof(p->cv_glide));
    memset(p->cv_role, 0, sizeof(p->cv_role));
    memset(p->cv_range, 0, sizeof(p->cv_range));
    p->tempo = 90;
    p->clock_policy = 0;
    const uint8_t seq[] = {0, 50, 100, 25, 75, 100, 50, 0};
    memcpy(p->clock_sequence, seq, sizeof(seq));
}
static bool modulation(const Engine *e, unsigned role, float *v) {
    for (unsigned i = 0; i < 2; i++)
        if (e->preset.cv[i].enabled && e->preset.cv_role[i] == role && e->cv_mod_valid[i]) {
            *v = e->cv_mod[i];
            return true;
        }
    return false;
}
SnClockConfig engine_effective_clock(const Engine *e) {
    SnClockConfig result = e->preset.sn_clock;
    unsigned reference = e->preset.sn_reference;
    if (e->preset.studio.enabled) {
        result.base = (uint8_t)limit(result.base + e->studio.clock_base, 0, 48);
        result.depth = (uint8_t)limit(result.depth + e->studio.clock_depth, 0, 48);
        if (result.mode == SN_CLOCK_OFF && e->studio.clock_base != 0)
            result.mode = SN_CLOCK_MANUAL;
        unsigned base = (result.mode == SN_CLOCK_OFF ? 0 : result.base) + reference * 12u;
        result.base = (uint8_t)(base > 48 ? 48 : base);
        if (result.mode == SN_CLOCK_OFF) {
            result.mode = SN_CLOCK_MANUAL;
            result.depth = 0;
        }
        return result;
    }
    float amount;
    if (modulation(e, CV_SN_BASE, &amount)) {
        result.base = (uint8_t)lroundf(amount * 48);
        if (result.mode == SN_CLOCK_OFF)
            result.mode = SN_CLOCK_MANUAL;
    }
    if (modulation(e, CV_SN_DEPTH, &amount))
        result.depth = (uint8_t)lroundf(amount * 48);
    unsigned base = (result.mode == SN_CLOCK_OFF ? 0 : result.base) + reference * 12u;
    result.base = (uint8_t)(base > 48 ? 48 : base);
    if (result.mode == SN_CLOCK_OFF) {
        result.mode = SN_CLOCK_MANUAL;
        result.depth = 0;
    }
    return result;
}
uint32_t engine_transport_clocks(const Engine *e) {
    if (e->preset.clock_policy == 2)
        return e->internal_clocks;
    if (e->preset.clock_policy == 1 || e->preset.clock_policy == 3 ||
        (e->clock_seen && (!e->clock_running || e->now - e->last_clock < 600)))
        return e->clocks;
    return e->internal_clocks;
}
void engine_cv_control(Engine *e, unsigned input, float volts, bool gate) {
    if (input >= 2 || !isfinite(volts) || !e->preset.cv[input].enabled ||
        e->preset.cv_role[input] == CV_PITCH)
        return;
    e->cv_mod[input] = limit(volts / (e->preset.cv_range[input] ? 10.0f : 5.0f), 0, 1);
    e->cv_mod_valid[input] = true;
    if (gate && !e->cv_control_gate[input])
        e->sn_manual_trigger++;
    e->cv_control_gate[input] = gate;
}
void engine_cv_clear(Engine *e, unsigned input) {
    if (input >= 2)
        return;
    e->cv_mod_valid[input] = e->cv_target_valid[input] = e->cv_quantized_valid[input] = false;
    e->cv_control_gate[input] = false;
    if (e->preset.cv_role[input] == CV_PITCH)
        engine_cv(e, input, 60, false);
}
bool engine_cv_patch_ok(int v, int p) {
    if (v < 0 || v >= VOICES || p < 0 || p >= PATCHES)
        return false;
    if (patches[p].hw_env)
        return v == 2 || v == 5;
    if (v >= 12)
        return patches[p].noise == 1;
    if (v >= 6)
        return !patches[p].noise;
    return !(patches[p].noise && patches[p].noise_rate == 3);
}
bool engine_target_patch_ok(int target, int program) {
    if (target < 0 || target >= TARGET_COUNT || program < 0 || program >= PATCHES)
        return false;
    const Patch *p = &patches[program];
    bool saa_ok = !(p->noise && p->noise_rate == 3);
    bool sn_ok = !p->hw_env && p->noise != 2;
    if (target == TARGET_SAA)
        return saa_ok;
    if (target == TARGET_SN1 || target == TARGET_SN2 || target == TARGET_SN_PAIR)
        return sn_ok;
    if (target == TARGET_FULL)
        return saa_ok && sn_ok;
    return true;
}
static int pair_mate(int v) {
    return v >= 6 && v < 9 ? v + 3 : v >= 9 && v < 12 ? v - 3 : v == 12 ? 13 : v == 13 ? 12 : -1;
}
static int companion(int v);
uint16_t engine_cv_mask(const Preset *p, unsigned input, bool resources) {
    if (input >= 2 || !p->cv[input].enabled || p->cv_role[input] != CV_PITCH ||
        p->cv[input].voice >= VOICES)
        return 0;
    int v = p->cv[input].voice;
    uint16_t mask = (uint16_t)(1u << v);
    int mate = p->cv_pair[input] ? pair_mate(v) : -1;
    if (mate >= 0)
        mask |= (uint16_t)(1u << mate);
    const Patch *patch = &patches[p->cv[input].program % PATCHES];
    if (resources && (patch->hw_env || (v >= 12 && patch->noise_rate == 3))) {
        int buddy = companion(v);
        if (buddy >= 0)
            mask |= (uint16_t)(1u << buddy);
        if (mate >= 0 && companion(mate) >= 0)
            mask |= (uint16_t)(1u << companion(mate));
    }
    return mask;
}
bool engine_cv_config_valid(const Preset *p) {
    for (unsigned i = 0; i < 2; i++) {
        const CvConfig *c = &p->cv[i];
        if (c->enabled > 1 || c->voice >= VOICES || c->program >= PATCHES || c->base_note > 127 ||
            p->cv_pair[i] > 1 || p->cv_scale[i] >= CV_SCALES || p->cv_root[i] >= 12 ||
            p->cv_glide[i] > 7 || p->cv_role[i] >= CV_ROLES || p->cv_range[i] > 1)
            return false;
        if (p->cv_pair[i] && !((c->voice >= 6 && c->voice <= 8) || c->voice == 12))
            return false;
        if (c->enabled && !engine_cv_patch_ok(c->voice, c->program))
            return false;
    }
    if (p->cv[0].enabled && p->cv[1].enabled && p->cv_role[0] != CV_PITCH &&
        p->cv_role[0] == p->cv_role[1])
        return false;
    return !(engine_cv_mask(p, 0, true) & engine_cv_mask(p, 1, true));
}
void settings_default(Settings *s) {
    memset(s, 0, sizeof(*s));
    s->version = SETTINGS_VERSION;
    s->brightness = 70;
    for (unsigned i = 0; i < ST_SOUNDS; i++)
        studio_factory(i, &s->sounds[i], s->sound_name[i]);
    s->sound_used = 0xffff;
    for (int i = 0; i < PRESETS; ++i) {
        Preset *p = &s->preset[i];
        preset_performance_default(p);
        p->mix[0] = 70;
        p->mix[1] = 80;
        p->mix[2] = p->mix[3] = 70;
        p->cv[0] = (CvConfig){1, 0, 1, 48};
        p->cv[1] = (CvConfig){1, 6, 2, 36};
        p->cv_pair[1] = 1;
        p->reserve_cv = 1;
        p->arp_rate = 4;
        p->arp_target = 1; /* harmonies on SAA; keep the SN bass steady */
        p->sn_reference = 1;
        p->sn_trigger_source = 7; /* melody/CV, excluding MIDI channel 10 */
        p->sn_clock = (SnClockConfig){SN_CLOCK_OFF, 0, 24, 4, 0, 1};
        for (unsigned source = 0; source < SOURCES; source++) {
            p->route[source] = (Route){0, TARGET_SAA, 5, 0};
            p->channel_map[source] = 1;
            for (unsigned ch = 0; ch < 16; ch++)
                p->part[source][ch] = (MidiPart){TARGET_OFF, 0};
            p->part[source][0] = (MidiPart){TARGET_SAA, 5};
            p->part[source][1] = (MidiPart){TARGET_SN_PAIR, 2};
            p->part[source][2] = (MidiPart){TARGET_SAA, 0};
            p->part[source][3] = (MidiPart){TARGET_SAA, 3};
            p->part[source][9] = (MidiPart){TARGET_SN_PAIR, 10};
            if (i == 1)
                p->part[source][1].program = 15;
            if (i == 3)
                p->part[source][0].program = 1;
            if (i == 4)
                p->part[source][1] = (MidiPart){TARGET_SAA, 2};
            if (i == 5) {
                p->part[source][1].program = 15;
                p->part[source][9] = (MidiPart){TARGET_SAA, 7};
            }
            if (i == 6 || i == 7)
                p->part[source][1].program = 15;
        }
        if (i == 1) {
            p->sn_width = 12;
            p->cv[0].program = 5;
        }
        if (i == 2) {
            p->arp_mode = 2;
            p->arp_rate = 6;
            p->cv[0].voice = 2;
            p->cv[0].program = 3;
        }
        if (i == 3)
            p->sn_reference = 2;
        if (i == 4) {
            p->sn_clock = (SnClockConfig){SN_CLOCK_DIVE, 0, 36, 3, 0, 1};
            p->sn_trigger_source = 4;
            p->cv[1].enabled = 0;
        }
        if (i == 5) {
            p->sn_clock = (SnClockConfig){SN_CLOCK_TRIANGLE, 0, 24, 11, 0, 0};
            p->cv[1].voice = 12;
            p->cv[1].program = 9;
        }
        if (i == 6)
            p->sn_clock = (SnClockConfig){SN_CLOCK_RANDOM, 0, 36, 12, 0, 0};
        if (i == 7)
            p->sn_clock = (SnClockConfig){SN_CLOCK_SEQUENCE, 0, 24, 4, 4, 0};
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
    if (!engine_cv_config_valid(p) || p->sn_reference > 2 || p->sn_width > 30 ||
        p->sn_trigger_source > 7 || p->arp_target > 2)
        return false;
    for (unsigned source = 0; source < SOURCES; source++) {
        if (p->channel_map[source] > 1)
            return false;
        for (unsigned ch = 0; ch < 16; ch++) {
            if (p->part[source][ch].target >= TARGET_COUNT ||
                p->part[source][ch].program >= PATCHES)
                return false;
        }
    }
    for (unsigned source = 0; source < SOURCES; source++)
        for (unsigned ch = 0; ch < 16; ch++) {
            const PartPerformance *f = &p->performance[source][ch];
            if (f->transpose > 48 || f->low > f->high || f->high > 127 || f->velocity > 3)
                return false;
        }
    for (unsigned i = 0; i < 2; i++) {
        const ToneControl *t = &p->tone[i];
        if (t->attack > 127 || t->decay > 127 || t->release > 127 ||
            (t->sustain > 100 && t->sustain != 127) || t->tremolo > 100 || t->rate > 7)
            return false;
    }
    for (unsigned i = 0; i < 8; i++)
        if (p->clock_sequence[i] > 100)
            return false;
    if (p->tempo > 210 || p->clock_policy > 3 || !studio_valid(&p->studio))
        return false;
    return p->reserve_cv <= 1 && p->arp_mode < ARP_MODES && p->arp_rate >= 1 && p->arp_rate <= 16 &&
           p->clock_source < SOURCES && sn_clock_valid(&p->sn_clock);
}
bool settings_valid(const Settings *s) {
    if (s->version != SETTINGS_VERSION || s->selected >= PRESETS)
        return false;
    for (int i = 0; i < PRESETS; i++)
        if (!preset_valid(&s->preset[i]))
            return false;
    if (s->brightness < 10 || s->brightness > 100)
        return false;
    for (unsigned i = 0; i < ST_SOUNDS; i++)
        if (s->sound_used & (1u << i)) {
            if (!studio_sound_valid(&s->sounds[i]) || s->sound_name[i][8])
                return false;
            for (unsigned c = 0; c < 8; c++)
                if (s->sound_name[i][c] < 32 || s->sound_name[i][c] > 90)
                    return false;
        }
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
uint16_t sn_divider_clock(float hz, uint32_t clock, bool *clamped) {
    if (clock < 250000 || clock > 4000000)
        clock = 4000000;
    float min = (float)clock / (32 * 1023), max = (float)clock / 32;
    *clamped = !isfinite(hz) || hz < min || hz > max;
    hz = isfinite(hz) ? limit(hz, min, max) : min;
    return (uint16_t)limit(roundf((float)clock / (32 * hz)), 1, 1023);
}
uint16_t sn_divider(float hz, bool *clamped) {
    return sn_divider_clock(hz, 4000000, clamped);
}
/* Tone-derived white noise has no note-frequency fundamental. Clocking its
   shift register at a bass/drum note rate produces slow steps, not broadband
   percussion. Use five octaves of colour offset; periodic noise uses its
   15-stage repeat length to retain the requested pitched fundamental. */
float sn_noise_clock_hz(float note_hz, bool white) {
    return note_hz * (white ? 32.0f : 15.0f);
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
void engine_routes_reset(Engine *e, unsigned source) {
    if (source >= SOURCES)
        return;
    for (unsigned c = 0; c < 16; c++) {
        uint8_t program = e->preset.channel_map[source] ? e->preset.part[source][c].program
                                                        : e->preset.route[source].program;
        e->channel[source][c] = (Channel){.volume = 100,
                                          .expression = 127,
                                          .pan = 64,
                                          .program = program,
                                          .bend_range = 2,
                                          .rpn_msb = 127,
                                          .rpn_lsb = 127,
                                          .attack = 64,
                                          .decay = 64,
                                          .release = 64,
                                          .envelope_sustain = 127};
    }
}
static void channels_reset(Engine *e) {
    for (unsigned s = 0; s < SOURCES; s++)
        engine_routes_reset(e, s);
}
static void retire_voice(Engine *e, int v);
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
    if (p->studio.enabled)
        studio_reset(e);
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
    if (e->preset.studio.enabled)
        studio_panic(e, source);
    if (source < 0) {
        memset(e->cv_target_valid, 0, sizeof(e->cv_target_valid));
        memset(e->cv_mod_valid, 0, sizeof(e->cv_mod_valid));
        memset(e->cv_quantized_valid, 0, sizeof(e->cv_quantized_valid));
        memset(e->cv_control_gate, 0, sizeof(e->cv_control_gate));
    }
    for (int i = 0; i < VOICES; i++)
        if (source < 0 || e->voice[i].source == source)
            retire_voice(e, i);
    for (int s = 0; s < SOURCES; s++)
        if (source < 0 || s == source) {
            memcpy(e->released[s], e->pressed[s], sizeof(e->released[s]));
            for (int c = 0; c < 16; c++)
                e->channel[s][c].sustain = 0;
        }
    engine_tick(e, 0);
}
void engine_transport_reset(Engine *e) {
    e->clocks = e->internal_clocks = e->internal_phase = e->last_clock = 0;
    e->clock_seen = false;
    e->clock_running = true;
    e->sn_manual_trigger++;
}
void engine_preset(Engine *e, const Preset *p) {
    engine_panic(e, -1);
    e->preset = *p;
    if (p->studio.enabled)
        studio_reset(e);
    engine_transport_reset(e);
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
    return e->preset.reserve_cv &&
           ((engine_cv_mask(&e->preset, 0, true) | engine_cv_mask(&e->preset, 1, true)) &
            (1u << v));
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
    if (target == TARGET_OFF)
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
static void mute_voice(Engine *e, int v) {
    if (v < 6) {
        saa(e, (uint8_t)v, 0);
        if (v == 2 || v == 5)
            saa(e, (uint8_t)(0x18 + (v == 5)), 0);
    } else {
        int chip = chip_for(v) - 1;
        int ch = v >= 12 ? 3 : (v - 6) % 3;
        sn(e, chip, (uint8_t)(0x90 | (ch << 5) | 15));
        e->sn_atten[chip][ch] = 15;
    }
}
static void retire_voice(Engine *e, int v) {
    Voice old = e->voice[v];
    int mate = old.pair ? old.pair - 1 : -1;
    for (int i = 0; i < VOICES; i++) {
        Voice *x = &e->voice[i];
        bool linked = old.hybrid && x->hybrid && old.source == x->source &&
                      old.channel == x->channel && old.note == x->note && old.token == x->token;
        if (i == v || (i == mate && x->pair == v + 1) || linked) {
            x->pair = x->hybrid = 0;
            if (x->active) {
                x->active = false;
                mute_voice(e, i);
            }
        }
    }
}
static bool sn_trigger(const Engine *e, int v, int source, int ch) {
    if (e->preset.sn_trigger_source == 2)
        return source == 2;
    if (e->preset.sn_trigger_source == 3)
        return source == 3;
    if (!chip_for(v))
        return false;
    switch (e->preset.sn_trigger_source) {
    case 1:
        return source < SOURCES;
    case 2:
        return source == 2;
    case 3:
        return source == 3;
    case 4:
        return source < SOURCES && ch == 9;
    case 5:
        return v < 12;
    case 6:
        return v >= 12;
    case 7:
        return source >= SOURCES || ch != 9;
    default:
        return true;
    }
}
static void start(Engine *e, int v, int source, int ch, int note, int vel, int program,
                  uint16_t token, float pitch) {
    if (e->voice[v].active) {
        e->steals++;
        retire_voice(e, v);
    }
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
    if (sn_trigger(e, v, source, ch))
        e->sn_note_trigger++;
    int buddy = companion(v);
    if ((p->hw_env || (v >= 12 && p->noise_rate == 3)) && buddy >= 0 && e->voice[buddy].active) {
        e->steals++;
        retire_voice(e, buddy);
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
static uint64_t voice_cost(const Engine *e, int v, const Patch *p) {
    const Voice *x = &e->voice[v];
    uint64_t score = x->active ? (x->held ? UINT64_C(1) << 40 : UINT64_C(1) << 39) + x->age : 0;
    int buddy = companion(v);
    if ((p->hw_env || (v >= 12 && p->noise_rate == 3)) && buddy >= 0 && e->voice[buddy].active) {
        const Voice *b = &e->voice[buddy];
        uint64_t cost = (b->held ? UINT64_C(1) << 40 : UINT64_C(1) << 39) + b->age;
        if (cost > score)
            score = cost;
        score += UINT64_C(1) << 38;
    }
    return score;
}
static int find_voice(const Engine *e, float note, int program, int target) {
    const Patch *p = &patches[program];
    int best = -1;
    uint64_t score_best = UINT64_MAX;
    for (int v = 0; v < VOICES; v++)
        if (fits(e, v, p, target)) {
            uint64_t score = voice_cost(e, v, p);
            if (target == TARGET_AUTO && !p->noise && v >= 6 &&
                440 * exp2f(((float)note - 69) / 12) <
                    (float)engine_sn_reference(&e->preset) / (32 * 1023))
                score += UINT64_C(1) << 42;
            if (target == TARGET_AUTO && p->noise == 1 && !p->pan_motion && v < 6)
                score += 1024;
            if (score < score_best) {
                score_best = score;
                best = v;
            }
        }
    return best;
}
static int find_pair(const Engine *e, int program) {
    const Patch *p = &patches[program];
    int best = -1;
    uint64_t score_best = UINT64_MAX;
    for (int left = 6; left <= 12; left++) {
        if (left > 8 && left < 12)
            continue;
        int right = pair_mate(left);
        if (!fits(e, left, p, TARGET_SN1) || !fits(e, right, p, TARGET_SN2))
            continue;
        uint64_t a = voice_cost(e, left, p), b = voice_cost(e, right, p);
        uint64_t score = (a > b ? a : b) + ((a || b) ? (UINT64_C(1) << 38) : 0);
        if (score < score_best) {
            score_best = score;
            best = left;
        }
    }
    return best;
}
static void start_pair(Engine *e, int left, int source, int ch, int note, int vel, int program,
                       uint16_t token, float pitch) {
    int right = pair_mate(left);
    start(e, left, source, ch, note, vel, program, token, pitch);
    start(e, right, source, ch, note, vel, program, token, pitch);
    e->voice[left].pair = (uint8_t)(right + 1);
    e->voice[right].pair = (uint8_t)(left + 1);
}
static void allocate(Engine *e, int source, int ch, int note, int vel, int program, uint16_t token,
                     int target) {
    float sounding_note = (float)note + (int)e->preset.performance[source][ch].transpose - 24;
    if (target == TARGET_SN_PAIR || target == TARGET_FULL) {
        int pair = find_pair(e, program);
        int saa_voice =
            target == TARGET_FULL ? find_voice(e, sounding_note, program, TARGET_SAA) : -1;
        if (pair < 0 || (target == TARGET_FULL && saa_voice < 0)) {
            e->refused++;
            return;
        }
        if (saa_voice >= 0)
            start(e, saa_voice, source, ch, note, vel, program, token, (float)note);
        start_pair(e, pair, source, ch, note, vel, program, token, (float)note);
        if (saa_voice >= 0) {
            e->voice[saa_voice].hybrid = e->voice[pair].hybrid = e->voice[pair_mate(pair)].hybrid =
                1;
        }
        return;
    }
    int best = find_voice(e, sounding_note, program, target);
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
        if (status <= 0xfc && (e->preset.clock_policy == 2 || e->preset.clock_policy == 3))
            return;
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
            e->clocks = e->internal_clocks = e->internal_phase = 0;
            e->last_clock = e->now;
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
        if (e->preset.studio.enabled)
            studio_realtime(e, status, s);
        return;
    }
    if (status < 0x80 || status >= 0xf0)
        return;
    int c = status & 15, kind = status >> 4;
    if (!e->preset.studio.enabled && e->preset.route[s].channel &&
        c + 1 != e->preset.route[s].channel)
        return;
    if (len < (kind == 0xc || kind == 0xd ? 2 : 3))
        return;
    int a = m[1] & 127, b = len > 2 ? m[2] & 127 : 0;
    Channel *ch = &e->channel[s][c];
    if (e->preset.studio.enabled) {
        e->studio.midi_seen[s] = true;
        e->studio.midi_time[s] = e->now;
        e->studio.midi_channel[s] = (uint8_t)c;
    }
    if (e->preset.studio.enabled && kind == 0xc) {
        studio_program(e, s, (unsigned)c, (unsigned)a);
        return;
    }
    if (e->preset.studio.enabled && kind == 0xb)
        studio_control(e, s, (unsigned)c, (unsigned)a, (unsigned)b);
    if (e->preset.studio.enabled && (kind == 8 || kind == 9)) {
        studio_message(e, s, m, len);
        return;
    }
    if (kind == 8 || (kind == 9 && b == 0)) {
        off(e, s, c, a);
        return;
    }
    if (kind == 9) {
        uint16_t t = ++e->pressed[s][c][a];
        int program = ch->program, target = e->preset.channel_map[s] ? e->preset.part[s][c].target
                                                                     : e->preset.route[s].target;
        const PartPerformance *performance = &e->preset.performance[s][c];
        if (target == TARGET_OFF || a < performance->low || a > performance->high)
            return; /* keep the token even for filtered notes: matching Note Off stays ordered */
        float velocity = (float)b / 127.0f;
        if (performance->velocity == 1)
            velocity = sqrtf(velocity);
        if (performance->velocity == 2)
            velocity *= velocity;
        b = performance->velocity == 3 ? 110 : (int)lroundf(127 * velocity);
        if (b < 1)
            b = 1;
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
    if (kind == 0xd) {
        ch->pressure = (uint8_t)a;
        return;
    }
    if (kind == 0xa) {
        for (unsigned v = 0; v < VOICES; v++)
            if (e->voice[v].active && e->voice[v].source == s && e->voice[v].channel == c &&
                e->voice[v].note == a)
                e->voice[v].pressure = (uint8_t)b;
        return;
    }
    if (kind != 0xb)
        return;
    if (s == e->preset.clock_source && a >= 20 && a <= 29) {
        SnClockConfig *clock = &e->preset.sn_clock;
        if (a == 20)
            clock->mode = (uint8_t)(b < (int)SN_CLOCK_MODES ? b : (int)SN_CLOCK_MODES - 1);
        if (a == 21)
            clock->base = (uint8_t)((b * 48 + 63) / 127);
        if (a == 22)
            clock->depth = (uint8_t)((b * 48 + 63) / 127);
        if (a == 23)
            clock->rate = (uint8_t)((b * 15 + 63) / 127);
        if (a == 24)
            clock->sync = (uint8_t)((b * 8 + 63) / 127);
        if (a == 25)
            clock->retrigger = (uint8_t)(b >= 64);
        if (a == 26 && b >= 64)
            e->sn_manual_trigger++;
        if (a == 27) {
            uint8_t reference = (uint8_t)(b > 2 ? 2 : b);
            if (reference != e->preset.sn_reference) {
                engine_panic(e, -1);
                e->preset.sn_reference = reference;
            }
        }
        if (a == 28)
            e->preset.sn_width = (uint8_t)((b * 30 + 63) / 127);
        if (a == 29)
            e->preset.sn_trigger_source = (uint8_t)(b > 7 ? 7 : b);
    }
    if (s == e->preset.clock_source && a == 30)
        e->sequence_step = (uint8_t)(b > 7 ? 7 : b);
    if (s == e->preset.clock_source && a == 31)
        e->preset.clock_sequence[e->sequence_step] = (uint8_t)((b * 100 + 63) / 127);
    if (a == 101)
        ch->rpn_msb = (uint8_t)b;
    if (a == 100)
        ch->rpn_lsb = (uint8_t)b;
    if (a == 99 || a == 98)
        ch->rpn_msb = ch->rpn_lsb = 127; /* NRPN deselects the active registered parameter */
    if (!ch->rpn_msb && !ch->rpn_lsb) {
        if (a == 6)
            ch->bend_range = (uint8_t)(b > 24 ? 24 : b);
        if (a == 38)
            ch->bend_cents = (uint8_t)(b > 99 ? 99 : b);
    }
    if (a == 73)
        ch->attack = (uint8_t)b;
    if (a == 75)
        ch->decay = (uint8_t)b;
    if (a == 72)
        ch->release = (uint8_t)b;
    if (a == 70)
        ch->envelope_sustain = (uint8_t)((b * 100 + 63) / 127);
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
        ch->sustain = ch->pressure = 0;
        ch->attack = ch->decay = ch->release = 64;
        ch->envelope_sustain = ch->rpn_msb = ch->rpn_lsb = 127;
        for (unsigned v = 0; v < VOICES; v++)
            if (e->voice[v].source == s && e->voice[v].channel == c)
                e->voice[v].pressure = 0;
        for (int v = 0; v < VOICES; v++) {
            Voice *x = &e->voice[v];
            if (x->active && x->source == s && x->channel == c && x->sustained)
                release_voice(x);
        }
    }
}
void engine_cv(Engine *e, unsigned input, float note, bool gate) {
    if (input >= 2 || !isfinite(note) || e->preset.cv_role[input] != CV_PITCH)
        return;
    CvConfig *cv = &e->preset.cv[input];
    int v = cv->voice, owner = (int)input + 2;
    if (!cv->enabled)
        return;
    Voice *x = &e->voice[v];
    int mate = e->preset.cv_pair[input] ? pair_mate(v) : -1;
    if (gate) {
        if (x->active && x->source == owner && x->held) {
            engine_cv_pitch(e, input, note);
            return;
        }
        uint8_t enabled = cv->enabled;
        cv->enabled = 0; /* exempt only this CV's reserved resources */
        bool ok = fits(e, v, &patches[cv->program],
                       chip_for(v) == 0   ? TARGET_SAA
                       : chip_for(v) == 1 ? TARGET_SN1
                                          : TARGET_SN2);
        if (mate >= 0)
            ok = ok && fits(e, mate, &patches[cv->program], TARGET_SN2);
        cv->enabled = enabled;
        if (!ok) {
            e->refused++;
            return;
        }
        e->cv_quantized_valid[input] = false;
        engine_cv_pitch(e, input, note);
        float pitch = e->cv_target[input];
        if (mate >= 0)
            start_pair(e, v, owner, 0, (int)pitch, 110, cv->program, 0, pitch);
        else
            start(e, v, owner, 0, (int)pitch, 110, cv->program, 0, pitch);
    } else {
        if (x->active && x->source == owner && x->held)
            release_voice(x);
        if (mate >= 0 && e->voice[mate].active && e->voice[mate].source == owner &&
            e->voice[mate].held)
            release_voice(&e->voice[mate]);
    }
}
void engine_cv_pitch(Engine *e, unsigned input, float note) {
    if (input >= 2 || !isfinite(note))
        return;
    if (e->preset.cv_role[input] != CV_PITCH)
        return;
    float quantized = engine_quantize(note, e->preset.cv_scale[input], e->preset.cv_root[input]);
    if (e->preset.cv_scale[input] && e->cv_quantized_valid[input] &&
        fabsf(note - e->cv_quantized[input]) <= fabsf(note - quantized) + 0.16f)
        quantized = e->cv_quantized[input]; /* 0.08 semitone threshold hysteresis */
    e->cv_quantized[input] = quantized;
    e->cv_quantized_valid[input] = e->cv_target_valid[input] = true;
    e->cv_target[input] = quantized;
    if (e->preset.cv_glide[input])
        return;
    int index = e->preset.cv[input].voice;
    Voice *v = &e->voice[index];
    if (v->active && v->held && v->source == input + 2)
        v->pitch = quantized;
    int mate = e->preset.cv_pair[input] ? pair_mate(index) : -1;
    if (mate >= 0 && e->voice[mate].active && e->voice[mate].held &&
        e->voice[mate].source == input + 2)
        e->voice[mate].pitch = quantized;
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
    if (e->clock_running) {
        e->internal_phase += dt * (e->preset.tempo + 30u) * 24u;
        e->internal_clocks += e->internal_phase / 60000u;
        e->internal_phase %= 60000u;
    }
    if (e->preset.studio.enabled) {
        studio_tick(e, dt);
        return;
    }
    static const uint16_t glide_ms[] = {0, 10, 30, 60, 120, 250, 500, 1000};
    for (unsigned input = 0; input < 2; input++)
        if (e->cv_target_valid[input] && e->preset.cv_glide[input]) {
            unsigned index = e->preset.cv[input].voice;
            Voice *voice = &e->voice[index];
            if (voice->active && voice->held && voice->source == input + 2) {
                voice->pitch += (e->cv_target[input] - voice->pitch) *
                                ((float)dt / (glide_ms[e->preset.cv_glide[input]] + (float)dt));
                if (voice->pair)
                    e->voice[voice->pair - 1].pitch = voice->pitch;
            }
        }
    /* Slow external clocks also slow the SN bus. Keep the shared bus available to MIDI/ADC/UI. */
    SnClockConfig effective_clock = engine_effective_clock(e);
    bool sn_frequency_due =
        sn_clock_write_us(&effective_clock) <= 40 || e->now - e->sn_frequency_updated >= 4;
    if (sn_frequency_due)
        e->sn_frequency_updated = e->now;
    uint8_t tone = 0, noise = 0, colour = 0, oct[3] = {0};
    uint8_t amplitude[6] = {0};
    for (int v = 0; v < VOICES; v++) {
        Voice *x = &e->voice[v];
        if (!x->active)
            continue;
        Patch edited = patches[x->program];
        Channel *ch = x->source < SOURCES ? &e->channel[x->source][x->channel] : NULL;
        const ToneControl *control = &e->preset.tone[v < 6 ? 0 : 1];
        edited.attack =
            (uint16_t)limit((float)((uint32_t)edited.attack * engine_time_scale(control->attack) *
                                    engine_time_scale(ch ? ch->attack : 64) / 65536u),
                            1, 8000);
        edited.decay =
            (uint16_t)limit((float)((uint32_t)edited.decay * engine_time_scale(control->decay) *
                                    engine_time_scale(ch ? ch->decay : 64) / 65536u),
                            1, 8000);
        edited.release =
            (uint16_t)limit((float)((uint32_t)edited.release * engine_time_scale(control->release) *
                                    engine_time_scale(ch ? ch->release : 64) / 65536u),
                            1, 8000);
        if (control->sustain != 127)
            edited.sustain = control->sustain;
        if (ch && ch->envelope_sustain != 127)
            edited.sustain = ch->envelope_sustain;
        const Patch *p = &edited;
        x->level = envelope(x, p, dt);
        if (!x->active)
            continue;
        float pitch =
            x->pitch + (ch ? ch->bend / 8192.0f * (ch->bend_range + ch->bend_cents / 100.0f) : 0);
        if (ch)
            pitch += (int)e->preset.performance[x->source][x->channel].transpose - 24;
        float sweep = 1 - limit((float)x->elapsed / p->decay, 0, 1);
        pitch += p->sweep * sweep;
        int phase_voice = x->pair && chip_for(v) == 2 ? x->pair - 1 : v;
        float depth = (p->vibrato +
                       (ch ? fmaxf(ch->modulation, fmaxf(ch->pressure, x->pressure)) * 0.8f : 0)) /
                      100.0f;
        if (x->elapsed > 180)
            pitch += depth * sinf((float)(e->now % 200000) * 0.032f + (float)phase_voice * 0.3f);
        if (e->preset.arp_mode && (!e->preset.arp_target || (e->preset.arp_target == 1 && v < 6) ||
                                   (e->preset.arp_target == 2 && v >= 6))) {
            static const int8_t arp[ARP_MODES - 1][4] = {
                {0, 4, 7, 12}, {0, 3, 7, 12}, {0, 12, 0, 12}, {0, 4, 7, 11},
                {0, 3, 7, 10}, {0, 3, 7, 9},  {0, 7, 12, 19}};
            uint32_t phase = engine_transport_clocks(e) / e->preset.arp_rate;
            pitch += arp[e->preset.arp_mode - 1][phase % 4];
        }
        float amount, width = e->preset.sn_width;
        if (modulation(e, CV_SN_WIDTH, &amount))
            width = amount * 30;
        if (x->pair && v >= 6)
            pitch += ((v >= 9 && v < 12) || v == 13 ? 1.0f : -1.0f) * width / 200.0f;
        float hz = 440 * exp2f((pitch - 69) / 12), gain = x->level * x->velocity / 127.0f;
        float master = e->preset.mix[0] / 100.0f;
        if (modulation(e, CV_MASTER, &amount))
            master *= amount;
        gain *= master * e->preset.mix[chip_for(v) + 1] / 100.0f;
        static const float trem_hz[] = {0.5f, 1, 2, 4, 8, 16, 32, 64};
        float trem_depth = control->tremolo / 100.0f;
        if (modulation(e, CV_TREMOLO, &amount))
            trem_depth = amount;
        if (trem_depth > 0)
            gain *= 1 - trem_depth * (0.5f + 0.5f * sinf((float)(e->now % 200000) *
                                                         trem_hz[control->rate] * 0.0062831853f));
        if (ch)
            gain *= ch->volume / 127.0f * ch->expression / 127.0f;
        if (x->pair && v >= 6) {
            float pan = ch ? ch->pan / 127.0f : 0.5f;
            if (p->pan_motion)
                pan = limit(pan + 0.35f *
                                      sinf((float)(e->now % 200000) * 0.004f + (float)phase_voice),
                            0, 1);
            bool right = (v >= 9 && v < 12) || v == 13;
            gain *= fminf(1, 2 * (right ? pan : 1 - pan));
        }
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
            if (modulation(e, CV_SAA_PAN, &amount))
                pan = amount;
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
                    sn_divider_clock(v >= 12 ? sn_noise_clock_hz(hz, x->program != 9) : hz,
                                     engine_sn_reference(&e->preset), &clamp);
                int dest = chidx < 3 ? chidx : 2;
                if (e->sn_div[chip][dest] != divider &&
                    (sn_frequency_due || x->elapsed == dt || e->sn_div[chip][dest] == UINT16_MAX)) {
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

unsigned engine_active_notes(const Engine *e) {
    unsigned count = 0;
    for (unsigned v = 0; v < VOICES; v++)
        if (e->voice[v].active) {
            bool seen = false;
            const Voice *x = &e->voice[v];
            for (unsigned j = 0; j < v; j++) {
                const Voice *y = &e->voice[j];
                if (y->active && x->source == y->source && x->channel == y->channel &&
                    x->note == y->note && x->token == y->token)
                    seen = true;
            }
            if (!seen)
                count++;
        }
    return count;
}
