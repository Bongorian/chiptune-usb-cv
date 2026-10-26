#include "ui.h"
#include "editor.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint8_t glyph[36][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4b, 0x31},
    {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1e},
    {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c},
    {0x7f, 0x49, 0x49, 0x49, 0x41}, {0x7f, 0x09, 0x09, 0x09, 0x01},
    {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01},
    {0x7f, 0x08, 0x14, 0x22, 0x41}, {0x7f, 0x40, 0x40, 0x40, 0x40},
    {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06},
    {0x3e, 0x41, 0x51, 0x21, 0x5e}, {0x7f, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f},
    {0x3f, 0x40, 0x38, 0x40, 0x3f}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
static void pixel(Ui *u, int x, int y, bool white) {
    if (x < 0 || x > 127 || y < 0 || y > 63)
        return;
    uint8_t *p = &u->frame[(y / 8) * 128 + x];
    uint8_t mask = (uint8_t)(1u << (y % 8));
    if (white)
        *p |= mask;
    else
        *p &= (uint8_t)~mask;
}
static void box(Ui *u, int x, int y, int w, int h, bool white) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            pixel(u, xx, yy, white);
}
static void line(Ui *u, int x, int y, int x1, int y1) {
    int dx = abs(x1 - x), sx = x < x1 ? 1 : -1, dy = -abs(y1 - y), sy = y < y1 ? 1 : -1,
        err = dx + dy;
    for (;;) {
        pixel(u, x, y, true);
        if (x == x1 && y == y1)
            break;
        int e = 2 * err;
        if (e >= dy) {
            err += dy;
            x += sx;
        }
        if (e <= dx) {
            err += dx;
            y += sy;
        }
    }
}
static void text(Ui *u, int x, int y, const char *s, int scale, bool white) {
    for (; *s && x < 128; s++, x += 6 * scale) {
        uint8_t g[5] = {0};
        char c = *s;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c >= '0' && c <= '9')
            memcpy(g, glyph[c - '0'], 5);
        else if (c >= 'A' && c <= 'Z')
            memcpy(g, glyph[10 + c - 'A'], 5);
        else if (c == '-')
            memset(g, 8, 5);
        else if (c == '+') {
            g[1] = 8;
            g[2] = 62;
            g[3] = 8;
        } else if (c == '%') {
            g[0] = 0x63;
            g[1] = 0x13;
            g[2] = 8;
            g[3] = 0x64;
            g[4] = 0x63;
        } else if (c == ':')
            g[2] = 20;
        else if (c == '.')
            g[2] = 64;
        else if (c == '*') {
            g[0] = 20;
            g[1] = 8;
            g[2] = 62;
            g[3] = 8;
            g[4] = 20;
        } else if (c == '/') {
            g[0] = 64;
            g[1] = 32;
            g[2] = 16;
            g[3] = 8;
            g[4] = 4;
        } else if (c == '>') {
            g[1] = 65;
            g[2] = 34;
            g[3] = 20;
            g[4] = 8;
        } else if (c == '?') {
            g[0] = 2;
            g[1] = 1;
            g[2] = 81;
            g[3] = 9;
            g[4] = 6;
        } else if (c == '#') {
            g[0] = 20;
            g[1] = 127;
            g[2] = 20;
            g[3] = 127;
            g[4] = 20;
        }
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] & (1u << row))
                    box(u, x + col * scale, y + row * scale, scale, scale, white);
    }
}
typedef enum {
    ITEM_U8,
    ITEM_I8,
    ITEM_U16,
    ITEM_U32,
    ITEM_I32,
    ITEM_HZ,
    ITEM_LINK,
    ITEM_ACTION
} ItemType;
typedef struct {
    const char *label;
    void *value;
    int64_t min, max, step;
    unsigned type, key;
    const char *const *names;
} Item;
/* Foreground UI only: one reusable menu buffer keeps resource allocation and
   persistence calls well clear of the 4 KiB interrupt-shared stack. */
static Item items[48];
enum {
    ACT_PATCH = 1,
    ACT_SAVE,
    ACT_LOAD_PROJECT,
    ACT_LOAD_TRACK,
    ACT_PANIC,
    ACT_BOOT,
    ACT_CAL0,
    ACT_CAL1,
    ACT_CAL2,
    ACT_CAL3,
    ACT_AUDITION,
    ACT_NAME,
    ACT_POLY_FOCUS,
    ACT_DIRECT10,
    ACT_CLOCK_TRIGGER
};
static const char *const mode_names[] = {"DIRECT", "POLY", "LEGATO"};
static const char *const chips[] = {"SAA1099", "SN76489"};
static const char *const sources[] = {"TONE", "NOISE", "TONE+NOISE"};
static const char *const sn_sources[] = {"TONE", "NOISE", "TONE3+NOISE"};
static const char *const noise_types[] = {"PERIODIC", "WHITE"};
static const char *const envelopes[] = {"OFF", "OSCILLATOR", "AM"};
static const uint8_t wave_choice[2][5] = {
    {WAVE_SQUARE, WAVE_ENV, WAVE_TONE_NOISE, WAVE_NOISE, WAVE_AM},
    {WAVE_SQUARE, WAVE_PULSE, WAVE_NOISE, WAVE_METAL}};
static const char *const saa_waves[] = {"SQUARE 50%", "ENV WAVE", "TONE + NOISE", "NOISE",
                                        "SAA AM"};
static const char *const sn_waves[] = {"SQUARE 50%", "PULSE 6.7%", "NOISE", "SN METAL"};
static const char *const outputs[] = {"L/R + PAN", "LEFT", "RIGHT"};
static const char *const yesno[] = {"OFF", "ON"};
static const char *const rates[] = {"FAST", "MID", "SLOW", "KEY TRACK"};
static const char *const shapes[] = {"ZERO",     "MAX",      "DECAY ONCE", "SAW DOWN",
                                     "TRI ONCE", "TRIANGLE", "RISE ONCE",  "SAW UP"};
static const char *const lfos[] = {"SINE", "TRIANGLE", "SQUARE", "RANDOM"};
static const char *const clock_modes[] = {
    "OFF",       "MANUAL",  "RISE",  "TRIANGLE",    "SQUARE",      "RANDOM", "SEQUENCE",
    "DIVE",      "SINE",    "SAW",   "RANDOM WALK", "SLEW RANDOM", "WOW",    "FLUTTER",
    "CURVE ENV", "RATCHET", "BURST", "AUDIO RATE",  "SAW DOWN"};
static const char *const follows[] = {"RAW", "BASE", "FULL"};
static const char *const domains[] = {"MUSIC CENT", "PHYSICAL HZ", "RATIO PPM", "PIO PERIOD"};
static const char *const save_names[] = {"PATCH ONLY", "TRACK ONLY", "PROJECT", "SYSTEM ONLY"};
static const char *const arp_names[] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM", "CHORD"};
static const char *const cv_modes[] = {"OFF", "1V/OCT", "MODULATE"};
static const char *const gate_modes[] = {"OFF", "NOTE", "RETRIGGER", "STEP", "CLOCK FX"};
static const char *const policies[] = {"AUTO", "EXTERNAL", "INTERNAL", "GATE"};
static const char *const directions[] = {"FORWARD", "REVERSE", "RANDOM"};
static const char *const dest[] = {"OFF",         "LEVEL",      "PAN",         "PITCH",
                                   "VIBRATO",     "TREMOLO",    "LAYER",       "ENV RATIO",
                                   "NOISE PITCH", "CLOCK BASE", "CLOCK DEPTH", "MORPH"};
static unsigned receive(const Project *p, unsigned t) {
    for (unsigned c = 0; c < 16; c++)
        if ((p->route[0][c] | p->route[1][c]) & (1u << t))
            return c;
    return 16;
}
static void toast(Ui *u, const char *message, uint32_t now) {
    snprintf(u->toast, sizeof(u->toast), "%.21s", message);
    u->toast_until = now + 1600;
}
static void error(Ui *u) {
    u->previous_page = u->page;
    u->page = UI_CONFLICT;
    u->edit = false;
}
static int64_t read_item(const Item *i) {
    return i->type == ITEM_U32   ? *(uint32_t *)i->value
           : i->type == ITEM_I32 ? *(int32_t *)i->value
           : i->type == ITEM_HZ  ? (int64_t)*(double *)i->value
           : i->type == ITEM_U16 ? *(uint16_t *)i->value
           : i->type == ITEM_I8  ? *(int8_t *)i->value
                                 : *(uint8_t *)i->value;
}
static void write_item(const Item *i, int64_t n) {
    if (i->type == ITEM_U32)
        *(uint32_t *)i->value = (uint32_t)n;
    else if (i->type == ITEM_I32)
        *(int32_t *)i->value = (int32_t)n;
    else if (i->type == ITEM_HZ)
        *(double *)i->value = (double)n;
    else if (i->type == ITEM_U16)
        *(uint16_t *)i->value = (uint16_t)n;
    else if (i->type == ITEM_I8)
        *(int8_t *)i->value = (int8_t)n;
    else
        *(uint8_t *)i->value = (uint8_t)n;
}
static unsigned build(Ui *u, Item a[48]) {
    Project *project = u->edit ? &u->draft : &u->engine->project;
    Track *t = &project->track[u->track];
    Patch *p = &t->patch;
    unsigned n = 0;
    uint32_t features = patch_features(p);
    if (!u->edit) {
        u->wave = 0;
        for (unsigned i = 0; i < (p->chip == CHIP_SAA ? 5u : 4u); i++)
            if (wave_choice[p->chip][i] == patch_wave(p))
                u->wave = (uint8_t)i;
    }
#define V(label, ptr, type, lo, hi, step, choices)                                             \
    a[n++] = (Item){label, ptr, lo, hi, step, type, 0, choices}
#define U(label, ptr, lo, hi) V(label, ptr, ITEM_U8, lo, hi, 1, NULL)
#define W(label, ptr, lo, hi, step) V(label, ptr, ITEM_U16, lo, hi, step, NULL)
#define I(label, ptr, lo, hi) V(label, ptr, ITEM_I8, lo, hi, 1, NULL)
#define C(label, ptr, lo, hi, choices) V(label, ptr, ITEM_U8, lo, hi, 1, choices)
#define L(title, target) a[n++] = (Item){.label = title, .type = ITEM_LINK, .key = target}
#define A(title, action)                                                                       \
    a[n++] = (Item) { .label = title, .type = ITEM_ACTION, .key = action }
    switch (u->page) {
    case UI_MENU:
        U("TRACK", &u->track, 0, TRACKS - 1);
        L("PLAY / SOUND", UI_QUICK);
        L("ADVANCED SOUND", UI_ADVANCED);
        L("SN CLOCK: ALL", UI_CLOCK);
        L("SEQUENCE / ARP", UI_SEQUENCE);
        L("MOD / MACROS", UI_MOD);
        L("CV / GATE", UI_CV);
        L("SAVE / LOAD", UI_SAVE);
        A("DIRECT10 RESET", ACT_DIRECT10);
        L("SYSTEM", UI_SYSTEM);
        A("PANIC", ACT_PANIC);
        break;
    case UI_QUICK:
        U("FACTORY", &u->factory, 0, FACTORY_PATCHES - 1);
        A("LOAD PATCH", ACT_PATCH);
        U("VOLUME", &t->level, 0, 100);
        if (p->output == OUTPUT_STEREO)
            U("PAN L/R", &t->pan, 0, 100);
        C("MUTE", &t->mute, 0, 1, yesno);
        C("TRACK ON", &t->enabled, 0, 1, yesno);
        A("POLY FOCUS", ACT_POLY_FOCUS);
        C("PLAY MODE", &t->mode, 0, 2, mode_names);
        U("POLY VOICES", &t->voices, 1, 6);
        C("CHIP", &p->chip, 0, 1, chips);
        C("WAVE", &u->wave, 0, p->chip == CHIP_SAA ? 4 : 3,
          p->chip == CHIP_SAA ? saa_waves : sn_waves);
        W("ATTACK MS", &p->attack, 0, 8000, 5);
        W("DECAY MS", &p->decay, 0, 8000, 10);
        U("SUSTAIN", &p->sustain, 0, 100);
        W("RELEASE MS", &p->release, 0, 8000, 10);
        U("PATCH LEVEL", &p->level, 0, 100);
        if (features & PATCH_F_ENV) {
            C("ENV SHAPE", &p->shape, 0, 7, shapes);
            V("ENV STEPS", &p->env_steps, ITEM_U8, 8, 16, 8, NULL);
            C("R INVERT", &p->env_invert, 0, 1, yesno);
            C("RETRIGGER", &p->retrigger, 0, 1, yesno);
        }
        if (features & PATCH_F_ENV_RATIO)
            I("ENV RATIO ST", &p->env_ratio, -48, 48);
        if ((features & PATCH_F_NOISE) && !(features & PATCH_F_PULSE)) {
            C("NOISE RATE", &p->noise_rate, 0, 3, rates);
            if (features & PATCH_F_NOISE_TYPE)
                C("NOISE TYPE", &p->noise_type, 0, 1, noise_types);
        }
        if (features & PATCH_F_NOISE_TRACK) {
            I(features & PATCH_F_PULSE ? "PULSE TUNE" : "NOISE PITCH", &p->noise_pitch, -48,
              48);
            I(features & PATCH_F_PULSE ? "PULSE SWEEP" : "NOISE SWEEP", &p->noise_sweep, -48,
              48);
        }
        if (features & PATCH_F_NOISE_MIX)
            U("NOISE MIX", &p->noise_mix, 0, 100);
        if (features & PATCH_F_LAYERS)
            U("LAYERS", &p->layers, 1, p->chip == CHIP_SAA ? 2 : 3);
        if (features & PATCH_F_LAYER_PITCH)
            I("LAYER ST", &p->layer_interval, -48, 48);
        if (features & PATCH_F_STACK)
            I("THIRD ST", &p->stack_interval, -48, 48);
        if (features & PATCH_F_SAA_LAYER) {
            C("LAYER SOURCE", &p->layer_source, 0, 2, sources);
            U("LAYER LEVEL", &p->layer_level, 0, 100);
            W("LAYER DECAY", &p->layer_decay, 0, 8000, 10);
        }
        if (features & PATCH_F_PITCH) {
            U("VIBRATO CENT", &p->vibrato, 0, 100);
            W("GLIDE MS", &p->glide, 0, 8000, 10);
        }
        U("TREMOLO", &p->tremolo, 0, 100);
        if (features & PATCH_F_DETUNE)
            U("DETUNE CENT", &p->detune, 0, 50);
        U("RECEIVE CH", &u->receive, 0, 16);
        A("AUDITION A2", ACT_AUDITION);
        L("SAVE / LOAD", UI_SAVE);
        L("ADVANCED", UI_ADVANCED);
        break;
    case UI_ADVANCED:
        C("CHIP", &p->chip, 0, 1, chips);
        C("SOURCE", &p->source, 0, 2, p->chip == CHIP_SAA ? sources : sn_sources);
        C("OUTPUT", &p->output, 0, 2, outputs);
        if (p->chip == CHIP_SAA) {
            C("HARDWARE ENV", &p->env, 0, 2, envelopes);
            if (p->env) {
                C("ENV SHAPE", &p->shape, 0, 7, shapes);
                V("ENV STEPS", &p->env_steps, ITEM_U8, 8, 16, 8, NULL);
                C("R INVERT", &p->env_invert, 0, 1, yesno);
                C("RETRIGGER", &p->retrigger, 0, 1, yesno);
                if (features & PATCH_F_ENV_RATIO)
                    I("ENV RATIO ST", &p->env_ratio, -48, 48);
            }
        }
        if (p->source != SOURCE_TONE ||
            (p->chip == CHIP_SAA && p->layers > 1 && p->layer_source != SOURCE_TONE)) {
            C("NOISE RATE", &p->noise_rate, 0, 3, rates);
            if (p->chip == CHIP_SN)
                C("NOISE TYPE", &p->noise_type, 0, 1, noise_types);
            if (p->noise_rate == 3) {
                I("NOISE PITCH", &p->noise_pitch, -48, 48);
                I("NOISE SWEEP", &p->noise_sweep, -48, 48);
            }
        }
        if (p->chip == CHIP_SN && p->source == SOURCE_MIX)
            U("NOISE MIX", &p->noise_mix, 0, 100);
        if (features & PATCH_F_LAYERS) {
            U("LAYERS", &p->layers, 1, p->chip == CHIP_SAA ? 2 : 3);
            if (p->layers > 1) {
                if (features & PATCH_F_LAYER_PITCH)
                    I("LAYER ST", &p->layer_interval, -48, 48);
                if (p->chip == CHIP_SN && p->layers == 3)
                    I("THIRD ST", &p->stack_interval, -48, 48);
                if (p->chip == CHIP_SAA) {
                    C("LAYER SOURCE", &p->layer_source, 0, 2, sources);
                    U("LAYER LEVEL", &p->layer_level, 0, 100);
                    W("LAYER DECAY", &p->layer_decay, 0, 8000, 10);
                }
            }
        }
        if (features & PATCH_F_PITCH) {
            I("TRANSPOSE ST", &p->transpose, -48, 48);
            I("FINE CENT", &p->fine, -50, 50);
            I("PITCH ENV ST", &p->pitch_amount, -48, 48);
            W("PITCH DECAY", &p->pitch_decay, 0, 8000, 10);
            W("GLIDE MS", &p->glide, 0, 8000, 10);
            U("VIBRATO CENT", &p->vibrato, 0, 100);
        }
        U("TREMOLO", &p->tremolo, 0, 100);
        U("LFO RATE", &p->lfo_rate, 0, 7);
        C("LFO SHAPE", &p->lfo_shape, 0, 3, lfos);
        if (features & PATCH_F_DETUNE)
            U("DETUNE CENT", &p->detune, 0, 50);
        U("MORPH USER", &t->morph_target, 0, USER_PATCHES);
        U("MORPH A-B", &t->morph, 0, 100);
        A("PATCH NAME", ACT_NAME);
        break;
    case UI_CLOCK: {
        SnClockConfig *c = &project->sn_clock;
        SnModConfig *m = &c->mod[u->clock_slot];
        V("BASE CLOCK HZ", &c->base_hz, ITEM_HZ, 1, SN_CLOCK_SYS_HZ / 10, 1, NULL);
        C("FOLLOW", &c->follow, 0, 2, follows);
        C("STOP CLOCK", &c->stopped, 0, 1, yesno);
        U("MOD A/B", &u->clock_slot, 0, 1);
        C("CLOCK FX", &m->mode, 0, SN_CLOCK_MODES - 1, clock_modes);
        C("DOMAIN", &m->domain, 0, 3, domains);
        V("OFFSET", &m->offset, ITEM_I32, INT32_MIN, INT32_MAX, 1, NULL);
        V("DEPTH", &m->depth, ITEM_I32, INT32_MIN, INT32_MAX, 1, NULL);
        V("RATE MILLIHZ", &m->rate_millihz, ITEM_U32, 0, UINT32_MAX, 1, NULL);
        U("SYNC CLOCKS", &m->sync, 0, 8);
        C("RETRIGGER", &m->retrigger, 0, 1, yesno);
        U("TRIGGER FROM", &project->sn_trigger_source, 0, 7);
        U("CURVE", &m->curve, 0, 2);
        A("TRIGGER", ACT_CLOCK_TRIGGER);
        L("ADVANCED / STATUS", UI_CLOCK_ADVANCED);
        break;
    }
    case UI_CLOCK_ADVANCED: {
        SnClockConfig *c = &project->sn_clock;
        C("DIRECT PIO", &c->direct, 0, 1, yesno);
        V("PIO X", &c->period, ITEM_U32, 0, UINT32_MAX, 1, NULL);
        V("DIV *256", &c->div256, ITEM_U32, SN_CLOCK_DIV_MIN, SN_CLOCK_DIV_MAX, 1, NULL);
        L("CLOCK", UI_CLOCK);
        break;
    }
    case UI_SEQUENCE: {
        Sequence *q = &project->sequence[u->track];
        C("SEQUENCE", &q->enabled, 0, 1, yesno);
        C("ARPEGGIO", &q->arp, 0, 5, arp_names);
        U("ARP OCTAVES", &q->arp_octaves, 1, 4);
        U("LENGTH", &q->length, 1, 16);
        U("DIVISION", &q->division, 1, 24);
        C("DIRECTION", &q->direction, 0, 2, directions);
        U("STEP", &u->step, 0, 15);
        U("NOTE", &q->note[u->step], 0, 127);
        U("GATE", &q->gate[u->step], 0, 100);
        U("VELOCITY", &q->velocity[u->step], 0, 127);
        I("MACRO1 STEP", &q->motion[u->step], -100, 100);
        break;
    }
    case UI_MOD: {
        U("MOD SLOT", &u->mod, 0, MOD_SLOTS - 1);
        ModRoute *m = &project->mod[u->mod];
        U("SOURCE", &m->source, 0, 4 + TRACKS * 2);
        U("TO TRACK", &m->track, 0, TRACKS - 1);
        C("PARAMETER", &m->parameter, 0, MOD_PARAMS - 1, dest);
        I("DEPTH", &m->amount, -100, 100);
        U("MACRO", &u->macro, 0, 3);
        C("MACRO DEST", &t->macro_dest[u->macro], 0, MOD_PARAMS - 1, dest);
        I("MACRO VALUE", &t->macro[u->macro], -100, 100);
        break;
    }
    case UI_CV: {
        U("INPUT", &u->input, 0, 1);
        CvRoute *c = &project->cv[u->input];
        C("CV MODE", &c->mode, 0, 2, cv_modes);
        U("CV TRACK", &c->track, 0, TRACKS - 1);
        U("0V NOTE", &c->base_note, 0, 127);
        U("SCALE", &c->scale, 0, CV_SCALES - 1);
        U("ROOT", &c->root, 0, 11);
        U("GLIDE", &c->glide, 0, 7);
        U("RANGE 10V", &c->range, 0, 1);
        C("MOD PARAM", &c->parameter, 0, MOD_PARAMS - 1, dest);
        I("AMOUNT", &c->amount, -100, 100);
        C("GATE MODE", &c->gate, 0, 4, gate_modes);
        U("GATE TRACK", &c->gate_track, 0, TRACKS - 1);
        U("FIXED NOTE", &c->gate_note, 0, 127);
        break;
    }
    case UI_SAVE:
        C("SAVE WHAT", &u->save_kind, 0, 3, save_names);
        U("SLOT", &u->slot, 0, u->save_kind == SAVE_PROJECT ? PROJECTS - 1 : 15);
        A("SAVE", ACT_SAVE);
        A("LOAD PROJECT", ACT_LOAD_PROJECT);
        A("LOAD TRACK", ACT_LOAD_TRACK);
        break;
    case UI_SYSTEM:
        U("BRIGHTNESS", &u->system_draft.brightness, 10, 100);
        U("TEMPO BPM", &project->tempo, 0, 210);
        U("CLOCK INPUT", &project->clock_source, 0, 1);
        C("CLOCK POLICY", &project->clock_policy, 0, 3, policies);
        U("GATE CLOCK", &project->gate_clock, 0, 1);
        U("TRS TO USB", &project->midi_thru, 0, 1);
        A("CV1 ZERO", ACT_CAL0);
        A("CV1 1V", ACT_CAL1);
        A("CV2 ZERO", ACT_CAL2);
        A("CV2 1V", ACT_CAL3);
        A("USB UPDATE", ACT_BOOT);
        A("PANIC", ACT_PANIC);
        break;
    default:
        break;
    }
#undef V
#undef U
#undef W
#undef I
#undef C
#undef L
#undef A
    return n;
}
unsigned ui_count(Ui *u) { return build(u, items); }
void ui_init(Ui *u, Settings *s, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx) {
    memset(u, 0, sizeof(*u));
    u->settings = s;
    u->engine = e;
    u->save = save;
    u->calibrate = calibrate;
    u->ctx = ctx;
    u->factory = 20;
    u->receive = (uint8_t)receive(&e->project, 0);
    e->system = s->system;
    u->system_draft = e->system;
    e->library = s;
}
void ui_open(Ui *u, unsigned page) {
    if (page >= UI_PAGES)
        return;
    if (u->depth < 4) {
        u->path[u->depth] = u->page;
        u->cursor[u->depth++] = u->field;
    }
    u->page = (uint8_t)page;
    u->field = 0;
    u->edit = false;
    u->receive = (uint8_t)receive(&u->engine->project, u->track);
    u->system_draft = u->engine->system;
}
static void back(Ui *u) {
    if (u->edit) {
        u->edit = false;
        return;
    }
    if (u->depth) {
        u->depth--;
        u->page = u->path[u->depth];
        u->field = u->cursor[u->depth];
    } else
        u->page = UI_HOME;
}
static void perform(Ui *u, unsigned action, uint32_t now) {
    Engine *e = u->engine;
    Settings *s = u->settings;
    bool ok = false;
    if (action == ACT_POLY_FOCUS) {
        ok = engine_poly_focus(e, u->track);
        if (!ok)
            error(u);
    } else if (action == ACT_DIRECT10) {
        project_default(&u->draft);
        ok = engine_load_project(e, &u->draft);
        if (!ok)
            error(u);
    } else if (action == ACT_PATCH) {
        Patch p;
        char name[9];
        patch_factory(u->factory, &p, name);
        ok = engine_load_patch(e, u->track, &p, name);
        if (!ok)
            error(u);
    } else if (action == ACT_SAVE) {
        if (u->save_kind == SAVE_SYSTEM)
            e->system = u->system_draft;
        ok = settings_save(s, e, (SaveKind)u->save_kind, u->track, u->slot, u->save, u->ctx);
        toast(u, ok ? "SAVED" : "SAVE FAILED", now);
    } else if (action == ACT_LOAD_PROJECT) {
        if (u->slot < PROJECTS)
            ok = engine_load_project(e, &s->project[u->slot]);
        if (!ok)
            error(u);
    } else if (action == ACT_LOAD_TRACK) {
        if (u->slot < USER_TRACKS && (s->track_used & (1u << u->slot))) {
            u->draft = e->project;
            u->draft.track[u->track] = s->tracks[u->slot];
            ok = engine_apply(e, &u->draft);
            if (!ok)
                error(u);
        } else
            toast(u, "EMPTY TRACK SLOT", now);
    } else if (action == ACT_CLOCK_TRIGGER) {
        e->sn_manual_trigger++;
    } else if (action == ACT_PANIC) {
        engine_panic(e, -1);
        toast(u, "ALL NOTES OFF", now);
    } else if (action == ACT_BOOT)
        u->boot_requested = true;
    else if (action >= ACT_CAL0 && action <= ACT_CAL3) {
        unsigned n = action - ACT_CAL0;
        ok = u->calibrate && u->calibrate(u->ctx, n / 2, (n & 1) != 0);
        u->system_draft = e->system;
        toast(u, ok ? "CALIBRATED (RAM)" : "CALIBRATION FAILED", now);
    } else if (action == ACT_AUDITION) {
        engine_audition(e, u->track, 45, e->test_on ? 0 : 3);
    } else if (action == ACT_NAME) {
        memcpy(u->name, e->project.track[u->track].name, 9);
        ui_open(u, UI_NAME);
        u->name_cursor = 0;
    }
    if (ok)
        u->dirty = true;
}
void ui_click(Ui *u, unsigned button, bool long_press, uint32_t now) {
    if (button > 2)
        return;
    if (u->page == UI_CONFLICT) {
        if (button == 1)
            u->page = u->previous_page;
        return;
    }
    if (u->confirm) {
        if (long_press) {
            u->confirm = false;
            return;
        }
        if (button == 1) {
            u->confirm = false;
            if (u->confirm_yes)
                perform(u, u->action, now);
        } else
            u->confirm_yes = !u->confirm_yes;
        return;
    }
    if (u->page == UI_NAME) {
        if (long_press) {
            back(u);
            return;
        }
        if (button == 1) {
            if (u->edit)
                u->edit = false;
            else if (u->name_cursor == 8) {
                u->draft = u->engine->project;
                memcpy(u->draft.track[u->track].name, u->name, 9);
                if (engine_apply(u->engine, &u->draft))
                    back(u);
            } else
                u->edit = true;
        } else if (u->edit) {
            int c = u->name[u->name_cursor] + (button == 2 ? 1 : -1);
            u->name[u->name_cursor] = (char)(c > 90 ? 32 : c < 32 ? 90 : c);
        } else
            u->name_cursor = (uint8_t)((u->name_cursor + (button == 2 ? 1 : 8)) % 9);
        return;
    }
    if (u->page == UI_HOME) {
        if (button != 1) {
            u->track = (uint8_t)((u->track + (button == 2 ? 1 : TRACKS - 1)) % TRACKS);
            u->receive = (uint8_t)receive(&u->engine->project, u->track);
        } else
            ui_open(u, long_press ? UI_MENU : UI_QUICK);
        return;
    }
    if (long_press) {
        back(u);
        return;
    }
    unsigned n = build(u, items);
    if (!n)
        return;
    if (u->field >= n)
        u->field = 0;
    Item *item = &items[u->field];
    if (u->edit) {
        if (button != 1) {
            int64_t v = u->value + (button == 2 ? 1 : -1) * item->step *
                                       (u->acceleration ? u->acceleration : 1);
            u->value = v < item->min ? item->min : v > item->max ? item->max : v;
            return;
        }
        write_item(item, u->value);
        if (item->type == ITEM_HZ)
            u->draft.sn_clock.direct = 0;
        if (!strcmp(item->label, "PIO X") || !strcmp(item->label, "DIV *256"))
            u->draft.sn_clock.direct = 1;
        Patch *p = &u->draft.track[u->track].patch;
        if (item->value == &p->chip)
            patch_default(p, (unsigned)u->value);
        if (item->value == &u->wave)
            (void)patch_set_wave(p, wave_choice[p->chip][u->wave]);
        if (item->value == &p->source && p->source != SOURCE_TONE) {
            p->layers = 1;
            if (p->env == ENV_OSC)
                p->env = ENV_AM;
        }
        if (item->value == &p->env && p->env != ENV_OSC && p->chip == CHIP_SAA)
            p->layers = 1;
        if (p->chip == CHIP_SN) {
            p->env = ENV_OFF;
            if (p->source != SOURCE_TONE)
                p->layers = 1;
            if (p->source == SOURCE_MIX)
                p->noise_rate = 3;
        }
        if (p->chip == CHIP_SAA && p->layers > 1) {
            p->env = ENV_OSC;
            p->source = SOURCE_TONE;
        }
        if (p->env == ENV_OSC)
            p->source = SOURCE_TONE;
        if (p->noise_rate != 3 ||
            (p->source == SOURCE_TONE &&
             !(p->chip == CHIP_SAA && p->layers > 1 && p->layer_source != SOURCE_TONE)))
            p->noise_pitch = p->noise_sweep = 0;
        if (!strcmp(item->label, "RECEIVE CH"))
            for (unsigned s = 0; s < SOURCES; s++)
                for (unsigned c = 0; c < 16; c++) {
                    u->draft.route[s][c] &= (uint16_t)~(1u << u->track);
                    if (c == u->receive)
                        u->draft.route[s][c] |= (uint16_t)(1u << u->track);
                }
        if (!patch_same_structure(p, &u->engine->project.track[u->track].patch))
            u->draft.track[u->track].morph_target = u->draft.track[u->track].morph = 0;
        bool local = item->value == &u->track || item->value == &u->slot ||
                     item->value == &u->save_kind || item->value == &u->factory ||
                     item->value == &u->step || item->value == &u->mod ||
                     item->value == &u->input || item->value == &u->clock_slot ||
                     item->value == &u->macro || item->value == &u->system_draft.brightness;
        if (item->value == &u->system_draft.brightness)
            u->engine->system.brightness = u->system_draft.brightness;
        u->edit = false;
        if (!local && !engine_apply(u->engine, &u->draft)) {
            error(u);
            return;
        }
        u->dirty = true;
        return;
    }
    if (button != 1) {
        u->field = (uint8_t)((u->field + (button == 2 ? 1 : n - 1)) % n);
        return;
    }
    if (item->type == ITEM_LINK) {
        ui_open(u, item->key);
        return;
    }
    if (item->type == ITEM_ACTION) {
        if (item->key == ACT_SAVE || item->key == ACT_LOAD_PROJECT ||
            item->key == ACT_LOAD_TRACK || item->key == ACT_BOOT || item->key == ACT_PATCH ||
            item->key == ACT_POLY_FOCUS || item->key == ACT_DIRECT10) {
            u->confirm = true;
            u->confirm_yes = false;
            u->action = item->key;
        } else
            perform(u, item->key, now);
        return;
    }
    u->value = read_item(item);
    u->draft = u->engine->project;
    u->edit = true;
}

void ui_buttons(Ui *u, const bool down[3], uint32_t now) {
    /* Chord capture precedes individual actions. A release cannot leak a click. */
    bool both = down[0] && down[2];
    if (both && !u->chord) {
        u->chord = true;
        u->chord_sent = false;
        u->chord_since = now;
    }
    if (u->chord) {
        for (unsigned b = 0; b < 3; b++) {
            u->raw[b] = u->stable[b] = down[b];
            u->long_sent[b] = true;
            u->changed[b] = now;
        }
        if (both && !u->chord_sent && now - u->chord_since >= 900) {
            perform(u, ACT_PANIC, now);
            u->chord_sent = true;
        }
        if (!down[0] && !down[1] && !down[2])
            u->chord = false;
        return;
    }
    for (unsigned b = 0; b < 3; b++) {
        if (down[b] != u->raw[b]) {
            u->raw[b] = down[b];
            u->changed[b] = now;
        }
        if (down[b] != u->stable[b] && now - u->changed[b] >= 20) {
            u->stable[b] = down[b];
            if (down[b]) {
                u->pressed[b] = u->repeat[b] = now;
                u->long_sent[b] = false;
            } else if (!u->long_sent[b])
                ui_click(u, b, false, now);
        }
        if (u->stable[b] && b == 1 && !u->long_sent[b] && now - u->pressed[b] >= 700) {
            u->long_sent[b] = true;
            ui_click(u, b, true, now);
        }
        if (u->stable[b] && b != 1 && now - u->pressed[b] >= 450 && now - u->repeat[b] >= 80) {
            u->repeat[b] = now;
            u->long_sent[b] = true;
            u->acceleration = (uint8_t)(now - u->pressed[b] > 5000   ? 64
                                        : now - u->pressed[b] > 3000 ? 16
                                        : now - u->pressed[b] > 1500 ? 4
                                                                     : 1);
            ui_click(u, b, false, now);
            u->acceleration = 1;
        }
    }
}
static void header(Ui *u, const char *s) {
    text(u, 0, 0, s, 1, true);
    if (u->dirty)
        text(u, 121, 0, "*", 1, true);
    line(u, 0, 10, 127, 10);
}
static void footer(Ui *u, const char *s) {
    box(u, 0, 54, 128, 10, false);
    line(u, 0, 53, 127, 53);
    text(u, 0, 56, s, 1, true);
}
static void item_value(Ui *u, const Item *i, int64_t value, char out[32]) {
    if (i->names) {
        snprintf(out, 32, "%s", i->names[value - i->min]);
        return;
    }
    if (!strcmp(i->label, "TRACK") || !strcmp(i->label, "TO TRACK") ||
        !strcmp(i->label, "CV TRACK") || !strcmp(i->label, "GATE TRACK")) {
        unsigned c = receive(&u->engine->project, (unsigned)value);
        if (c < 16)
            snprintf(out, 32, "%u CH%u", (unsigned)value + 1, c + 1);
        else
            snprintf(out, 32, "%u OFF", (unsigned)value + 1);
        return;
    }
    if (!strcmp(i->label, "FACTORY")) {
        Patch p;
        char name[9];
        patch_factory((unsigned)value, &p, name);
        snprintf(out, 32, "%02d %.8s", (int)value + 1, name);
        return;
    }
    if (!strcmp(i->label, "SOURCE") && u->page == UI_MOD) {
        if (value < 5) {
            snprintf(out, 32, "%s",
                     (const char *[]){"OFF", "CV1", "CV2", "CC1", "PRESSURE"}[value]);
            return;
        }
        snprintf(out, 32, "%s%u",
                 value < 5 ? (const char *[]){"OFF", "CV", "CV", "CC1", "PRESSURE"}[value]
                 : value < 5 + TRACKS ? "LFO T"
                                      : "STEP T",
                 value == 1 || value == 2 ? (unsigned)value
                 : value >= 5             ? (unsigned)(value - 5) % TRACKS + 1
                                          : 0);
        return;
    }
    if (!strcmp(i->label, "RATE") && u->page == UI_CLOCK) {
        static const char *rates_text[] = {"0.25HZ", "0.5HZ", "1HZ",   "2HZ",  "4HZ",   "8HZ",
                                           "16HZ",   "32HZ",  "50HZ",  "80HZ", "100HZ", "160HZ",
                                           "250HZ",  "400HZ", "600HZ", "800HZ"};
        snprintf(out, 32, "%s", rates_text[value]);
        return;
    }
    if (!strcmp(i->label, "LFO RATE")) {
        static const char *rates_text[] = {"0.5HZ", "1HZ",  "2HZ",  "4HZ",
                                           "8HZ",   "16HZ", "32HZ", "64HZ"};
        snprintf(out, 32, "%s", rates_text[value]);
        return;
    }
    if (!strcmp(i->label, "TEMPO BPM")) {
        snprintf(out, 32, "%lld", (long long)value + 30);
        return;
    }
    if (!strcmp(i->label, "RECEIVE CH") && value == 16) {
        snprintf(out, 32, "OFF");
        return;
    }
    bool one = !strcmp(i->label, "GATE CLOCK") || !strcmp(i->label, "SLOT") ||
               !strcmp(i->label, "RECEIVE CH") || !strcmp(i->label, "INPUT") ||
               !strcmp(i->label, "STEP") || !strcmp(i->label, "MOD SLOT") ||
               !strcmp(i->label, "MACRO");
    snprintf(out, 32, "%lld", (long long)value + (one ? 1 : 0));
}
void ui_render(Ui *u, const UiStatus *status, uint32_t now) {
    memset(u->frame, 0, sizeof(u->frame));
    char label[40];
    const Track *t = &u->engine->project.track[u->track];
    if (u->page == UI_CONFLICT) {
        header(u, "RESOURCE CONFLICT");
        const char *reason = u->engine->conflict.message;
        const char *suggestion = u->engine->conflict.suggestion;
        for (unsigned i = 0; i < 2; i++) {
            snprintf(label, sizeof(label), "%.21s",
                     reason + (strlen(reason) > i * 21 ? i * 21 : strlen(reason)));
            text(u, 0, 13 + (int)i * 10, label, 1, true);
            snprintf(label, sizeof(label), "%.21s",
                     suggestion + (strlen(suggestion) > i * 21 ? i * 21 : strlen(suggestion)));
            text(u, 0, 33 + (int)i * 10, label, 1, true);
        }
        footer(u, "OK:BACK - KEEP SOUND");
        return;
    }
    if (u->confirm) {
        header(u, "CONFIRM");
        snprintf(label, sizeof(label), "%s",
                 u->action == ACT_SAVE         ? save_names[u->save_kind]
                 : u->action == ACT_PATCH      ? "LOAD PATCH?"
                 : u->action == ACT_BOOT       ? "USB UPDATE?"
                 : u->action == ACT_POLY_FOCUS ? "POLY FOCUS?"
                 : u->action == ACT_DIRECT10   ? "DIRECT10 RESET?"
                                               : "LOAD SETTINGS?");
        text(u, 0, 17, label, 1, true);
        if (u->action == ACT_POLY_FOCUS)
            text(u, 0, 31, "SAME CHIP OTHERS OFF", 1, true);
        else if (u->action == ACT_DIRECT10)
            text(u, 0, 31, "REPLACE ALL TRACKS", 1, true);
        else
            text(u, 0, 31,
                 u->action == ACT_SAVE && u->save_kind == SAVE_PATCH ? "PROJECT UNCHANGED"
                                                                     : "OK TO CONTINUE?",
                 1, true);
        text(u, 12, 43, u->confirm_yes ? "NO    > YES" : " > NO     YES", 1, true);
        footer(u, "OK:SET HOLD:CANCEL");
        return;
    }
    if (u->page == UI_NAME) {
        header(u, "PATCH NAME");
        text(u, 2, 21, u->name, 2, true);
        if (u->name_cursor < 8)
            line(u, 2 + u->name_cursor * 12, 39, 11 + u->name_cursor * 12, 39);
        else
            text(u, 92, 43, "DONE", 1, true);
        footer(u, u->edit ? "-+:LETTER OK:SET" : "-+:MOVE OK:EDIT");
        return;
    }
    if (u->page == UI_HOME) {
        unsigned c = receive(&u->engine->project, u->track);
        snprintf(label, sizeof(label), "T%02u %s %s", u->track + 1,
                 t->patch.chip == CHIP_SAA ? "SAA" : "SN",
                 t->mode == PLAY_POLY ? "POLY" : "DIRECT");
        header(u, label);
        text(u, 2, 15, t->name, 2, true);
        snprintf(label, sizeof(label), "CH%u %u/%u %s", c + 1,
                 engine_track_notes(u->engine, u->track), u->engine->plan.count[u->track],
                 t->mute      ? "MUTED"
                 : t->enabled ? ""
                              : "OFF");
        if (c >= 16)
            snprintf(label, sizeof(label), "NO MIDI %s", t->enabled ? "" : "OFF");
        text(u, 1, 34, label, 1, true);
        snprintf(label, sizeof(label), "VOL %u  PAN %u %s", t->level, t->pan,
                 status->usb ? "USB" : "TRS");
        text(u, 1, 44, label, 1, true);
        footer(u, (now / 3000) % 2 ? "HOLD OK:MENU" : "-+:TRACK OK:SOUND");
    } else {
        static const char *titles[] = {"HOME",           "MENU",          "PLAY / SOUND",
                                       "ADVANCED SOUND", "SN CLOCK: ALL", "SEQUENCE / ARP",
                                       "MOD / MACROS",   "CV / GATE",     "SAVE / LOAD",
                                       "SYSTEM",         "NAME",          "CONFLICT",
                                       "CLOCK ADVANCED"};
        header(u, u->page == UI_SYSTEM ? "SYSTEM V0.11" : titles[u->page]);
        unsigned n = build(u, items);
        if (u->field >= n)
            u->field = 0;
        if (u->edit && n) {
            Item *item = &items[u->field];
            text(u, 0, 16, item->label, 1, true);
            item_value(u, item, u->value, label);
            text(u, 0, 31, label, 1, true);
            footer(u, "-+:VALUE OK:SET");
        } else {
            unsigned start = u->field / 3 * 3;
            for (unsigned i = 0; i < 3 && start + i < n; i++) {
                Item *item = &items[start + i];
                int y = 14 + (int)i * 13;
                bool selected = start + i == u->field;
                if (selected)
                    box(u, 0, y - 1, 128, 11, true);
                char name[24];
                if (item->type < ITEM_LINK) {
                    item_value(u, item, read_item(item), label);
                    unsigned width = (unsigned)strlen(label);
                    if (width > 12)
                        width = 12;
                    int x = 127 - (int)width * 6;
                    unsigned chars = x > 7 ? (unsigned)x / 6 - 1 : 0;
                    snprintf(name, sizeof(name), "%.*s", (int)chars, item->label);
                    text(u, 1, y, name, 1, !selected);
                    text(u, x, y, label, 1, !selected);
                } else {
                    snprintf(name, sizeof(name), "%.18s", item->label);
                    text(u, 1, y, name, 1, !selected);
                    text(u, 116, y, ">", 1, !selected);
                }
            }
            footer(u, "OK:OPEN HOLD:BACK");
        }
    }
    if (u->page == UI_CLOCK || u->page == UI_CLOCK_ADVANCED) {
        static const char *states[] = {"VALID", "DEGRADED", "UNAVAILABLE", "STOPPED"};
        Engine *e = u->engine;
        if (u->page == UI_CLOCK_ADVANCED && (now / 2000) % 2) {
            unsigned c = (now / 4000) % 2;
            snprintf(label, sizeof(label), "%c %u %u %u C%u", c ? 'R' : 'L', e->bus.tone[c][0],
                     e->bus.tone[c][1], e->bus.tone[c][2], e->sn_clip_mask);
        } else if (u->page == UI_CLOCK_ADVANCED) {
            snprintf(label, sizeof(label), "WAIT %llu US Q%u",
                     (unsigned long long)e->bus.last_wait_us, bus_pending(&e->bus));
        } else {
            snprintf(label, sizeof(label), "%.4gHZ %s%s", e->sn_clock_hz,
                     states[e->sn_clock_state < 4 ? e->sn_clock_state : 2],
                     e->sn_experimental ? " EXP" : "");
        }
        footer(u, label);
    }
    if (u->toast_until && (int32_t)(u->toast_until - now) > 0) {
        box(u, 0, 43, 128, 10, false);
        text(u, 0, 44, u->toast, 1, true);
    }
}
void ui_external_change(Ui *u) {
    u->edit = u->confirm = false;
    u->dirty = true;
    u->receive = (uint8_t)receive(&u->engine->project, u->track);
    u->system_draft = u->engine->system;
}
