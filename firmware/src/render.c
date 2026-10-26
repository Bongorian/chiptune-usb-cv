#include "engine.h"
#include <math.h>
#include <string.h>
static float bound(float x, float lo, float hi) {
    return x < lo ? lo : x > hi ? hi : x;
}
static float source_value(Engine *e, const ModRoute *m) {
    if (m->source == 1 || m->source == 2)
        return e->cv_valid[m->source - 1] ? e->cv[m->source - 1] : 0;
    if (m->source == 3 || m->source == 4) {
        float value = 0;
        for (unsigned s = 0; s < SOURCES; s++)
            for (unsigned c = 0; c < 16; c++)
                if (e->project.route[s][c] & (1u << m->track)) {
                    float v =
                        (m->source == 3 ? e->channel[s][c].modulation : e->channel[s][c].pressure) /
                        127.0f;
                    if (v > value)
                        value = v;
                }
        if (m->source == 4)
            for (unsigned i = 0; i < 14; i++)
                if (e->voice[i].active && e->voice[i].track == m->track) {
                    float v = e->voice[i].pressure * (1.0f / 127.0f);
                    if (v > value)
                        value = v;
                }
        return value;
    }
    if (m->source < 5 + TRACKS)
        return e->lfo[m->source - 5];
    return e->motion[m->source - 5 - TRACKS].lane * 0.01f;
}
static void parameter(Engine *e, unsigned t, unsigned dest, float amount) {
    Patch *p = &e->effective[t];
    if (!amount)
        return;
    switch (dest) {
    case MOD_LEVEL:
        p->level = (uint8_t)bound(p->level + 100 * amount, 0, 100);
        break;
    case MOD_PAN:
        e->pan_mod[t] += amount;
        break;
    case MOD_PITCH:
        e->pitch_mod[t] += amount * 24;
        break;
    case MOD_VIBRATO:
        p->vibrato = (uint8_t)bound(p->vibrato + 100 * amount, 0, 100);
        break;
    case MOD_TREMOLO:
        p->tremolo = (uint8_t)bound(p->tremolo + 100 * amount, 0, 100);
        break;
    case MOD_LAYER:
        p->layer_level = (uint8_t)bound(p->layer_level + 100 * amount, 0, 100);
        break;
    case MOD_ENV_RATIO:
        p->env_ratio = (int8_t)bound(p->env_ratio + 48 * amount, -48, 48);
        break;
    case MOD_NOISE_PITCH:
        if (p->noise_rate == 3)
            p->noise_pitch = (int8_t)bound(p->noise_pitch + 48 * amount, -48, 48);
        break;
    case MOD_CLOCK_BASE:
        e->clock_base += 48 * amount;
        break;
    case MOD_CLOCK_DEPTH:
        e->clock_depth += 48 * amount;
        break;
    default:
        break;
    }
}
static float envelope(Voice *v, const Patch *p, uint32_t dt) {
    v->elapsed += dt;
    if (v->releasing) {
        v->release_elapsed += dt;
        if (!p->release || v->release_elapsed >= p->release) {
            v->active = false;
            return 0;
        }
        return v->release_level * (1 - (float)v->release_elapsed / p->release);
    }
    if (p->attack && v->elapsed < p->attack)
        return (float)v->elapsed / p->attack;
    unsigned time = v->elapsed - p->attack;
    if (p->decay && time < p->decay)
        return 1 - (1 - p->sustain * 0.01f) * (float)time / p->decay;
    if (!p->sustain)
        v->active = false;
    return p->sustain * 0.01f;
}
static void saa_pitch(Engine *e, unsigned v, float hz, uint8_t oct[3], bool *clamp) {
    uint8_t o, f;
    saa_note_register(hz, &o, &f, clamp);
    bus_saa(&e->bus, 8 + v, f);
    unsigned shift = (v & 1) * 4;
    oct[v / 2] = (uint8_t)((oct[v / 2] & ~(15u << shift)) | ((unsigned)o << shift));
}
static uint8_t saa_gain(float gain, float pan, unsigned output) {
    unsigned l = output == OUTPUT_RIGHT ? 0 : saa_amplitude(gain * fminf(1, 2 * (1 - pan)));
    unsigned r = output == OUTPUT_LEFT ? 0 : saa_amplitude(gain * fminf(1, 2 * pan));
    return (uint8_t)(l | (r << 4));
}
static void sn_pitch(Engine *e, unsigned c, unsigned n, float hz) {
    bool clamp;
    unsigned divider = sn_divider_clock(hz, engine_sn_reference(&e->project), &clamp);
    bus_tone(&e->bus, c, n, divider);
    e->pitch_clamps += clamp;
}
void render_tick(Engine *e, uint32_t dt) {
    memset(e->pitch_mod, 0, sizeof(e->pitch_mod));
    memset(e->pan_mod, 0, sizeof(e->pan_mod));
    e->clock_base = e->clock_depth = 0;
    for (unsigned t = 0; t < TRACKS; t++) {
        const Track *a = &e->project.track[t];
        unsigned morph = a->morph;
        for (unsigned m = 0; m < 4; m++)
            if (a->macro_dest[m] == MOD_MORPH)
                morph = (unsigned)bound((float)morph + e->macro[t][m], 0, 100);
        for (unsigned i = 0; i < MOD_SLOTS; i++)
            if (e->project.mod[i].track == t && e->project.mod[i].source &&
                e->project.mod[i].parameter == MOD_MORPH)
                morph = (unsigned)bound((float)morph + source_value(e, &e->project.mod[i]) *
                                                           e->project.mod[i].amount,
                                        0, 100);
        for (unsigned input = 0; input < 2; input++)
            if (e->cv_valid[input] && e->project.cv[input].mode == 2 &&
                e->project.cv[input].track == t && e->project.cv[input].parameter == MOD_MORPH)
                morph = (unsigned)bound((float)morph + e->cv[input] * e->project.cv[input].amount,
                                        0, 100);
        if (a->morph_target && e->library)
            patch_morph(&e->effective[t], &a->patch, &e->library->patches[a->morph_target - 1],
                        morph);
        else
            e->effective[t] = a->patch;
        if (!a->enabled)
            continue;
        for (unsigned m = 0; m < 4; m++)
            parameter(e, t, a->macro_dest[m], e->macro[t][m] * 0.01f);
        if (e->running && e->project.sequence[t].enabled)
            parameter(e, t, a->macro_dest[0], e->motion[t].lane * 0.01f);
    }
    for (unsigned i = 0; i < 2; i++)
        if (e->project.cv[i].mode == 2 && e->cv_valid[i])
            parameter(e, e->project.cv[i].track, e->project.cv[i].parameter,
                      e->cv[i] * e->project.cv[i].amount * 0.01f);
    for (unsigned i = 0; i < MOD_SLOTS; i++) {
        const ModRoute *m = &e->project.mod[i];
        if (m->source)
            parameter(e, m->track, m->parameter, source_value(e, m) * m->amount * 0.01f);
    }
    uint8_t tone = 0, noise = 0, colour = 0, amp[6] = {0}, oct[3] = {0};
    bool env_on[2] = {false, false};
    uint8_t volume[2][4];
    memset(volume, 15, sizeof(volume));
    for (unsigned i = 0; i < 14; i++) {
        Voice *v = &e->voice[i];
        if (!v->active)
            continue;
        unsigned t = v->track;
        const Track *a = &e->project.track[t];
        const Patch *p = &e->effective[t];
        v->level = envelope(v, p, dt);
        if (!v->active)
            continue;
        unsigned glide = p->glide;
        static const unsigned cv_glide[] = {0, 10, 30, 60, 120, 250, 500, 1000};
        if (v->source == 2 || v->source == 3)
            for (unsigned input = 0; input < 2; input++)
                if (e->project.cv[input].mode == 1 && e->project.cv[input].track == t)
                    glide = cv_glide[e->project.cv[input].glide];
        if (glide)
            v->pitch += (v->target - v->pitch) * dt / (glide + (float)dt);
        else
            v->pitch = v->target;
        const Channel *c = v->source < SOURCES ? &e->channel[v->source][v->channel] : NULL;
        float progress = 1 - bound((float)v->elapsed / (p->pitch_decay ? p->pitch_decay : 1), 0, 1);
        float pitch = v->pitch + p->transpose + p->fine * 0.01f + e->pitch_mod[t] +
                      p->pitch_amount * progress + p->vibrato * 0.01f * e->lfo[t];
        if (c)
            pitch += c->bend * (1.0f / 8192.0f) * (c->bend_range + c->bend_cents * 0.01f);
        float gain = v->level * v->velocity * (1.0f / 127.0f) * p->level * 0.01f * a->level *
                     0.01f * e->project.mix[0] * 0.01f;
        if (c)
            gain *= c->volume * (1.0f / 127.0f) * c->expression * (1.0f / 127.0f);
        gain *= 1 - p->tremolo * 0.01f * (.5f + .5f * e->lfo[t]);
        if (a->mute)
            gain = 0;
        float pan =
            bound(a->pan * 0.01f + e->pan_mod[t] + (c ? (c->pan - 64) * (1.0f / 127.0f) : 0), 0, 1);
        uint16_t out = e->plan.outputs[t][v->slot];
        float noise_hz = p->chip == CHIP_SN && p->source != SOURCE_TONE && p->noise_rate == 3
                             ? midi_note_hz(pitch + p->noise_pitch + p->noise_sweep * progress)
                             : 0;
        if (p->chip == CHIP_SAA) {
            gain *= e->project.mix[1] * 0.01f;
            gain *= sound_output_gain(0);
            for (unsigned n = 0; n < 6; n++)
                if (out & (1u << n)) {
                    bool clamp = false;
                    if (p->env) {
                        if (n != 2 && n != 5)
                            continue;
                        unsigned group = n / 3;
                        float clock =
                            pitch + (p->env == ENV_OSC ? ((p->shape == 4 || p->shape == 5)
                                                              ? (p->env_steps == 16 ? 48 : 36)
                                                              : (p->env_steps == 16 ? 36 : 24))
                                                       : p->env_ratio);
                        saa_pitch(e, n - 1, clock, oct, &clamp);
                        env_on[group] = true;
                        bus_saa(&e->bus, 0x18 + group,
                                0x80u | (p->shape << 1) | (p->env_steps == 8 ? 0x10u : 0) |
                                    p->env_invert);
                        if (p->env == ENV_AM && p->source != SOURCE_NOISE) {
                            bool carrier;
                            saa_pitch(e, n, pitch, oct, &carrier);
                            tone |= (uint8_t)(1u << n);
                        }
                        if (p->source != SOURCE_TONE) {
                            noise |= (uint8_t)(1u << n);
                            colour |= (uint8_t)(p->noise_rate << (group * 4));
                            if (p->noise_rate == 3) {
                                bool nc;
                                saa_pitch(e, group * 3,
                                          pitch + p->noise_pitch + p->noise_sweep * progress, oct,
                                          &nc);
                            }
                        }
                        if (p->env == ENV_OSC && clamp)
                            gain = 0;
                        if (p->layers > 1) {
                            unsigned aux = n - 2;
                            float layer_hz = pitch + p->layer_interval;
                            if (p->noise_rate == 3 && p->layer_source != SOURCE_TONE)
                                layer_hz += p->noise_pitch + p->noise_sweep * progress;
                            bool lc = false;
                            if (p->layer_source != SOURCE_NOISE || p->noise_rate == 3)
                                saa_pitch(e, aux, layer_hz, oct, &lc);
                            if (p->layer_source != SOURCE_NOISE)
                                tone |= (uint8_t)(1u << aux);
                            if (p->layer_source != SOURCE_TONE) {
                                noise |= (uint8_t)(1u << aux);
                                colour |= (uint8_t)(p->noise_rate << (group * 4));
                            }
                            float layer_gain =
                                gain * p->layer_level * 0.01f *
                                (p->layer_decay ? fast_decay((float)v->elapsed / p->layer_decay)
                                                : 1);
                            amp[aux] = saa_gain(layer_gain, pan, p->output);
                        }
                    } else {
                        saa_pitch(e, n, pitch, oct, &clamp);
                        if (p->source != SOURCE_NOISE)
                            tone |= (uint8_t)(1u << n);
                        if (p->source != SOURCE_TONE) {
                            unsigned group = n / 3;
                            noise |= (uint8_t)(1u << n);
                            colour |= (uint8_t)(p->noise_rate << (group * 4));
                            if (p->noise_rate == 3)
                                saa_pitch(e, group * 3,
                                          pitch + p->noise_pitch + p->noise_sweep * progress, oct,
                                          &clamp);
                        }
                    }
                    amp[n] = saa_gain(gain, pan, p->output);
                    if (clamp)
                        e->pitch_clamps++;
                }
        } else {
            for (unsigned side = 0; side < 2; side++) {
                float side_gain =
                    gain * e->project.mix[side + 2] * 0.01f * sound_output_gain(side + 1);
                if (p->output == OUTPUT_STEREO)
                    side_gain *= fminf(1, 2 * (side ? pan : 1 - pan));
                float detune =
                    p->output == OUTPUT_STEREO ? (side ? 1.0f : -1.0f) * p->detune * 0.005f : 0;
                if (p->source == SOURCE_TONE) {
                    unsigned layer = 0;
                    for (unsigned n = 0; n < 3; n++)
                        if (out & (1u << (6 + side * 3 + n))) {
                            float offset = layer == 1   ? p->layer_interval
                                           : layer == 2 ? p->stack_interval
                                                        : 0;
                            sn_pitch(e, side, n, midi_note_hz(pitch + detune + offset));
                            volume[side][n] = sn_attenuation(side_gain / p->layers);
                            layer++;
                        }
                } else if (out & (1u << (12 + side))) {
                    bus_noise(&e->bus, side, (p->noise_type ? 4u : 0u) | p->noise_rate, false);
                    if (p->noise_rate == 3)
                        sn_pitch(e, side, 2,
                                 noise_hz * (p->noise_type ? 32.0f : 15.0f) * fast_ratio(detune));
                    if (p->source == SOURCE_MIX) {
                        volume[side][2] = sn_attenuation(side_gain * (1 - p->noise_mix * 0.01f));
                        side_gain *= p->noise_mix * 0.01f;
                    }
                    volume[side][3] = sn_attenuation(side_gain);
                }
            }
        }
    }
    for (unsigned v = 0; v < 6; v++)
        bus_saa(&e->bus, v, amp[v]);
    for (unsigned n = 0; n < 3; n++)
        bus_saa(&e->bus, 0x10 + n, oct[n]);
    bus_saa(&e->bus, 0x14, tone);
    bus_saa(&e->bus, 0x15, noise);
    bus_saa(&e->bus, 0x16, colour);
    for (unsigned g = 0; g < 2; g++)
        if (!env_on[g])
            bus_saa(&e->bus, 0x18 + g, 0);
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 4; n++)
            bus_volume(&e->bus, c, n, volume[c][n]);
}
