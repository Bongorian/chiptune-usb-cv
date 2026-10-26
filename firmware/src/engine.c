#include "engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static float clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static void release(Voice *v) {
    if (v->active && !v->releasing) {
        v->held = v->sustained = false;
        v->releasing = true;
        v->release_elapsed = 0;
        v->release_level = v->level;
    }
}
static Channel channel_default(void) {
    return (Channel){.volume = 100,
                     .expression = 127,
                     .pan = 64,
                     .bend_range = 2,
                     .rpn_msb = 127,
                     .rpn_lsb = 127};
}
float engine_sn_reference(const Engine *e) {
    if (e->project.sn_clock.follow == SN_FOLLOW_RAW)
        return 2000000;
    if (e->project.sn_clock.follow == SN_FOLLOW_FULL)
        return (float)e->sn_clock_hz;
    return e->sn_pitch_hz;
}
SnClockConfig engine_effective_clock(const Engine *e) {
    SnClockConfig c = e->project.sn_clock;
    if (e->clock_base || e->clock_depth) {
        SnModConfig *m = &c.mod[0];
        if (m->domain == SN_DOMAIN_MUSIC) {
            int64_t offset = (int64_t)m->offset + (int32_t)(e->clock_base * 100);
            int64_t depth = (int64_t)m->depth + (int32_t)(e->clock_depth * 100);
            m->offset = (int32_t)(offset > INT32_MAX   ? INT32_MAX
                                  : offset < INT32_MIN ? INT32_MIN
                                                       : offset);
            m->depth = (int32_t)(depth > INT32_MAX   ? INT32_MAX
                                 : depth < INT32_MIN ? INT32_MIN
                                                     : depth);
            if (m->mode == SN_CLOCK_OFF)
                m->mode = SN_CLOCK_MANUAL;
        }
    }
    return c;
}
uint32_t engine_transport_clocks(const Engine *e) {
    return e->project.clock_policy == 2 ? e->internal_clocks
           : e->project.clock_policy == 1 || e->project.clock_policy == 3 ||
                   (e->clock_seen && (!e->clock_running || e->now - e->last_clock < 600))
               ? e->clocks
               : e->internal_clocks;
}
float engine_quantize(float note, unsigned scale, unsigned root) {
    static const uint16_t mask[] = {0xfff, 0xfff, 0xab5, 0x5ad, 0x6ad, 0x4a9, 0x295, 0x555};
    if (!isfinite(note) || scale >= CV_SCALES || root > 11)
        return NAN;
    note = clamp(note, 0, 127);
    if (!scale)
        return note;
    int center = (int)note;
    float best = 0, distance = 1000;
    for (int n = center - 6; n <= center + 6; n++)
        if (n >= 0 && n <= 127 &&
            (mask[scale] & (1u << ((unsigned)(n + 12 - (int)root) % 12)))) {
            float d = fabsf((float)n - note);
            if (d < distance) {
                distance = d;
                best = (float)n;
            }
        }
    return best;
}
void engine_init(Engine *e, const Project *p, SoundSink sink) {
    memset(e, 0, sizeof(*e));
    e->sn_clock_hz = 2000000;
    e->sn_pitch_hz = 2000000;
    e->project = *p;
    e->system =
        (System){.cv_codes_per_volt = {1.0f / .0006875f, 1.0f / .0006875f}, .brightness = 70};
    e->random = 0x10997649;
    e->clock_running = true;
    bus_init(&e->bus, sink);
    if (!resource_plan(p, NULL, NULL, &e->plan, &e->conflict))
        project_default(&e->project);
    (void)resource_plan(&e->project, NULL, NULL, &e->plan, &e->conflict);
    SnClockPlan initial = sn_clock_base(&e->project.sn_clock);
    e->sn_pitch_hz = (float)initial.hz;
    e->sn_clock_hz = e->project.sn_clock.stopped ? 0 : initial.hz;
    e->sn_clock_period = initial.period;
    e->sn_clock_div256 = initial.div256;
    e->sn_clock_state = e->project.sn_clock.stopped ? SN_STOPPED
                        : initial.experimental      ? SN_DEGRADED
                                                    : SN_VALID;
    for (unsigned s = 0; s < SOURCES; s++)
        for (unsigned c = 0; c < 16; c++)
            e->channel[s][c] = channel_default();
    for (unsigned t = 0; t < TRACKS; t++) {
        e->motion[t].pulse = UINT32_MAX;
        e->effective[t] = e->project.track[t].patch;
        memcpy(e->macro[t], e->project.track[t].macro, 4);
    }
}
void engine_reset_audio(Engine *e, SoundSink sink) {
    static Project current;
    current = e->project;
    System system = e->system;
    const Settings *library = e->library;
    engine_init(e, &current, sink);
    e->system = system;
    e->library = library;
}
void engine_stop_track(Engine *e, unsigned t) {
    if (t >= TRACKS)
        return;
    for (unsigned i = 0; i < 14; i++)
        if (e->voice[i].active && e->voice[i].track == t) {
            unsigned source = e->voice[i].source;
            if (source == 2 || source == 3)
                e->gate_block[source - 2] = true;
            e->voice[i].active = false;
        }
    memset(e->held[t], 0, sizeof(e->held[t]));
    e->motion[t].due = 0;
    if (e->test_on && e->test_track == t)
        e->test_on = false;
    uint16_t mask = resource_track_mask(&e->plan, t, false);
    for (unsigned v = 0; v < 14; v++)
        if (mask & (1u << v)) {
            if (v < 6)
                bus_saa(&e->bus, v, 0);
            else
                bus_volume(&e->bus, (v < 9 || v == 12) ? 0 : 1, v >= 12 ? 3 : (v - 6) % 3, 15);
        }
}
static bool morph_valid(const Engine *e, const Project *p) {
    for (unsigned t = 0; t < TRACKS; t++) {
        const Track *a = &p->track[t];
        if (!a->morph_target)
            continue;
        unsigned slot = a->morph_target - 1;
        if (!e->library || !(e->library->patch_used & (1u << slot)) ||
            !patch_same_structure(&a->patch, &e->library->patches[slot]))
            return false;
    }
    return true;
}
bool engine_apply(Engine *e, const Project *p) {
    static ResourcePlan candidate;
    if (!resource_plan(p, &e->project, &e->plan, &candidate, &e->conflict)) {
        e->refused++;
        return false;
    }
    if (!morph_valid(e, p)) {
        e->conflict.reason = RESOURCE_MORPH;
        snprintf(e->conflict.message, 40, "Morph requires matching structure");
        snprintf(e->conflict.suggestion, 56, "Choose matching A/B chip, source and layers");
        e->refused++;
        return false;
    }
    for (unsigned t = 0; t < TRACKS; t++) {
        bool structural =
            !patch_same_structure(&p->track[t].patch, &e->project.track[t].patch) ||
            p->track[t].mode != e->project.track[t].mode ||
            candidate.count[t] != e->plan.count[t];
        bool routing = false;
        for (unsigned s = 0; s < SOURCES; s++)
            for (unsigned c = 0; c < 16; c++)
                if (((p->route[s][c] ^ e->project.route[s][c]) & (1u << t)))
                    routing = true;
        if (structural || routing || (!p->track[t].enabled && e->project.track[t].enabled))
            engine_stop_track(e, t);
        if (memcmp(p->track[t].macro, e->project.track[t].macro, 4))
            memcpy(e->macro[t], p->track[t].macro, 4);
    }
    for (unsigned i = 0; i < 2; i++)
        if (memcmp(&p->cv[i], &e->project.cv[i], sizeof(CvRoute))) {
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].source == i + 2)
                    release(&e->voice[v]);
            e->gate_block[i] = e->gate[i];
            e->cv_valid[i] = false;
        }
    e->project = *p;
    e->sn_pitch_hz = (float)sn_clock_base(&p->sn_clock).hz;
    e->plan = candidate;
    return true;
}
bool engine_load_patch(Engine *e, unsigned t, const Patch *p, const char name[9]) {
    if (t >= TRACKS || !patch_valid(p) || !name_valid(name))
        return false;
    static Project candidate;
    candidate = e->project;
    candidate.track[t].patch = *p;
    candidate.track[t].morph_target = candidate.track[t].morph = 0;
    memcpy(candidate.track[t].name, name, 9);
    return engine_apply(e, &candidate);
}
unsigned engine_active(const Engine *e) {
    unsigned n = 0;
    for (unsigned i = 0; i < 14; i++)
        n += e->voice[i].active;
    return n;
}
unsigned engine_track_notes(const Engine *e, unsigned t) {
    unsigned n = 0;
    for (unsigned i = 0; i < 14; i++)
        n += e->voice[i].active && e->voice[i].track == t;
    return n;
}
void engine_release_source(Engine *e, unsigned t, unsigned source) {
    for (unsigned i = 0; i < 14; i++)
        if (e->voice[i].active && e->voice[i].track == t && e->voice[i].source == source)
            release(&e->voice[i]);
}
void engine_panic(Engine *e, int source) {
    for (unsigned i = 0; i < 14; i++)
        if (source < 0 || e->voice[i].source == (unsigned)source)
            e->voice[i].active = false;
    for (unsigned t = 0; t < TRACKS; t++)
        for (unsigned h = 0; h < HELD_NOTES; h++)
            if (source < 0 || e->held[t][h].source == (unsigned)source)
                e->held[t][h].velocity = 0;
    for (unsigned s = 0; s < SOURCES; s++)
        if (source < 0 || s == (unsigned)source) {
            memcpy(e->released[s], e->pressed[s], sizeof(e->pressed[s]));
            for (unsigned c = 0; c < 16; c++)
                e->channel[s][c].sustain = 0;
        }
    if (source < 0) {
        e->running = e->test_on = false;
        memset(e->cv_valid, 0, sizeof(e->cv_valid));
        bus_mute(&e->bus);
    }
}
void engine_note(Engine *e, unsigned t, unsigned source, unsigned channel, unsigned note,
                 unsigned velocity, uint32_t token, float pitch) {
    if (t >= TRACKS || !e->project.track[t].enabled || !e->plan.count[t])
        return;
    const Track *a = &e->project.track[t];
    int selected = -1;
    uint32_t oldest = 0;
    if (a->mode != PLAY_POLY)
        for (unsigned v = 0; v < 14; v++)
            if (e->voice[v].active && e->voice[v].track == t) {
                selected = (int)v;
                break;
            }
    if (selected < 0) {
        for (unsigned slot = 0; slot < e->plan.count[t]; slot++) {
            bool occupied = false;
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].track == t && e->voice[v].slot == slot)
                    occupied = true;
            if (!occupied) {
                for (unsigned v = 0; v < 14; v++)
                    if (!e->voice[v].active) {
                        selected = (int)v;
                        break;
                    }
                if (selected >= 0) {
                    e->voice[selected].slot = (uint8_t)slot;
                    break;
                }
            }
        }
    }
    if (selected < 0)
        for (unsigned v = 0; v < 14; v++)
            if (e->voice[v].active && e->voice[v].track == t) {
                uint32_t age = e->age - e->voice[v].age;
                if (selected < 0 || (e->voice[v].releasing && !e->voice[selected].releasing) ||
                    (e->voice[v].releasing == e->voice[selected].releasing && age > oldest)) {
                    selected = (int)v;
                    oldest = age;
                }
            }
    if (selected < 0) {
        e->refused++;
        return;
    }
    Voice *v = &e->voice[selected];
    bool active = v->active;
    unsigned slot = v->slot;
    bool legato = active && a->mode == PLAY_LEGATO && v->source == source && !v->releasing;
    float previous = v->pitch;
    if (active && !legato)
        e->steals++;
    if (legato) {
        v->note = (uint8_t)note;
        v->velocity = (uint8_t)velocity;
        v->token = token;
        v->target = pitch;
        v->held = true;
        v->sustained = false;
        v->channel = (uint8_t)channel;
        v->age = ++e->age;
        return;
    }
    *v = (Voice){.active = true,
                 .held = true,
                 .source = (uint8_t)source,
                 .channel = (uint8_t)channel,
                 .note = (uint8_t)note,
                 .velocity = (uint8_t)velocity,
                 .track = (uint8_t)t,
                 .slot = (uint8_t)slot,
                 .token = token,
                 .age = ++e->age,
                 .pitch = active && a->patch.glide ? previous : pitch,
                 .target = pitch};
    const Patch *p = &a->patch;
    uint16_t out = e->plan.outputs[t][slot];
    if (p->chip == CHIP_SN && p->source != SOURCE_TONE)
        for (unsigned c = 0; c < 2; c++)
            if (out & (1u << (12 + c)))
                bus_noise(&e->bus, c, (p->noise_type ? 4u : 0u) | p->noise_rate, true);
    if (p->chip == CHIP_SAA && p->env && p->retrigger)
        for (unsigned g = 0; g < 2; g++)
            if (out & (1u << (2 + g * 3)))
                bus_env_retrigger(&e->bus, g);
    unsigned trigger = e->project.sn_trigger_source;
    if (p->chip == CHIP_SN &&
        (trigger == 0 || (trigger == 1 && source < 2) || (trigger == 2 && source == 2) ||
         (trigger == 3 && source == 3) || (trigger == 4 && source < 2 && channel == 9) ||
         (trigger == 5 && p->source == SOURCE_TONE) ||
         (trigger == 6 && p->source != SOURCE_TONE) ||
         (trigger == 7 && p->source == SOURCE_TONE)))
        e->sn_note_trigger++;
}
static void restore_mono(Engine *e, unsigned t) {
    for (unsigned i = 0; i < 14; i++)
        if (e->voice[i].active && e->voice[i].track == t &&
            (e->voice[i].held || e->voice[i].sustained))
            return;
    if (e->project.track[t].mode == PLAY_POLY)
        return;
    HeldNote *best = NULL;
    for (unsigned h = 0; h < HELD_NOTES; h++) {
        HeldNote *k = &e->held[t][h];
        if (k->velocity && (!best || (int32_t)(k->age - best->age) > 0))
            best = k;
    }
    if (best && e->project.track[t].mode == PLAY_LEGATO)
        for (unsigned i = 0; i < 14; i++)
            if (e->voice[i].active && e->voice[i].track == t &&
                e->voice[i].source == best->source)
                e->voice[i].releasing = false;
    if (best)
        engine_note(e, t, best->source, best->channel, best->note, best->velocity, best->token,
                    best->note);
}
static void note_off(Engine *e, unsigned source, unsigned channel, unsigned note) {
    if (e->released[source][channel][note] == e->pressed[source][channel][note])
        return;
    uint32_t token = ++e->released[source][channel][note];
    bool affected[TRACKS] = {0};
    for (unsigned t = 0; t < TRACKS; t++)
        for (unsigned h = 0; h < HELD_NOTES; h++) {
            HeldNote *k = &e->held[t][h];
            if (k->velocity && k->source == source && k->channel == channel &&
                k->note == note && k->token == token) {
                affected[t] = true;
                if (e->channel[source][channel].sustain)
                    k->sustained = true;
                else
                    k->velocity = 0;
            }
        }
    for (unsigned i = 0; i < 14; i++) {
        Voice *v = &e->voice[i];
        if (v->active && v->source == source && v->channel == channel && v->note == note &&
            v->token == token) {
            affected[v->track] = true;
            v->held = false;
            if (e->channel[source][channel].sustain)
                v->sustained = true;
            else
                release(v);
        }
    }
    for (unsigned t = 0; t < TRACKS; t++)
        if (affected[t])
            restore_mono(e, t);
}
static void control(Engine *e, unsigned source, unsigned channel, unsigned cc, unsigned value) {
    Channel *c = &e->channel[source][channel];
    if (cc == 7)
        c->volume = (uint8_t)value;
    if (cc == 11)
        c->expression = (uint8_t)value;
    if (cc == 10)
        c->pan = (uint8_t)value;
    if (cc == 1)
        c->modulation = (uint8_t)value;
    if (cc == 101)
        c->rpn_msb = (uint8_t)value;
    if (cc == 100)
        c->rpn_lsb = (uint8_t)value;
    if (cc == 98 || cc == 99)
        c->rpn_msb = c->rpn_lsb = 127;
    if (!c->rpn_msb && !c->rpn_lsb) {
        if (cc == 6)
            c->bend_range = (uint8_t)(value > 24 ? 24 : value);
        if (cc == 38)
            c->bend_cents = (uint8_t)(value > 99 ? 99 : value);
    }
    if (cc >= 16 && cc <= 19)
        for (unsigned t = 0; t < TRACKS; t++)
            if (e->project.route[source][channel] & (1u << t))
                e->project.track[t].macro[cc - 16] = e->macro[t][cc - 16] =
                    (int8_t)((int)(value * 200 / 127) - 100);
    if (cc == 64 || cc == 121) {
        c->sustain = (uint8_t)(cc == 64 && value >= 64);
        if (cc == 121)
            *c = channel_default();
        if (!c->sustain) {
            bool restore[TRACKS] = {0};
            for (unsigned t = 0; t < TRACKS; t++)
                for (unsigned h = 0; h < HELD_NOTES; h++) {
                    HeldNote *k = &e->held[t][h];
                    if (k->source == source && k->channel == channel && k->sustained)
                        k->velocity = 0;
                }
            for (unsigned i = 0; i < 14; i++) {
                Voice *v = &e->voice[i];
                if (v->active && v->source == source && v->channel == channel && v->sustained) {
                    release(v);
                    restore[v->track] = true;
                }
            }
            for (unsigned t = 0; t < TRACKS; t++)
                if (restore[t])
                    restore_mono(e, t);
        }
    }
    if (cc == 120 || cc == 123) {
        for (unsigned t = 0; t < TRACKS; t++)
            for (unsigned h = 0; h < HELD_NOTES; h++) {
                HeldNote *k = &e->held[t][h];
                if (k->source == source && k->channel == channel) {
                    if (cc == 123 && c->sustain)
                        k->sustained = true;
                    else
                        k->velocity = 0;
                }
            }
        for (unsigned i = 0; i < 14; i++) {
            Voice *v = &e->voice[i];
            if (v->active && v->source == source && v->channel == channel) {
                if (cc == 120)
                    v->active = false;
                else if (c->sustain) {
                    v->held = false;
                    v->sustained = true;
                } else
                    release(v);
            }
        }
        memcpy(e->released[source][channel], e->pressed[source][channel],
               sizeof(e->pressed[source][channel]));
    }
    if (source == e->project.clock_source && cc >= 20 && cc <= 27) {
        SnModConfig *k = &e->project.sn_clock.mod[0];
        if (cc == 20)
            k->mode = (uint8_t)(value >= SN_CLOCK_MODES ? SN_CLOCK_MODES - 1 : value);
        if (cc == 21)
            k->offset = ((int32_t)value - 64) * 100;
        if (cc == 22)
            k->depth = (int32_t)value * 100;
        if (cc == 23)
            k->rate_millihz = 250u + (uint32_t)value * 250u;
        if (cc == 24)
            k->sync = (uint8_t)((value * 8 + 63) / 127);
        if (cc == 25)
            k->retrigger = (uint8_t)(value >= 64);
        if (cc == 26 && value >= 64)
            e->sn_manual_trigger++;
        if (cc == 27)
            e->project.sn_clock.follow = (uint8_t)(value > 2 ? 2 : value);
    }
}
void engine_message(Engine *e, uint8_t source, const uint8_t *m, uint8_t len) {
    if (source >= SOURCES || !len)
        return;
    unsigned status = m[0];
    if (status >= 0xf8) {
        if (status == 0xff) {
            engine_panic(e, source);
            return;
        }
        if (source != e->project.clock_source || e->project.clock_policy >= 2)
            return;
        if (status == 0xf8) {
            e->clock_seen = true;
            e->last_clock = e->now;
            if (e->clock_running)
                e->clocks++;
        }
        if (status == 0xfa) {
            e->clocks = e->internal_clocks = e->internal_phase = 0;
            e->clock_seen = e->clock_running = true;
            sequence_run(e, true, true);
        }
        if (status == 0xfb) {
            e->clock_seen = e->clock_running = true;
            sequence_run(e, true, false);
        }
        if (status == 0xfc) {
            e->clock_seen = true;
            e->clock_running = false;
            sequence_run(e, false, false);
        }
        return;
    }
    if (status < 0x80 || status >= 0xf0)
        return;
    unsigned channel = status & 15, kind = status >> 4;
    if (len < (kind == 12 || kind == 13 ? 2 : 3))
        return;
    unsigned a = m[1] & 127, b = len > 2 ? m[2] & 127 : 0;
    Channel *c = &e->channel[source][channel];
    e->midi_seen[source] = true;
    e->midi_time[source] = e->now;
    e->midi_channel[source] = (uint8_t)channel;
    if (kind == 8 || (kind == 9 && !b)) {
        note_off(e, source, channel, a);
        return;
    }
    if (kind == 9) {
        uint32_t token = ++e->pressed[source][channel][a];
        for (unsigned t = 0; t < TRACKS; t++)
            if (e->project.route[source][channel] & (1u << t)) {
                const Track *track = &e->project.track[t];
                if (!track->enabled || a < track->low || a > track->high)
                    continue;
                unsigned velocity = track->velocity_curve == 1
                                        ? (unsigned)(fast_sqrt((float)b / 127) * 127)
                                    : track->velocity_curve == 2 ? b * b / 127
                                    : track->velocity_curve == 3 ? 110
                                                                 : b;
                int slot = -1;
                uint32_t age = 0;
                for (unsigned h = 0; h < HELD_NOTES; h++)
                    if (!e->held[t][h].velocity) {
                        slot = (int)h;
                        break;
                    } else if (slot < 0 || e->age - e->held[t][h].age > age) {
                        slot = (int)h;
                        age = e->age - e->held[t][h].age;
                    }
                e->held[t][slot] = (HeldNote){.token = token,
                                              .age = ++e->age,
                                              .source = source,
                                              .channel = (uint8_t)channel,
                                              .note = (uint8_t)a,
                                              .velocity = (uint8_t)(velocity ? velocity : 1)};
                if (!e->project.sequence[t].arp)
                    engine_note(e, t, source, channel, a, velocity ? velocity : 1, token,
                                (float)a);
            }
        return;
    }
    if (kind == 11) {
        control(e, source, channel, a, b);
        return;
    }
    if (kind == 14) {
        c->bend = (int16_t)((a | (b << 7)) - 8192);
        return;
    }
    if (kind == 13) {
        c->pressure = (uint8_t)a;
        return;
    }
    if (kind == 10) {
        for (unsigned i = 0; i < 14; i++)
            if (e->voice[i].active && e->voice[i].source == source &&
                e->voice[i].channel == channel && e->voice[i].note == a)
                e->voice[i].pressure = (uint8_t)b;
        return;
    }
    if (kind == 12 && e->library && a < USER_PATCHES && (e->library->patch_used & (1u << a))) {
        static Project candidate;
        candidate = e->project;
        for (unsigned t = 0; t < TRACKS; t++)
            if (candidate.route[source][channel] & (1u << t)) {
                candidate.track[t].patch = e->library->patches[a];
                candidate.track[t].morph_target = candidate.track[t].morph = 0;
                memcpy(candidate.track[t].name, e->library->patch_name[a], 9);
            }
        (void)engine_apply(e, &candidate);
    }
}
void engine_cv(Engine *e, unsigned input, float volts, bool gate, bool fresh) {
    if (input >= 2)
        return;
    const CvRoute *c = &e->project.cv[input];
    bool rising = gate && !e->gate[input];
    e->gate[input] = gate;
    if (!gate)
        e->gate_block[input] = false;
    fresh = fresh && isfinite(volts);
    bool was_fresh = e->cv_valid[input];
    e->cv_valid[input] = fresh;
    if (fresh) {
        e->cv[input] = clamp(volts / (c->range ? 10.0f : 5.0f), 0, 1);
        if (c->mode == 1) {
            float raw = c->base_note + volts * 12;
            float pitch = engine_quantize(raw, c->scale, c->root);
            if (was_fresh && c->scale &&
                fabsf(raw - e->cv_note[input]) <= fabsf(raw - pitch) + .16f)
                pitch = e->cv_note[input];
            e->cv_note[input] = pitch;
            for (unsigned i = 0; i < 14; i++)
                if (e->voice[i].active && e->voice[i].source >= 2 && e->voice[i].source <= 3 &&
                    e->voice[i].track == c->track && e->voice[i].held)
                    e->voice[i].target = e->cv_note[input];
        }
    }
    if (!fresh && c->mode == 1)
        for (unsigned i = 0; i < 14; i++)
            if (e->voice[i].active && e->voice[i].track == c->track &&
                e->voice[i].source >= 2 && e->voice[i].source <= 3)
                release(&e->voice[i]);
    if (!gate || !fresh)
        engine_release_source(e, c->gate_track, input + 2);
    if (!gate || !rising || e->gate_block[input])
        return;
    if (e->project.clock_policy == 3 && e->project.gate_clock == input) {
        e->clocks += 6;
        e->clock_running = true;
    }
    if (c->gate == 1 || c->gate == 2) {
        if (!fresh)
            return;
        float pitch = c->gate_note;
        for (unsigned i = 0; i < 2; i++)
            if (e->project.cv[i].mode == 1 && e->project.cv[i].track == c->gate_track &&
                e->cv_valid[i])
                pitch = e->cv_note[i];
        if (c->gate == 2)
            engine_release_source(e, c->gate_track, input + 2);
        engine_note(e, c->gate_track, input + 2, 0, (unsigned)clamp(pitch, 0, 127), 100,
                    ++e->age, pitch);
    } else if (c->gate == 3 &&
               !(e->project.clock_policy == 3 && e->project.gate_clock == input))
        sequence_gate(e, c->gate_track);
    else if (c->gate == 4)
        e->sn_manual_trigger++;
}
void engine_audition(Engine *e, unsigned t, unsigned note, unsigned mode) {
    if (t >= TRACKS || note > 127 || mode > 3)
        return;
    for (unsigned i = 0; i < TRACKS; i++)
        engine_release_source(e, i, 4);
    e->test_on = mode != 0;
    e->test_track = (uint8_t)t;
    e->test_note = (uint8_t)note;
    e->test_mode = (uint8_t)mode;
    if (mode) {
        engine_note(e, t, 4, 0, note, 100, ++e->age, (float)note);
        e->test_due = e->now + 1000;
        e->test_off = e->now + 400;
    }
}
void render_tick(Engine *e, uint32_t dt);
void engine_tick(Engine *e, uint32_t dt) {
    if (dt > 1000)
        dt = 1000;
    e->now += dt;
    uint32_t phase = e->internal_phase + dt * (e->project.tempo + 30u) * 24u;
    e->internal_clocks += phase / 60000u;
    e->internal_phase = phase % 60000u;
    sequence_tick(e, dt);
    render_tick(e, dt);
}

void engine_bind_library(Engine *e, const Settings *s) { e->library = s; }
bool engine_poly_focus(Engine *e, unsigned track) {
    if (track >= TRACKS)
        return false;
    static Project next;
    static ResourcePlan plan;
    next = e->project;
    unsigned chip = next.track[track].patch.chip;
    for (unsigned t = 0; t < TRACKS; t++)
        if (t != track && next.track[t].patch.chip == chip)
            next.track[t].enabled = 0;
    next.track[track].enabled = 1;
    next.track[track].mode = PLAY_POLY;
    bool routed = false;
    for (unsigned s = 0; s < SOURCES; s++)
        for (unsigned c = 0; c < 16; c++)
            routed |= (next.route[s][c] & (1u << track)) != 0;
    if (!routed)
        for (unsigned s = 0; s < SOURCES; s++)
            next.route[s][track] |= (uint16_t)(1u << track);
    for (unsigned voices = MAX_POLYPHONY; voices; voices--) {
        next.track[track].voices = (uint8_t)voices;
        if (resource_plan(&next, &e->project, &e->plan, &plan, NULL))
            return engine_apply(e, &next);
    }
    return false;
}
