#include "studio.h"
#include <math.h>
#include <string.h>
#include <limits.h>
#include <stddef.h>
_Static_assert(sizeof(StSound) == 42 && sizeof(StPart) == 128 && offsetof(StPart, role) == 127,
               "v6 sound and part layout");
const char *const st_role_names[ST_ROLES] = {"FREE", "SAA TONE", "SAA ENV", "SN TONE", "SN NOISE"};
bool studio_role_accepts(unsigned role, unsigned model) {
    if (role >= ST_ROLES || model >= ST_MODELS)
        return false;
    return role == ST_ROLE_ANY || (role == ST_ROLE_SAA_TONE && model <= ST_SAA_MIX) ||
           (role == ST_ROLE_SAA_ENV && studio_is_env(model)) ||
           (role == ST_ROLE_SN_TONE && (model == ST_SN_TWIN || model == ST_SN_STACK)) ||
           (role == ST_ROLE_SN_NOISE && (model == ST_SN_NOISE || model == ST_SN_METAL));
}
const char *const st_model_names[ST_MODELS] = {"SAA TONE",  "SAA NOISE", "SAA MIX",    "SAA ENV",
                                               "SAA AM",    "SN TWIN",   "SN LEFT",    "SN RIGHT",
                                               "SN NOISE",  "SN METAL",  "SN NOISE L", "SN NOISE R",
                                               "ENV NOISE", "ENV MIX",   "SAA LAYER",  "SN STACK"};
const char *const st_param_names[ST_PARAMS] = {
    "OFF",          "WAVE",        "STEPS",       "R INVERT",  "PAN",
    "VIBRATO",      "DETUNE",      "TONE MIX",    "ATTACK",    "DECAY",
    "RELEASE",      "TREMOLO",     "LEVEL",       "PITCH",     "SN BASE ALL",
    "SN DEPTH ALL", "LAYER LEVEL", "LAYER DECAY", "ENV RATIO", "LAYER PITCH"};
static float bound(float x, float a, float b) {
    return x < a ? a : x > b ? b : x;
}
bool studio_is_saa(unsigned m) {
    return m <= ST_SAA_AM || (m >= ST_SAA_ENV_NOISE && m <= ST_SAA_LAYER);
}
bool studio_is_env(unsigned m) {
    return m == ST_SAA_ENV || m == ST_SAA_AM || (m >= ST_SAA_ENV_NOISE && m <= ST_SAA_LAYER);
}
static bool env_model(unsigned m) {
    return studio_is_env(m);
}
static bool noise_model(unsigned m) {
    return m == ST_SAA_NOISE || m == ST_SAA_MIX || (m >= ST_SN_NOISE && m <= ST_SN_NOISE_R) ||
           m == ST_SAA_ENV_NOISE || m == ST_SAA_ENV_MIX;
}
bool studio_is_noise(unsigned m) {
    return noise_model(m);
}
static bool twin(unsigned m) {
    return m == ST_SN_TWIN || m == ST_SN_NOISE || m == ST_SN_METAL || m == ST_SN_STACK;
}
static int mate(int v) {
    return v >= 6 && v <= 8 ? v + 3 : v >= 9 && v <= 11 ? v - 3 : v == 12 ? 13 : 12;
}
static unsigned chip(unsigned v) {
    return v < 6 ? 0 : v < 9 || v == 12 ? 1 : 2;
}
unsigned studio_limit(unsigned m) {
    return m == ST_SN_STACK                                         ? 1
           : env_model(m)                                           ? 2
           : m == ST_SN_TWIN || m == ST_SN_LEFT || m == ST_SN_RIGHT ? 3
           : (m >= ST_SN_NOISE && m <= ST_SN_NOISE_R)               ? 1
                                                                    : 6;
}
void studio_sound_default(StSound *s, unsigned model) {
    memset(s, 0, sizeof(*s));
    s->model = (uint8_t)model;
    s->attack = 3;
    s->decay = 150;
    s->release = 180;
    s->sustain = 75;
    s->level = 65;
    s->pan = 50;
    s->shape = 5;
    s->steps = 16;
    s->retrigger = 1;
    s->noise_rate = 1;
    s->noise_mix = 50;
    s->layer_decay = 80;
    s->layer_level = 35;
    s->layer_source = 1;
    s->layer_interval = 0;
    s->stack_interval = 12;
    s->stack_size = 2;
    s->lfo_rate = 3;
    s->macro_dest[0] = env_model(model) ? ST_WAVE : ST_VIBRATO;
    s->macro_dest[1] = env_model(model) ? ST_STEPSIZE : ST_TREMOLO;
    s->macro_dest[2] = env_model(model) ? ST_INVERT : studio_is_saa(model) ? ST_PAN : ST_DETUNE;
    s->macro_dest[3] = ST_RELEASE;
    if (model >= ST_SN_NOISE && model <= ST_SN_NOISE_R) {
        s->shape = 1;
        s->sustain = 0;
        s->decay = 100;
        s->release = 40;
    }
    if (model == ST_SN_LEFT || model == ST_SN_RIGHT)
        s->macro_dest[2] = ST_ATTACK;
    if (noise_model(model) && !env_model(model)) {
        s->macro_dest[0] = ST_DECAY;
        s->macro_dest[1] = ST_TREMOLO;
        s->macro_dest[2] = twin(model) || model == ST_SAA_NOISE ? ST_PAN : ST_ATTACK;
        s->macro_dest[3] = ST_LEVEL;
    }
    if (model == ST_SAA_LAYER) {
        s->macro_dest[0] = ST_LAYER_LEVEL;
        s->macro_dest[1] = ST_LAYER_DECAY;
        s->macro_dest[2] = ST_PAN;
        s->macro_dest[3] = ST_RELEASE;
    }
    if (model == ST_SN_STACK) {
        s->macro_dest[0] = ST_LAYER_INTERVAL;
        s->macro_dest[1] = ST_DETUNE;
        s->macro_dest[2] = ST_TREMOLO;
        s->macro_dest[3] = ST_RELEASE;
    }
    if (model == ST_SN_METAL) {
        s->macro_dest[0] = ST_NOISE_MIX;
        s->macro_dest[1] = ST_DETUNE;
        s->shape = 0;
        s->noise_rate = 3;
    }
}
void studio_default(StScene *s) {
    memset(s, 0, sizeof(*s));
    memcpy(s->name, "ENV LAB ", 9);
    for (unsigned p = 0; p < ST_PARTS; p++) {
        StPart *a = &s->part[p];
        studio_sound_default(&a->sound, ST_SAA_TONE);
        memcpy(a->name, "PART A  ", 9);
        a->name[5] = (char)('A' + p);
        a->poly = 1;
        a->high = 127;
        a->length = 16;
        a->division = 6;
        a->arp_octaves = 1;
        for (unsigned i = 0; i < ST_STEPS; i++) {
            static const uint8_t notes[] = {48, 55, 60, 55};
            a->note[i] = notes[i % 4];
            a->gate[i] = 70;
            a->velocity[i] = 85;
        }
    }
    const unsigned models[] = {ST_SAA_ENV, ST_SAA_TONE, ST_SN_TWIN, ST_SN_NOISE};
    for (unsigned p = 0; p < 4; p++) {
        StPart *a = &s->part[p];
        a->enabled = 1;
        a->poly = p == 3 ? 1 : 2;
        studio_sound_default(&a->sound, models[p]);
        for (unsigned src = 0; src < 2; src++)
            s->route[src][p] = (uint16_t)(1u << p);
    }
    s->part[3].sound.noise_rate = 3;
    s->part[0].sequence = 1;
    s->route[0][9] = s->route[1][9] = 8;
    s->cv[0] = (StCv){.mode = 1,
                      .part = 0,
                      .base_note = 48,
                      .scale = 1,
                      .gate = 1,
                      .gate_part = 0,
                      .gate_note = 48,
                      .amount = 100};
    s->cv[1] = (StCv){.mode = 2,
                      .part = 2,
                      .base_note = 36,
                      .gate = 4,
                      .gate_part = 2,
                      .gate_note = 36,
                      .parameter = ST_CLOCK_DEPTH,
                      .amount = 100};
    for (unsigned i = 0; i < ST_MODS; i++)
        s->mod[i].amount = 100;
    s->mod[0] = (StMod){3, 0, ST_VIBRATO, 50};
    s->mod[1] = (StMod){4, 0, ST_VIBRATO, 50};
}
void studio_channel_layout(StScene *s) {
    studio_default(s);
    s->enabled = 1;
    memcpy(s->name, "CHIP SET", 9);
    memset(s->route, 0, sizeof(s->route));
    memset(s->mod, 0, sizeof(s->mod));
    for (unsigned p = 0; p < ST_PARTS; p++) {
        s->part[p].enabled = p < 4;
        s->part[p].arp = s->part[p].sequence = 0;
        s->part[p].role = ST_ROLE_ANY;
    }
    const uint8_t sounds[] = {3, 1, 7, 12};
    const uint8_t channels[] = {0, 1, 2, 9};
    const uint8_t roles[] = {ST_ROLE_SAA_TONE, ST_ROLE_SAA_ENV, ST_ROLE_SN_TONE, ST_ROLE_SN_NOISE};
    for (unsigned p = 0; p < 4; p++) {
        StPart *a = &s->part[p];
        studio_factory(sounds[p], &a->sound, a->name);
        a->poly = p == 3 ? 1 : 2;
        a->role = roles[p];
        for (unsigned src = 0; src < 2; src++)
            s->route[src][channels[p]] = (uint16_t)(1u << p);
        s->mod[p * 2] = (StMod){3, (uint8_t)p, p == 3 ? ST_DECAY : ST_VIBRATO, 50};
        s->mod[p * 2 + 1] =
            (StMod){4, (uint8_t)p, p == 3 ? ST_LEVEL : ST_VIBRATO, p == 3 ? 25 : 50};
    }
    for (unsigned i = 0; i < 2; i++)
        s->cv[i].mode = s->cv[i].gate = 0;
}
void studio_factory(unsigned index, StSound *s, char name[9]) {
    static const char names[ST_FACTORIES][9] = {
        "ENV BASS", "ENV LEAD", "STEP SAW", "CHIPLEAD", "SAA PLUK", "AM BELL ", "SOFT PAD",
        "TWINLEAD", "SN BASS ", "DETUNE  ", "DRY SN  ", "METAL   ", "SN SNARE", "HI HAT  ",
        "SAA NOIS", "LASER   ", "ENV HISS", "ENV GRIT", "ATK BASS", "SN STACK"};
    static const uint8_t models[ST_FACTORIES] = {
        ST_SAA_ENV, ST_SAA_ENV,       ST_SAA_ENV,     ST_SAA_TONE,  ST_SAA_TONE,
        ST_SAA_AM,  ST_SAA_TONE,      ST_SN_TWIN,     ST_SN_TWIN,   ST_SN_TWIN,
        ST_SN_TWIN, ST_SN_METAL,      ST_SN_NOISE,    ST_SN_NOISE,  ST_SAA_NOISE,
        ST_SN_TWIN, ST_SAA_ENV_NOISE, ST_SAA_ENV_MIX, ST_SAA_LAYER, ST_SN_STACK};
    index %= ST_FACTORIES;
    studio_sound_default(s, models[index]);
    memcpy(name, names[index], 9);
    switch (index) {
    case 0:
        s->mode = 1;
        s->decay = 180;
        s->sustain = 65;
        s->release = 60;
        break;
    case 1:
        s->vibrato = 12;
        s->release = 110;
        break;
    case 2:
        s->shape = 3;
        s->steps = 8;
        s->decay = 220;
        s->sustain = 55;
        break;
    case 3:
        s->vibrato = 10;
        s->release = 90;
        break;
    case 4:
        s->attack = 1;
        s->decay = 240;
        s->sustain = 0;
        s->release = 100;
        break;
    case 5:
        s->shape = 3;
        s->env_ratio = 36;
        s->decay = 600;
        s->sustain = 0;
        break;
    case 6:
        s->attack = 450;
        s->release = 800;
        s->vibrato = 8;
        break;
    case 7:
        s->detune = 12;
        s->vibrato = 8;
        break;
    case 8:
        s->mode = 1;
        s->glide = 1;
        s->release = 60;
        s->sustain = 85;
        break;
    case 9:
        s->detune = 25;
        s->attack = 15;
        s->release = 300;
        break;
    case 10:
        s->attack = 0;
        s->decay = 0;
        s->sustain = 100;
        s->release = 10;
        break;
    case 11:
        s->decay = 280;
        s->noise_mix = 45;
        break;
    case 12:
        s->decay = 140;
        s->noise_rate = 1;
        break;
    case 13:
        s->decay = 45;
        s->release = 20;
        s->noise_rate = 0;
        break;
    case 14:
        s->attack = 180;
        s->sustain = 60;
        s->release = 420;
        break;
    case 15:
        s->sweep = 36;
        s->decay = 260;
        s->sustain = 0;
        s->release = 80;
        break;
    case 16:
        s->shape = 3;
        s->env_ratio = -24;
        s->decay = 600;
        s->sustain = 0;
        break;
    case 17:
        s->shape = 5;
        s->env_ratio = 7;
        s->noise_rate = 1;
        break;
    case 18:
        s->shape = 5;
        s->layer_source = 1;
        s->layer_decay = 35;
        s->layer_level = 60;
        s->decay = 300;
        s->sustain = 65;
        s->mode = 1;
        break;
    case 19:
        s->layer_interval = 7;
        s->stack_interval = 12;
        s->stack_size = 2;
        s->detune = 8;
        s->mode = 1;
        break;
    }
}
bool studio_sound_valid(const StSound *s) {
    if (s->model >= ST_MODELS || s->shape > 7 || (s->steps != 8 && s->steps != 16) ||
        s->invert > 1 || s->retrigger > 1 || s->noise_rate > 3 || s->noise_mix > 100 ||
        s->attack > 8000 || s->decay > 8000 || s->release > 8000 || s->sustain > 100 ||
        s->level > 100 || s->pan > 100 || s->detune > 50 || s->vibrato > 100 || s->tremolo > 100 ||
        s->lfo_rate > 7 || s->glide > 7 || s->mode > 2 || s->transpose < -48 || s->transpose > 48 ||
        s->fine < -50 || s->fine > 50 || s->sweep < -48 || s->sweep > 48 || s->layer_decay > 8000 ||
        s->layer_interval < -48 || s->layer_interval > 48 || s->env_ratio < -48 ||
        s->env_ratio > 48 || s->stack_interval < -48 || s->stack_interval > 48 ||
        s->layer_level > 100 || s->layer_source > 2 || s->stack_size < 2 || s->stack_size > 3)
        return false;
    for (unsigned i = 0; i < 4; i++)
        if (s->macro_dest[i] >= ST_PARAMS || s->macro[i] < -100 || s->macro[i] > 100)
            return false;
    return true;
}
static bool text_valid(const char s[9]) {
    if (s[8])
        return false;
    for (unsigned i = 0; i < 8; i++)
        if (s[i] < 32 || s[i] > 90)
            return false;
    return true;
}
bool studio_valid(const StScene *s) {
    if (s->enabled > 1)
        return false;
    if (!s->enabled)
        return true;
    if (!text_valid(s->name) || s->gate_clock > 1)
        return false;
    for (unsigned p = 0; p < ST_PARTS; p++) {
        const StPart *a = &s->part[p];
        if (!studio_sound_valid(&a->sound) || !text_valid(a->name) || a->enabled > 1 || !a->poly ||
            !studio_role_accepts(a->role, a->sound.model) ||
            a->poly > studio_limit(a->sound.model) || a->low > a->high || a->high > 127 ||
            !a->length || a->length > 16 || !a->division || a->division > 24 || a->direction > 2 ||
            a->arp > 5 || !a->arp_octaves || a->arp_octaves > 4 || a->lfo_shape > 3 ||
            a->sequence > 1 || a->velocity_curve > 3)
            return false;
        for (unsigned i = 0; i < 16; i++)
            if (a->note[i] > 127 || a->gate[i] > 100 || a->velocity[i] > 127 ||
                a->motion[i] < -100 || a->motion[i] > 100)
                return false;
    }
    for (unsigned src = 0; src < 2; src++)
        for (unsigned c = 0; c < 16; c++)
            if (s->route[src][c] & 0xc000u)
                return false;
    for (unsigned i = 0; i < 2; i++) {
        const StCv *c = &s->cv[i];
        if (c->mode > 2 || c->part >= ST_PARTS || c->base_note > 127 || c->scale >= CV_SCALES ||
            c->root > 11 || c->glide > 7 || c->range > 1 || c->gate > 4 ||
            c->gate_part >= ST_PARTS || c->gate_note > 127 || c->parameter >= ST_PARAMS ||
            c->amount < -100 || c->amount > 100)
            return false;
    }
    if (s->cv[0].mode == 1 && s->cv[1].mode == 1 && s->cv[0].part == s->cv[1].part)
        return false;
    for (unsigned i = 0; i < ST_MODS; i++) {
        const StMod *m = &s->mod[i];
        if (m->source > 4 + ST_PARTS * 2 || m->part >= ST_PARTS || m->parameter >= ST_PARAMS ||
            m->amount < -100 || m->amount > 100)
            return false;
    }
    return studio_plan(s, NULL, NULL, NULL);
}
/* Six SAA outputs are small enough to search exhaustively. Greedy packing can strand
   a noise colour in both groups and reject an otherwise feasible scene. */
static bool saa_search(const StScene *s, const uint8_t task[6], unsigned count, unsigned at,
                       uint16_t used, int colour0, int colour1, uint16_t out[ST_PARTS],
                       uint16_t res[ST_PARTS]) {
    if (at == count)
        return true;
    unsigned p = task[at];
    const StSound *sound = &s->part[p].sound;
    for (unsigned v = 0; v < 6; v++) {
        uint16_t bit = (uint16_t)(1u << v), need = bit;
        if (env_model(sound->model)) {
            if (v != 2 && v != 5)
                continue;
            need |= (uint16_t)(1u << (v - 1));
            if (sound->model == ST_SAA_LAYER)
                need |= (uint16_t)(1u << (v - 2));
        }
        int a = colour0, b = colour1;
        if (noise_model(sound->model) || (sound->model == ST_SAA_LAYER && sound->layer_source)) {
            int colour = v < 3 ? a : b;
            if (colour >= 0 && colour != sound->noise_rate)
                continue;
            if (v < 3)
                a = sound->noise_rate;
            else
                b = sound->noise_rate;
            if (sound->noise_rate == 3)
                need |= (uint16_t)(1u << ((v / 3) * 3));
        }
        if (used & need)
            continue;
        out[p] |= bit;
        res[p] |= need;
        if (saa_search(s, task, count, at + 1, (uint16_t)(used | need), a, b, out, res))
            return true;
        out[p] &= (uint16_t)~bit;
        res[p] &= (uint16_t)~need;
    }
    return false;
}
/* Allocate the restrictive consumers first. A failed plan never changes the live engine. */
bool studio_plan(const StScene *s, uint16_t outputs[ST_PARTS], uint16_t resources[ST_PARTS],
                 unsigned *conflict) {
    uint16_t out[ST_PARTS] = {0}, res[ST_PARTS] = {0}, used = 0;
    uint8_t tasks[6];
    unsigned total = 0;
    for (unsigned pass = 0; pass < 3; pass++)
        for (unsigned p = 0; p < ST_PARTS; p++) {
            const StPart *a = &s->part[p];
            if (!a->enabled)
                continue;
            if (a->sound.model >= ST_MODELS || !a->poly || a->poly > studio_limit(a->sound.model)) {
                if (conflict)
                    *conflict = p;
                return false;
            }
            if (!studio_is_saa(a->sound.model))
                continue;
            unsigned priority = env_model(a->sound.model) ? 0 : noise_model(a->sound.model) ? 1 : 2;
            if (priority != pass)
                continue;
            for (unsigned n = 0; n < a->poly; n++) {
                if (total >= 6) {
                    if (conflict)
                        *conflict = p;
                    return false;
                }
                tasks[total++] = (uint8_t)p;
            }
        }
    if (!saa_search(s, tasks, total, 0, 0, -1, -1, out, res)) {
        if (conflict)
            *conflict = total ? tasks[total - 1] : 0;
        return false;
    }
    for (unsigned p = 0; p < ST_PARTS; p++)
        used |= res[p];
    for (unsigned pass = 0; pass < 3; pass++)
        for (unsigned p = 0; p < ST_PARTS; p++) {
            const StPart *a = &s->part[p];
            const StSound *snd = &a->sound;
            if (!a->enabled)
                continue;
            if (studio_is_saa(snd->model))
                continue;
            unsigned priority =
                env_model(snd->model) || (noise_model(snd->model) && snd->noise_rate == 3) ? 0
                : twin(snd->model)                                                         ? 1
                                                                                           : 2;
            if (priority != pass)
                continue;
            if (snd->model >= ST_MODELS || !a->poly || a->poly > studio_limit(snd->model))
                goto failed;
            for (unsigned n = 0; n < a->poly; n++) {
                bool found = false;
                for (unsigned v = 0; v < 14; v++) {
                    uint16_t audible = (uint16_t)(1u << v), need = audible;
                    unsigned m = snd->model;
                    if (m == ST_SN_STACK) {
                        if (v != 6)
                            continue;
                        audible = snd->stack_size == 3 ? 0x0fc0 : 0x06c0;
                        need = audible;
                    } else if (m == ST_SN_TWIN) {
                        if (v < 6 || v > 8)
                            continue;
                        audible |= (uint16_t)(1u << (v + 3));
                        need = audible;
                    } else if (m == ST_SN_LEFT || m == ST_SN_RIGHT) {
                        if (v < (m == ST_SN_LEFT ? 6u : 9u) || v > (m == ST_SN_LEFT ? 8u : 11u))
                            continue;
                    } else {
                        if ((m == ST_SN_NOISE_R && v != 13) || (m != ST_SN_NOISE_R && v != 12))
                            continue;
                        if (twin(m))
                            audible |= (uint16_t)(1u << 13);
                        need = audible;
                        if (snd->noise_rate == 3 || m == ST_SN_METAL) {
                            if (audible & (1u << 12))
                                need |= (uint16_t)(1u << 8);
                            if (audible & (1u << 13))
                                need |= (uint16_t)(1u << 11);
                        }
                    }
                    if (used & need)
                        continue;
                    used |= need;
                    out[p] |= audible;
                    res[p] |= need;
                    found = true;
                    break;
                }
                if (!found)
                    goto failed;
            }
            continue;
        failed:
            if (conflict)
                *conflict = p;
            return false;
        }
    if (outputs)
        memcpy(outputs, out, sizeof(out));
    if (resources)
        memcpy(resources, res, sizeof(res));
    return true;
}
static void swrite(Engine *e, unsigned r, unsigned value) {
    if (e->saa_cache[r] != (uint8_t)value) {
        e->saa_cache[r] = (uint8_t)value;
        if (e->sink.saa)
            e->sink.saa(e->sink.ctx, (uint8_t)r, (uint8_t)value);
    }
}
static void nwrite(Engine *e, unsigned c, unsigned value) {
    if (e->sink.sn)
        e->sink.sn(e->sink.ctx, (uint8_t)c, (uint8_t)value);
}
void studio_reset(Engine *e) {
    memset(&e->studio, 0, sizeof(e->studio));
    e->studio.random = 0x10997649;
    for (unsigned p = 0; p < ST_PARTS; p++)
        e->studio.pulse[p] = UINT32_MAX;
    for (unsigned p = 0; p < ST_PARTS; p++)
        e->studio.lane_value[p] = 100;
    (void)studio_plan(&e->preset.studio, e->studio.outputs, e->studio.resources, NULL);
    for (unsigned p = 0; p < ST_PARTS; p++)
        e->studio.effective[p] = e->preset.studio.part[p].sound;
}
void studio_activate(Engine *e) {
    if (!e->preset.studio.enabled) {
        studio_default(&e->preset.studio);
        e->preset.studio.enabled = 1;
    }
    studio_reset(e);
}
void studio_stop_part(Engine *e, unsigned part) {
    if (part >= ST_PARTS)
        return;
    for (unsigned v = 0; v < 14; v++) {
        Voice *x = &e->voice[v];
        if (x->active && x->part == part) {
            if (x->source == 2 || x->source == 3)
                e->studio.gate_block[x->source - 2] = true;
            x->active = x->held = x->sustained = false;
            x->pair = 0;
        }
        if (!(e->studio.resources[part] & (1u << v)))
            continue;
        if (v < 6) {
            swrite(e, v, 0);
            if (v == 2 || v == 5)
                swrite(e, 0x18 + (v == 5), 0);
        } else {
            unsigned c = chip(v) - 1, ch = v >= 12 ? 3 : (v - 6) % 3;
            if (e->sn_atten[c][ch] != 15) {
                nwrite(e, c, 0x90u | (ch << 5) | 15u);
                e->sn_atten[c][ch] = 15;
            }
        }
    }
    memset(e->studio.held[part], 0, sizeof(e->studio.held[part]));
    e->studio.due[part] = 0;
    e->studio.pulse[part] = UINT32_MAX;
    if (e->studio.test_on && e->studio.test_part == part)
        e->studio.test_on = false;
}
static void release(Voice *v);
bool studio_apply(Engine *e, const StScene *s) {
    if (!studio_valid(s))
        return false;
    uint16_t changed = 0;
    uint16_t outputs[ST_PARTS], resources[ST_PARTS];
    if (!studio_plan(s, outputs, resources, NULL))
        return false;
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (outputs[p] != e->studio.outputs[p] || resources[p] != e->studio.resources[p] ||
            s->part[p].sound.model != e->preset.studio.part[p].sound.model ||
            s->part[p].sound.noise_rate != e->preset.studio.part[p].sound.noise_rate ||
            s->part[p].sound.mode != e->preset.studio.part[p].sound.mode ||
            s->part[p].sound.layer_source != e->preset.studio.part[p].sound.layer_source)
            changed |= (uint16_t)(1u << p);
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (changed & (1u << p))
            studio_stop_part(e, p);
    for (unsigned i = 0; i < 2; i++) {
        const StCv *old = &e->preset.studio.cv[i], *next = &s->cv[i];
        if (old->mode != next->mode || old->part != next->part || old->gate != next->gate ||
            old->gate_part != next->gate_part) {
            for (unsigned v = 0; v < 14; v++) {
                Voice *x = &e->voice[v];
                if (x->active && (x->source == i + 2 || (old->mode == 1 && x->part == old->part &&
                                                         (x->source == 2 || x->source == 3)))) {
                    release(x);
                    e->studio.gate_block[x->source - 2] = true;
                }
            }
            e->studio.gate_block[i] = true;
            e->studio.cv_valid[i] = false;
        }
    }
    e->preset.studio = *s;
    memcpy(e->studio.outputs, outputs, sizeof(outputs));
    memcpy(e->studio.resources, resources, sizeof(resources));
    return true;
}
unsigned studio_owner(const Engine *e, unsigned v) {
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (e->studio.resources[p] & (1u << v))
            return p;
    return ST_PARTS;
}
unsigned studio_note_count(const Engine *e, unsigned p) {
    unsigned count = 0;
    if (p < ST_PARTS && e->preset.studio.part[p].sound.model == ST_SN_STACK) {
        for (unsigned v = 6; v < 12; v++)
            if (e->voice[v].active && e->voice[v].part == p)
                return 1;
        return 0;
    }
    for (unsigned v = 0; v < 14; v++)
        if (e->voice[v].active && e->voice[v].part == p &&
            (!e->voice[v].pair || v < (unsigned)e->voice[v].pair - 1))
            count++;
    return count;
}
static void drop(Engine *e, unsigned v) {
    unsigned pair = e->voice[v].pair;
    e->voice[v].active = false;
    e->voice[v].pair = 0;
    if (pair) {
        e->voice[pair - 1].active = false;
        e->voice[pair - 1].pair = 0;
    }
}
static void release(Voice *v) {
    if (!v->releasing) {
        v->held = v->sustained = false;
        v->releasing = true;
        v->release_elapsed = 0;
        v->release_level = v->level;
    }
}
static void part_off(Engine *e, unsigned p, unsigned source) {
    for (unsigned v = 0; v < 14; v++)
        if (e->voice[v].active && e->voice[v].part == p && e->voice[v].source == source)
            release(&e->voice[v]);
}
static void begin_voice(Engine *e, unsigned v, unsigned p, unsigned source, unsigned channel,
                        unsigned note, unsigned velocity, uint16_t token, float pitch) {
    const StSound *s = &e->preset.studio.part[p].sound;
    bool old = e->voice[v].active;
    float previous = e->voice[v].pitch;
    drop(e, v);
    e->voice[v] = (Voice){.active = true,
                          .held = true,
                          .source = (uint8_t)source,
                          .channel = (uint8_t)channel,
                          .note = (uint8_t)note,
                          .velocity = (uint8_t)velocity,
                          .token = token,
                          .age = ++e->age,
                          .pitch = (s->glide && old) ? previous : pitch,
                          .part = (uint8_t)p};
    e->studio.pitch_target[v] = pitch;
    if (v == 2 || v == 5) {
        unsigned r = 0x18 + (v == 5);
        if (env_model(s->model) && s->retrigger)
            swrite(e, r, 0);
    }
    if (v >= 12) {
        unsigned c = v - 12, rate = s->model == ST_SN_METAL ? 3 : s->noise_rate;
        unsigned value = (s->shape ? 4u : 0u) | rate;
        nwrite(e, c, 0xe0u | value);
        e->sn_noise[c] = (uint8_t)value;
    }
}
static void on(Engine *e, unsigned p, unsigned source, unsigned channel, unsigned note,
               unsigned velocity, uint16_t token, float pitch) {
    if (p >= ST_PARTS || !e->preset.studio.part[p].enabled)
        return;
    const StPart *a = &e->preset.studio.part[p];
    const StSound *s = &a->sound;
    uint16_t mask = e->studio.outputs[p];
    int best = -1;
    uint32_t oldest = UINT32_MAX;
    if (s->mode)
        for (unsigned v = 0; v < 14; v++)
            if (e->voice[v].active && e->voice[v].part == p &&
                !(source < 2 && e->voice[v].held &&
                  (e->voice[v].source == 2 || e->voice[v].source == 3))) {
                best = (int)v;
                break;
            }
    if (best < 0)
        for (unsigned v = 0; v < 14; v++)
            if (mask & (1u << v)) {
                if (twin(s->model) && chip(v) == 2)
                    continue;
                if (source < 2 && e->voice[v].active && e->voice[v].held &&
                    (e->voice[v].source == 2 || e->voice[v].source == 3))
                    continue;
                if (!e->voice[v].active) {
                    best = (int)v;
                    break;
                }
                uint32_t score = e->voice[v].age + (e->voice[v].held ? 0x40000000u : 0);
                if (score < oldest) {
                    oldest = score;
                    best = (int)v;
                }
            }
    if (best < 0) {
        e->refused++;
        return;
    }
    unsigned v = (unsigned)best;
    if (s->model == ST_SN_STACK) {
        bool keep = s->mode == 2 && e->voice[v].active && !e->voice[v].releasing &&
                    e->voice[v].source == source;
        for (unsigned i = 6; i < 12; i++) {
            if (!(mask & (1u << i)))
                continue;
            if (keep) {
                Voice *x = &e->voice[i];
                x->note = (uint8_t)note;
                x->token = token;
                x->channel = (uint8_t)channel;
                x->velocity = (uint8_t)velocity;
                x->held = true;
                x->sustained = false;
                e->studio.pitch_target[i] = pitch;
                if (!s->glide)
                    x->pitch = pitch;
            } else {
                begin_voice(e, i, p, source, channel, note, velocity, token, pitch);
            }
        }
        for (unsigned i = 6; i < 9; i++)
            if (mask & (1u << i)) {
                e->voice[i].pair = (uint8_t)(i + 4);
                e->voice[i + 3].pair = (uint8_t)(i + 1);
            }
        goto trigger_note;
    }
    if (e->voice[v].active && !e->voice[v].releasing && s->mode == 2 &&
        e->voice[v].source == source) {
        Voice *x = &e->voice[v];
        x->note = (uint8_t)note;
        x->token = token;
        x->channel = (uint8_t)channel;
        x->velocity = (uint8_t)velocity;
        x->held = true;
        x->releasing = false;
        e->studio.pitch_target[v] = pitch;
        if (!s->glide)
            x->pitch = pitch;
        if (x->pair) {
            unsigned r = x->pair - 1;
            e->voice[r] = *x;
            e->voice[r].pair = (uint8_t)(v + 1);
            e->studio.pitch_target[r] = pitch;
        }
        return;
    }
    if (e->voice[v].active)
        e->steals++;
    begin_voice(e, v, p, source, channel, note, velocity, token, pitch);
    if (twin(s->model)) {
        unsigned r = (unsigned)mate((int)v);
        begin_voice(e, r, p, source, channel, note, velocity, token, pitch);
        e->voice[v].pair = (uint8_t)(r + 1);
        e->voice[r].pair = (uint8_t)(v + 1);
    }
trigger_note:;
    unsigned trigger = e->preset.sn_trigger_source;
    bool fire = trigger == 2 ? source == 2
                : trigger == 3
                    ? source == 3
                    : !studio_is_saa(s->model) && (trigger == 0 || (trigger == 1 && source < 2) ||
                                                   (trigger == 4 && source < 2 && channel == 9) ||
                                                   (trigger == 5 && !noise_model(s->model)) ||
                                                   (trigger == 6 && noise_model(s->model)) ||
                                                   (trigger == 7 && (source >= 2 || channel != 9)));
    if (fire)
        e->sn_note_trigger++;
}
void studio_panic(Engine *e, int source) {
    for (unsigned p = 0; p < ST_PARTS; p++)
        for (unsigned i = 0; i < ST_HELD; i++)
            if (source < 0 || e->studio.held[p][i].source == source)
                e->studio.held[p][i].velocity = 0;
    if (source < 0) {
        e->studio.running = e->studio.test_on = false;
        for (unsigned i = 0; i < 2; i++) {
            e->studio.cv_valid[i] = false;
            e->studio.gate_block[i] = true;
        }
    }
}
static void restore_sustain(Engine *e, unsigned p, const StHeld *key) {
    if (!key->sustained)
        return;
    for (unsigned v = 0; v < 14; v++) {
        Voice *x = &e->voice[v];
        if (x->active && x->part == p && x->source == key->source && x->channel == key->channel &&
            x->note == key->note && x->token == key->token) {
            x->held = false;
            x->sustained = true;
        }
    }
}
static void mono_fallback(Engine *e, unsigned p) {
    StHeld *best = NULL;
    for (unsigned i = 0; i < ST_HELD; i++) {
        StHeld *h = &e->studio.held[p][i];
        if (h->velocity && (!best || h->age > best->age))
            best = h;
    }
    if (best) {
        if (e->preset.studio.part[p].sound.mode == 2)
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].part == p &&
                    e->voice[v].source == best->source)
                    e->voice[v].releasing = false;
        on(e, p, best->source, best->channel, best->note, best->velocity, best->token, best->note);
        restore_sustain(e, p, best);
    }
}
static void note_off(Engine *e, unsigned source, unsigned channel, unsigned note) {
    if (e->released[source][channel][note] == e->pressed[source][channel][note])
        return;
    uint16_t token = ++e->released[source][channel][note];
    bool affected[ST_PARTS] = {0};
    for (unsigned p = 0; p < ST_PARTS; p++)
        for (unsigned i = 0; i < ST_HELD; i++) {
            StHeld *h = &e->studio.held[p][i];
            if (h->velocity && h->source == source && h->channel == channel && h->note == note &&
                h->token == token) {
                if (e->channel[source][channel].sustain)
                    h->sustained = true;
                else
                    h->velocity = 0;
            }
        }
    for (unsigned v = 0; v < 14; v++) {
        Voice *x = &e->voice[v];
        if (x->active && x->source == source && x->channel == channel && x->note == note &&
            x->token == token) {
            x->held = false;
            if (e->channel[source][channel].sustain)
                x->sustained = true;
            else {
                release(x);
                affected[x->part] = true;
            }
        }
    }
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (affected[p] && e->preset.studio.part[p].sound.mode && !e->preset.studio.part[p].arp)
            mono_fallback(e, p);
}
void studio_message(Engine *e, unsigned source, const uint8_t *m, unsigned len) {
    unsigned c = m[0] & 15u, kind = m[0] >> 4, a = m[1] & 127u, b = len > 2 ? m[2] & 127u : 0;
    if (kind == 8 || (kind == 9 && !b)) {
        note_off(e, source, c, a);
        return;
    }
    if (kind != 9)
        return;
    uint16_t token = ++e->pressed[source][c][a], route = e->preset.studio.route[source][c];
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (route & (1u << p)) {
            const StPart *part = &e->preset.studio.part[p];
            if (!part->enabled || a < part->low || a > part->high)
                continue;
            unsigned velocity = b;
            if (part->velocity_curve == 1)
                velocity = (unsigned)lroundf(sqrtf((float)b / 127) * 127);
            if (part->velocity_curve == 2)
                velocity = (unsigned)lroundf((float)(b * b) / 127);
            if (part->velocity_curve == 3)
                velocity = 110;
            if (!velocity)
                velocity = 1;
            if (part->sound.mode == 1)
                part_off(e, p, source);
            {
                for (unsigned i = 0; i < ST_HELD; i++)
                    if (!e->studio.held[p][i].velocity) {
                        e->studio.held[p][i] = (StHeld){.token = token,
                                                        .source = (uint8_t)source,
                                                        .channel = (uint8_t)c,
                                                        .note = (uint8_t)a,
                                                        .velocity = (uint8_t)velocity,
                                                        .age = ++e->age};
                        break;
                    }
            }
            if (part->arp)
                e->studio.pulse[p] = UINT32_MAX;
            else
                on(e, p, source, c, a, velocity, token, (float)a);
        }
}
float studio_env_clock(float hz, unsigned shape, unsigned steps) {
    return hz * (float)((shape == 4 || shape == 5) ? steps : steps / 2);
}
static uint32_t random_next(Engine *e) {
    uint32_t x = e->studio.random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    e->studio.random = x;
    return x;
}
static void step_note(Engine *e, unsigned p) {
    const StPart *a = &e->preset.studio.part[p];
    unsigned pos = e->studio.position[p] % a->length;
    if (a->direction == 1)
        pos = a->length - 1 - pos;
    if (a->direction == 2)
        pos = random_next(e) % a->length;
    part_off(e, p, 5);
    e->studio.lane_value[p] = (uint8_t)(a->motion[pos] + 100);
    if (a->gate[pos] && a->velocity[pos]) {
        on(e, p, 5, p, a->note[pos], a->velocity[pos], ++e->studio.serial, (float)a->note[pos]);
        unsigned period = 60000u * a->division / (24u * (e->preset.tempo + 30u));
        e->studio.due[p] =
            e->now + (period * a->gate[pos] / 100u ? period * a->gate[pos] / 100u : 1);
    }
    e->studio.position[p] = (uint8_t)((e->studio.position[p] + 1) % a->length);
}
void studio_cv(Engine *e, unsigned input, float volts, bool gate, bool fresh) {
    if (input >= 2)
        return;
    StRuntime *r = &e->studio;
    const StCv *c = &e->preset.studio.cv[input];
    bool rising = gate && !r->gate[input];
    r->gate[input] = gate;
    if (!gate)
        r->gate_block[input] = false;
    fresh = fresh && isfinite(volts);
    r->cv_valid[input] = fresh;
    if (fresh) {
        r->cv[input] = bound(volts / (c->range ? 10.0f : 5.0f), 0, 1);
        if (c->mode == 1) {
            float pitch = engine_quantize(c->base_note + volts * 12, c->scale, c->root);
            if (c->scale && fabsf((c->base_note + volts * 12) - r->cv_note[input]) <=
                                fabsf((c->base_note + volts * 12) - pitch) + .16f)
                pitch = r->cv_note[input];
            r->cv_note[input] = pitch;
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].held &&
                    (e->voice[v].source == 2 || e->voice[v].source == 3) &&
                    e->voice[v].part == c->part) {
                    r->pitch_target[v] = pitch;
                    if (!c->glide)
                        e->voice[v].pitch = pitch;
                }
        }
    } else {
        part_off(e, c->gate_part, input + 2);
        if (c->mode == 1)
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].part == c->part &&
                    (e->voice[v].source == 2 || e->voice[v].source == 3))
                    release(&e->voice[v]);
    }
    if (!gate) {
        part_off(e, c->gate_part, input + 2);
        return;
    }
    if (r->gate_block[input] || !rising)
        return;
    if (e->preset.clock_policy == 3 && e->preset.studio.gate_clock == input) {
        e->clocks += 6;
        e->clock_running = true;
    }
    if (c->gate == 1 || c->gate == 2) {
        if (!fresh)
            return;
        if (c->gate == 2)
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].part == c->gate_part &&
                    e->voice[v].source == input + 2)
                    drop(e, v);
        float pitch = (float)c->gate_note;
        for (unsigned i = 0; i < 2; i++)
            if (e->preset.studio.cv[i].mode == 1 && e->preset.studio.cv[i].part == c->gate_part &&
                r->cv_valid[i])
                pitch = r->cv_note[i];
        on(e, c->gate_part, input + 2, 0, (unsigned)bound(roundf(pitch), 0, 127), 100, ++r->serial,
           pitch);
    } else if (c->gate == 3 &&
               !(e->preset.clock_policy == 3 && e->preset.studio.gate_clock == input))
        step_note(e, c->gate_part);
    else if (c->gate == 4)
        e->sn_manual_trigger++;
}
void studio_test(Engine *e, unsigned p, unsigned note, unsigned mode) {
    if (p >= ST_PARTS || note > 127 || mode > 3)
        return;
    for (unsigned i = 0; i < ST_PARTS; i++)
        part_off(e, i, 4);
    e->studio.test_on = mode != 0;
    e->studio.test_mode = (uint8_t)mode;
    e->studio.test_part = (uint8_t)p;
    e->studio.test_note = (uint8_t)note;
    if (mode) {
        on(e, p, 4, 0, note, 80, ++e->studio.serial, (float)note);
        e->studio.test_due = e->now + 1000;
        e->studio.test_off = mode == 3 ? UINT32_MAX : e->now + 400;
    }
}
void studio_run(Engine *e, bool run) {
    e->studio.running = run;
    for (unsigned p = 0; p < ST_PARTS; p++) {
        if (!run)
            part_off(e, p, 5);
        else {
            e->studio.position[p] = 0;
            e->studio.pulse[p] = UINT32_MAX;
        }
    }
}
void studio_realtime(Engine *e, uint8_t status, unsigned source) {
    if (source != e->preset.clock_source || e->preset.clock_policy == 2 ||
        e->preset.clock_policy == 3)
        return;
    if (status == 0xfa)
        studio_run(e, true);
    if (status == 0xfb)
        e->studio.running = true;
    if (status == 0xfc) {
        studio_run(e, false);
        for (unsigned p = 0; p < ST_PARTS; p++)
            if (e->preset.studio.part[p].arp)
                for (unsigned v = 0; v < 14; v++)
                    if (e->voice[v].active && e->voice[v].part == p && e->voice[v].source < 2)
                        release(&e->voice[v]);
    }
}
static void arp_tick(Engine *e, unsigned p) {
    const StPart *a = &e->preset.studio.part[p];
    StHeld *h = e->studio.held[p];
    unsigned order[ST_HELD], count = 0;
    for (unsigned i = 0; i < ST_HELD; i++)
        if (h[i].velocity)
            order[count++] = i;
    if (!count)
        return;
    for (unsigned i = 1; i < count; i++)
        for (unsigned j = i; j && h[order[j]].note < h[order[j - 1]].note; j--) {
            unsigned x = order[j];
            order[j] = order[j - 1];
            order[j - 1] = x;
        }
    for (unsigned v = 0; v < 14; v++)
        if (e->voice[v].active && e->voice[v].part == p && e->voice[v].source < 2)
            release(&e->voice[v]);
    unsigned pos = e->studio.arp_position[p]++, span = count * a->arp_octaves, index = pos % span;
    if (a->arp == 2)
        index = span - 1 - index;
    if (a->arp == 3 && span > 1) {
        unsigned cycle = 2 * (span - 1);
        index = pos % cycle;
        if (index >= span)
            index = cycle - index;
    }
    if (a->arp == 4)
        index = random_next(e) % span;
    unsigned chord = a->arp == 5 ? count : 1;
    for (unsigned n = 0; n < chord; n++) {
        unsigned i = a->arp == 5 ? n : index % count;
        StHeld *key = &h[order[i]];
        on(e, p, key->source, key->channel, key->note, key->velocity, key->token,
           (float)key->note + 12.0f * (float)(index / count));
        restore_sustain(e, p, key);
    }
}
static float lfo(Engine *e, unsigned p) {
    return e->studio.lfo_value[p];
}
static void parameter(StSound *s, unsigned dest, float amount, float *pitch, float *base,
                      float *depth) {
    if (amount == 0 || dest == ST_NONE)
        return;
    switch (dest) {
    case ST_LAYER_LEVEL:
        s->layer_level = (uint8_t)bound(roundf(s->layer_level + amount * 100), 0, 100);
        break;
    case ST_LAYER_DECAY:
        s->layer_decay = (uint16_t)bound(s->layer_decay * exp2f(amount * 4), 0, 8000);
        break;
    case ST_ENV_RATIO:
        s->env_ratio = (int8_t)bound(roundf(s->env_ratio + amount * 48), -48, 48);
        break;
    case ST_LAYER_INTERVAL:
        s->layer_interval = (int8_t)bound(roundf(s->layer_interval + amount * 24), -48, 48);
        break;
    case ST_WAVE:
        if (!studio_is_saa(s->model) && noise_model(s->model)) {
            if (amount > .5f)
                s->shape = 1;
            else if (amount < -.5f)
                s->shape = 0;
        } else
            s->shape = (uint8_t)bound(roundf(s->shape + amount * 7), 0, 7);
        break;
    case ST_STEPSIZE:
        if (amount > .5f)
            s->steps = 8;
        else if (amount < -.5f)
            s->steps = 16;
        break;
    case ST_INVERT:
        if (amount > .5f)
            s->invert = 1;
        else if (amount < -.5f)
            s->invert = 0;
        break;
    case ST_PAN:
        s->pan = (uint8_t)bound(s->pan + amount * 100, 0, 100);
        break;
    case ST_VIBRATO:
        s->vibrato = (uint8_t)bound(s->vibrato + amount * 100, 0, 100);
        break;
    case ST_DETUNE:
        s->detune = (uint8_t)bound(s->detune + amount * 50, 0, 50);
        break;
    case ST_NOISE_MIX:
        s->noise_mix = (uint8_t)bound(s->noise_mix + amount * 100, 0, 100);
        break;
    case ST_ATTACK:
        s->attack = (uint16_t)bound(s->attack * exp2f(amount * 4), 0, 8000);
        break;
    case ST_DECAY:
        s->decay = (uint16_t)bound(s->decay * exp2f(amount * 4), 0, 8000);
        break;
    case ST_RELEASE:
        s->release = (uint16_t)bound(s->release * exp2f(amount * 4), 0, 8000);
        break;
    case ST_TREMOLO:
        s->tremolo = (uint8_t)bound(s->tremolo + amount * 100, 0, 100);
        break;
    case ST_LEVEL:
        s->level = (uint8_t)bound(s->level + amount * 100, 0, 100);
        break;
    case ST_PITCH:
        *pitch += amount * 24;
        break;
    case ST_CLOCK_BASE:
        *base += amount * 48;
        break;
    case ST_CLOCK_DEPTH:
        *depth += amount * 48;
        break;
    default:
        break;
    }
}
bool studio_is_single(const Engine *e, unsigned part) {
    if (part >= ST_PARTS || !e->preset.studio.part[part].enabled)
        return false;
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (p != part && e->preset.studio.part[p].enabled)
            return false;
    return true;
}
bool studio_sound_load(Engine *e, unsigned part, const StSound *sound, const char name[9]) {
    if (part >= ST_PARTS || !studio_sound_valid(sound) || !text_valid(name))
        return false;
    static StScene candidate;
    candidate = e->preset.studio;
    StPart *p = &candidate.part[part];
    if (!studio_role_accepts(p->role, sound->model))
        return false;
    p->sound = *sound;
    memcpy(p->name, name, 9);
    unsigned maximum = studio_limit(sound->model);
    if (sound->mode && p->role == ST_ROLE_ANY)
        maximum = 1;
    if (!studio_is_single(e, part) && maximum > p->poly)
        maximum = p->poly;
    p->poly = (uint8_t)maximum;
    while (!studio_plan(&candidate, NULL, NULL, NULL)) {
        if (p->poly <= 1)
            return false;
        p->poly--;
    }
    return studio_apply(e, &candidate);
}
bool studio_single(Engine *e, unsigned part, unsigned channel) {
    if (part >= ST_PARTS || channel >= 16)
        return false;
    static StScene candidate;
    candidate = e->preset.studio;
    candidate.enabled = 1;
    memcpy(candidate.name, "SINGLE  ", 9);
    for (unsigned p = 0; p < ST_PARTS; p++) {
        candidate.part[p].enabled = p == part;
        candidate.part[p].arp = candidate.part[p].sequence = 0;
    }
    StPart *p = &candidate.part[part];
    p->role = ST_ROLE_ANY;
    p->poly = (uint8_t)(p->sound.mode ? 1 : studio_limit(p->sound.model));
    p->low = 0;
    p->high = 127;
    memset(candidate.route, 0, sizeof(candidate.route));
    for (unsigned src = 0; src < 2; src++)
        candidate.route[src][channel] = (uint16_t)(1u << part);
    for (unsigned i = 0; i < 2; i++) {
        candidate.cv[i].mode = candidate.cv[i].gate = 0;
        candidate.cv[i].part = candidate.cv[i].gate_part = (uint8_t)part;
    }
    memset(candidate.mod, 0, sizeof(candidate.mod));
    candidate.mod[0] = (StMod){3, (uint8_t)part, ST_VIBRATO, 50};
    candidate.mod[1] = (StMod){4, (uint8_t)part, ST_VIBRATO, 50};
    if (!studio_apply(e, &candidate))
        return false;
    engine_panic(e, -1);
    memset(e->studio.mute, 0, sizeof(e->studio.mute));
    return true;
}
void studio_program(Engine *e, unsigned source, unsigned channel, unsigned program) {
    if (!e->library || source >= SOURCES || channel >= 16 || program >= ST_SOUNDS ||
        !(e->library->sound_used & (1u << program)))
        return;
    /* Keep multi-part Program Change atomic: reject the whole assignment if the
       new sounds cannot share the available hardware resources. */
    static StScene candidate;
    candidate = e->preset.studio;
    uint16_t route = candidate.route[source][channel];
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (route & (1u << p)) {
            if (!studio_role_accepts(candidate.part[p].role, e->library->sounds[program].model)) {
                e->refused++;
                return;
            }
            candidate.part[p].sound = e->library->sounds[program];
            unsigned maximum = studio_limit(candidate.part[p].sound.model);
            if (candidate.part[p].sound.mode && candidate.part[p].role == ST_ROLE_ANY)
                maximum = 1;
            if (studio_is_single(e, p) || candidate.part[p].poly > maximum)
                candidate.part[p].poly = (uint8_t)maximum;
            memcpy(candidate.part[p].name, e->library->sound_name[program], 9);
        }
    if (!studio_apply(e, &candidate))
        e->refused++;
}
void studio_control(Engine *e, unsigned source, unsigned channel, unsigned cc, unsigned value) {
    if (cc == 120 || cc == 123)
        for (unsigned p = 0; p < ST_PARTS; p++)
            for (unsigned i = 0; i < ST_HELD; i++)
                if (e->studio.held[p][i].source == source &&
                    e->studio.held[p][i].channel == channel)
                    e->studio.held[p][i].velocity = 0;
    if (cc == 64 && value < 64)
        for (unsigned p = 0; p < ST_PARTS; p++)
            for (unsigned i = 0; i < ST_HELD; i++) {
                StHeld *h = &e->studio.held[p][i];
                if (h->source == source && h->channel == channel && h->sustained)
                    h->velocity = 0;
            }
    if (cc >= 16 && cc <= 19) {
        uint16_t route = e->preset.studio.route[source][channel];
        for (unsigned p = 0; p < ST_PARTS; p++)
            if (route & (1u << p))
                e->preset.studio.part[p].sound.macro[cc - 16] =
                    (int8_t)((int)(value * 200 / 127) - 100);
    }
}
static float source_value(Engine *e, const StMod *m) {
    if (m->source == 1 || m->source == 2)
        return e->studio.cv_valid[m->source - 1] ? e->studio.cv[m->source - 1] : 0;
    if (m->source == 3 || m->source == 4) {
        float value = 0;
        for (unsigned src = 0; src < 2; src++)
            for (unsigned ch = 0; ch < 16; ch++)
                if (e->preset.studio.route[src][ch] & (1u << m->part)) {
                    const Channel *c = &e->channel[src][ch];
                    float x = (m->source == 3 ? c->modulation : c->pressure) / 127.0f;
                    if (x > value)
                        value = x;
                }
        if (m->source == 4)
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].part == m->part) {
                    float x = e->voice[v].pressure / 127.0f;
                    if (x > value)
                        value = x;
                }
        return value;
    }
    if (m->source >= 5 && m->source < 5 + ST_PARTS)
        return lfo(e, m->source - 5);
    if (m->source >= 5 + ST_PARTS && m->source < 5 + ST_PARTS * 2)
        return ((int)e->studio.lane_value[m->source - 5 - ST_PARTS] - 100) / 100.0f;
    return 0;
}
static void attenuation(Engine *e, unsigned c, unsigned channel, float gain) {
    uint8_t value = sn_attenuation(gain);
    if (e->sn_atten[c][channel] != value) {
        nwrite(e, c, 0x90u | (channel << 5) | value);
        e->sn_atten[c][channel] = value;
    }
}
static void divisor(Engine *e, unsigned c, unsigned channel, float hz, bool due, bool *clamped) {
    uint16_t value = sn_divider_clock(hz, engine_sn_reference(&e->preset), clamped);
    if (value != e->sn_div[c][channel] && due) {
        nwrite(e, c, 0x80u | (channel << 5) | (value & 15));
        nwrite(e, c, (value >> 4) & 63u);
        e->sn_div[c][channel] = value;
    }
}
static void frequency(Engine *e, unsigned v, float hz, uint8_t oct[3], bool *clamped) {
    uint8_t o, f;
    saa_frequency(hz, &o, &f, clamped);
    swrite(e, 8 + v, f);
    unsigned shift = (v & 1u) * 4;
    oct[v / 2] = (uint8_t)((oct[v / 2] & ~(15u << shift)) | ((unsigned)o << shift));
}
static float amp(Voice *v, const StSound *s, unsigned dt) {
    v->elapsed += dt;
    if (v->releasing) {
        v->release_elapsed += dt;
        if (!s->release || v->release_elapsed >= s->release) {
            v->active = false;
            return 0;
        }
        return v->release_level * (1 - (float)v->release_elapsed / s->release);
    }
    if (s->attack && v->elapsed < s->attack)
        return (float)v->elapsed / s->attack;
    unsigned t = v->elapsed - s->attack;
    if (s->decay && t < s->decay)
        return 1 - (1 - s->sustain / 100.0f) * (float)t / s->decay;
    if (!s->sustain)
        v->active = false;
    return s->sustain / 100.0f;
}
void studio_tick(Engine *e, uint32_t dt) {
    StRuntime *r = &e->studio;
    StScene *scene = &e->preset.studio;
    static const float rates[] = {.5f, 1, 2, 4, 8, 16, 32, 64};
    static const unsigned glides[] = {0, 10, 30, 60, 120, 250, 500, 1000};
    if (r->test_on) {
        if (r->test_mode != 3 && r->test_off && (int32_t)(e->now - r->test_off) >= 0) {
            part_off(e, r->test_part, 4);
            r->test_off = 0;
            if (r->test_mode == 1)
                r->test_on = false;
        }
        if (r->test_mode == 2 && (int32_t)(e->now - r->test_due) >= 0) {
            on(e, r->test_part, 4, 0, r->test_note, 80, ++r->serial, r->test_note);
            r->test_due = e->now + 1000;
            r->test_off = e->now + 400;
        }
    }
    uint32_t clocks = engine_transport_clocks(e);
    for (unsigned p = 0; p < ST_PARTS; p++) {
        StPart *a = &scene->part[p];
        if (!a->enabled)
            continue;
        float phase = r->phase[p] + (float)dt * rates[a->sound.lfo_rate] / 1000;
        if (phase >= 1) {
            phase -= floorf(phase);
            r->lfo_sample[p] = (float)(random_next(e) & 65535) / 32767.5f - 1;
        }
        r->phase[p] = phase;
        r->lfo_value[p] = a->lfo_shape == 0   ? sinf(phase * 6.2831853f)
                          : a->lfo_shape == 1 ? 1 - 4 * fabsf(phase - .5f)
                          : a->lfo_shape == 2 ? (phase < .5f ? 1.0f : -1.0f)
                                              : r->lfo_sample[p];
        if (r->due[p] && (int32_t)(e->now - r->due[p]) >= 0) {
            part_off(e, p, 5);
            r->due[p] = 0;
        }
        bool gated = false;
        for (unsigned i = 0; i < 2; i++)
            if (scene->cv[i].gate == 3 && scene->cv[i].gate_part == p &&
                !(e->preset.clock_policy == 3 && scene->gate_clock == i))
                gated = true;
        uint32_t pulse = clocks / a->division;
        if (pulse != r->pulse[p] && e->clock_running) {
            if (r->running && a->sequence && !gated)
                step_note(e, p);
            if (a->arp)
                arp_tick(e, p);
            r->pulse[p] = pulse;
        }
    }
    StSound *sound = r->effective;
    float pitch_mod[ST_PARTS] = {0};
    r->clock_base = r->clock_depth = 0;
    for (unsigned p = 0; p < ST_PARTS; p++) {
        sound[p] = scene->part[p].sound;
        if (!scene->part[p].enabled)
            continue;
        for (unsigned m = 0; m < 4; m++)
            parameter(&sound[p], sound[p].macro_dest[m], sound[p].macro[m] / 100.0f, &pitch_mod[p],
                      &r->clock_base, &r->clock_depth);
        if (scene->part[p].sequence && r->running)
            parameter(&sound[p], sound[p].macro_dest[0], ((int)r->lane_value[p] - 100) / 100.0f,
                      &pitch_mod[p], &r->clock_base, &r->clock_depth);
    }
    for (unsigned i = 0; i < 2; i++) {
        const StCv *c = &scene->cv[i];
        if (c->mode == 2 && r->cv_valid[i])
            parameter(&sound[c->part], c->parameter, r->cv[i] * c->amount / 100.0f,
                      &pitch_mod[c->part], &r->clock_base, &r->clock_depth);
    }
    for (unsigned i = 0; i < ST_MODS; i++) {
        const StMod *m = &scene->mod[i];
        if (m->source && m->parameter)
            parameter(&sound[m->part], m->parameter, source_value(e, m) * m->amount / 100.0f,
                      &pitch_mod[m->part], &r->clock_base, &r->clock_depth);
    }
    uint8_t tone = 0, noise = 0, colour = 0, amplitude[6] = {0}, oct[3];
    for (unsigned i = 0; i < 3; i++)
        oct[i] = e->saa_cache[0x10 + i] == 255 ? 0 : e->saa_cache[0x10 + i];
    bool sn_used[2][4] = {{false}}, due = e->now - e->sn_frequency_updated >= 4;
    SnClockConfig cfg = engine_effective_clock(e);
    if (sn_clock_write_us(&cfg) <= 40)
        due = true;
    if (due)
        e->sn_frequency_updated = e->now;
    for (unsigned v = 0; v < 14; v++) {
        Voice *x = &e->voice[v];
        if (!x->active || x->part >= ST_PARTS)
            continue;
        unsigned p = x->part;
        StSound s = sound[p];
        Channel *c = x->source < 2 ? &e->channel[x->source][x->channel] : NULL;
        if (c) {
            s.attack =
                (uint16_t)bound(s.attack * (float)engine_time_scale(c->attack) / 256, 0, 8000);
            s.decay = (uint16_t)bound(s.decay * (float)engine_time_scale(c->decay) / 256, 0, 8000);
            s.release =
                (uint16_t)bound(s.release * (float)engine_time_scale(c->release) / 256, 0, 8000);
            if (c->envelope_sustain != 127)
                s.sustain = c->envelope_sustain;
        }
        x->level = amp(x, &s, dt);
        if (!x->active)
            continue;
        unsigned glide = s.glide;
        if (x->source == 2 || x->source == 3)
            for (unsigned input = 0; input < 2; input++)
                if (scene->cv[input].mode == 1 && scene->cv[input].part == p && r->cv_valid[input])
                    glide = scene->cv[input].glide;
        if (glide)
            x->pitch +=
                (r->pitch_target[v] - x->pitch) * (float)dt / ((float)glides[glide] + (float)dt);
        else
            x->pitch = r->pitch_target[v];
        float pitch = x->pitch + s.transpose + s.fine / 100.0f + pitch_mod[p];
        if (c)
            pitch += c->bend / 8192.0f * (c->bend_range + c->bend_cents / 100.0f);
        float sweep = 1 - bound((float)x->elapsed / (s.decay ? s.decay : 1), 0, 1);
        pitch += s.sweep * sweep;
        float depth = s.vibrato / 100.0f;
        pitch += depth * lfo(e, p);
        if (x->pair)
            pitch += (chip(v) == 2 ? 1.0f : -1.0f) * s.detune / 200.0f;
        if (s.model == ST_SN_STACK) {
            unsigned n = (v - 6) % 3;
            pitch += n == 1 ? s.layer_interval : n == 2 ? s.stack_interval : 0;
        }
        float hz = 440 * exp2f((pitch - 69) / 12),
              gain = x->level * x->velocity / 127.0f * s.level / 100.0f;
        if (s.model == ST_SN_STACK)
            gain /= s.stack_size;
        gain *= e->preset.mix[0] / 100.0f * e->preset.mix[chip(v) + 1] / 100.0f;
        if (c)
            gain *= c->volume / 127.0f * c->expression / 127.0f;
        gain *= 1 - s.tremolo / 100.0f * (.5f + .5f * lfo(e, p));
        if (r->mute[p])
            gain = 0;
        float pan = bound(s.pan / 100.0f + (c ? (c->pan - 64) / 127.0f : 0), 0, 1);
        bool clamped = false;
        if (v < 6) {
            if (env_model(s.model)) {
                unsigned buddy = v - 1;
                float clock = (s.model == ST_SAA_ENV || s.model == ST_SAA_LAYER)
                                  ? studio_env_clock(hz, s.shape, s.steps)
                                  : hz * exp2f((float)s.env_ratio / 12);
                frequency(e, buddy, clock, oct, &clamped);
                if (s.model == ST_SAA_AM || s.model == ST_SAA_ENV_MIX) {
                    bool carrier;
                    frequency(e, v, hz, oct, &carrier);
                    clamped |= carrier;
                    tone |= (uint8_t)(1u << v);
                }
                if (s.model == ST_SAA_ENV_NOISE || s.model == ST_SAA_ENV_MIX) {
                    noise |= (uint8_t)(1u << v);
                    colour |= (uint8_t)(s.noise_rate << ((v / 3) * 4));
                    if (s.noise_rate == 3)
                        frequency(e, (v / 3) * 3, hz, oct, &clamped);
                }
                if (s.model == ST_SAA_LAYER) {
                    unsigned aux = v - 2;
                    float layer_hz = hz * exp2f((float)s.layer_interval / 12);
                    bool layer_clamped = false;
                    if (s.layer_source != 1 || s.noise_rate == 3)
                        frequency(e, aux, layer_hz, oct, &layer_clamped);
                    if (s.layer_source != 1)
                        tone |= (uint8_t)(1u << aux);
                    if (s.layer_source) {
                        noise |= (uint8_t)(1u << aux);
                        colour |= (uint8_t)(s.noise_rate << ((v / 3) * 4));
                    }
                    float decay = s.layer_decay ? expf(-(float)x->elapsed / s.layer_decay) : 1;
                    float layer_gain =
                        (clamped || layer_clamped ? 0 : gain) * s.layer_level / 100.0f * decay;
                    unsigned l =
                        (unsigned)bound(roundf(15 * layer_gain * fminf(1, 2 * (1 - pan))), 0, 15);
                    unsigned rr =
                        (unsigned)bound(roundf(15 * layer_gain * fminf(1, 2 * pan)), 0, 15);
                    amplitude[aux] = (uint8_t)((rr << 4) | l);
                }
                unsigned env = 0x80u | (s.shape << 1) | (s.steps == 8 ? 0x10u : 0u) | s.invert;
                swrite(e, 0x18 + (v == 5), env);
                if ((s.model == ST_SAA_ENV || s.model == ST_SAA_LAYER) && clamped)
                    gain = 0;
            } else {
                frequency(e, v, hz, oct, &clamped);
                if (s.model != ST_SAA_NOISE)
                    tone |= (uint8_t)(1u << v);
                if (noise_model(s.model)) {
                    noise |= (uint8_t)(1u << v);
                    colour |= (uint8_t)(s.noise_rate << ((v / 3) * 4));
                    if (s.noise_rate == 3)
                        frequency(e, (v / 3) * 3, hz, oct, &clamped);
                }
            }
            unsigned left = (unsigned)bound(roundf(15 * gain * fminf(1, 2 * (1 - pan))), 0, 15),
                     right = (unsigned)bound(roundf(15 * gain * fminf(1, 2 * pan)), 0, 15);
            amplitude[v] = (uint8_t)((right << 4) | left);
        } else {
            unsigned ch = chip(v) - 1, channel = v >= 12 ? 3 : (v - 6) % 3;
            sn_used[ch][channel] = true;
            if (x->pair)
                gain *= fminf(1, 2 * (ch ? pan : 1 - pan));
            if (channel < 3)
                divisor(e, ch, channel, hz, due || x->elapsed == dt, &clamped);
            else {
                unsigned rate = s.model == ST_SN_METAL ? 3 : s.noise_rate,
                         value = (s.shape ? 4u : 0u) | rate;
                if (e->sn_noise[ch] != value) {
                    nwrite(e, ch, 0xe0u | value);
                    e->sn_noise[ch] = (uint8_t)value;
                }
                if (rate == 3)
                    divisor(e, ch, 2, sn_noise_clock_hz(hz, s.shape != 0), due || x->elapsed == dt,
                            &clamped);
                if (s.model == ST_SN_METAL) {
                    sn_used[ch][2] = true;
                    attenuation(e, ch, 2, gain * s.noise_mix / 100.0f);
                    gain *= 1 - s.noise_mix / 100.0f;
                }
            }
            attenuation(e, ch, channel, gain);
        }
        if (clamped && x->elapsed == dt)
            e->pitch_clamps++;
    }
    for (unsigned v = 0; v < 6; v++)
        swrite(e, v, amplitude[v]);
    for (unsigned i = 0; i < 3; i++)
        swrite(e, 0x10 + i, oct[i]);
    swrite(e, 0x14, tone);
    swrite(e, 0x15, noise);
    swrite(e, 0x16, colour);
    for (unsigned group = 0; group < 2; group++) {
        unsigned v = group ? 5 : 2, owner = studio_owner(e, v);
        if (owner >= ST_PARTS || !env_model(scene->part[owner].sound.model))
            swrite(e, 0x18 + group, 0);
        else if (!e->voice[v].active && scene->part[owner].sound.retrigger)
            swrite(e, 0x18 + group, 0);
    }
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 4; n++)
            if (!sn_used[c][n])
                attenuation(e, c, n, 0);
}

void studio_profile(StScene *s, unsigned profile) {
    studio_channel_layout(s);
    if (profile == 1) {
        s->part[0].poly = 6;
        s->part[1].enabled = 0;
        memcpy(s->name, "SAA POLY", 9);
    } else if (profile == 2) {
        s->part[0].role = ST_ROLE_SAA_ENV;
        studio_factory(18, &s->part[0].sound, s->part[0].name);
        s->part[0].sound.mode = 0;
        s->part[0].poly = 2;
        s->part[1].enabled = 0;
        memcpy(s->name, "DUAL LYR", 9);
    } else if (profile == 3) {
        s->part[1].poly = 1;
        s->part[4].enabled = 1;
        s->part[4].poly = 2;
        s->part[4].role = ST_ROLE_SAA_TONE;
        studio_factory(14, &s->part[4].sound, s->part[4].name);
        s->part[4].sound.attack = 0;
        s->part[4].sound.decay = 100;
        s->part[4].sound.sustain = 0;
        s->part[4].sound.release = 30;
        for (unsigned src = 0; src < 2; src++)
            s->route[src][10] = 1u << 4;
        memcpy(s->name, "PERC SET", 9);
    }
    for (unsigned src = 0; src < 2; src++)
        for (unsigned c = 0; c < 16; c++)
            for (unsigned p = 0; p < ST_PARTS; p++)
                if (!s->part[p].enabled)
                    s->route[src][c] &= (uint16_t) ~(1u << p);
}
