#include "ui.h"
#include "studio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
_Static_assert(UI_PAGES <= sizeof(((Ui *)0)->memory), "Page cursor capacity");
static const uint8_t glyph[36][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},       {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
/* All ordinary pages share the same selection/edit/back state machine. */
typedef enum { U8, I8, U16, LINK, ACTION } ItemType;
typedef struct {
    const char *label;
    void *ptr;
    int min, max, step;
    const char *const *names;
    ItemType type;
    unsigned key;
    bool structural;
} Item;
enum {
    A_PANIC = 100,
    A_SAVE,
    A_LOAD,
    A_INIT,
    A_SAVE_SOUND,
    A_LOAD_SOUND,
    A_NAME_SCENE,
    A_NAME_PART,
    A_NAME_SOUND,
    A_COPY,
    A_SAVE_SYSTEM,
    A_BOOT,
    A_CAL0,
    A_CAL1,
    A_CAL2,
    A_CAL3,
    A_GRID_NOTE,
    A_GRID_GATE,
    A_GRID_VEL,
    A_GRID_MOD,
    A_CLOCK_GRID,
    A_VERSION,
    A_SELECT_SOUND,
    A_SINGLE,
    A_FACTORY
};
static const char *const yesno[] = {"OFF", "ON"};
static const char *const shape_names[] = {"ZERO",  "MAX",      "DECAY 1",  "SAW DOWN",
                                          "TRI 1", "TRIANGLE", "ATTACK 1", "SAW UP"};
static const char *const modes[] = {"POLY", "MONO", "LEGATO"};
static const char *const noise_rates[] = {"FAST", "MID", "SLOW", "KEY TRACK"};
static const char *const cv_modes[] = {"OFF", "NOTE", "MOD"};
static const char *const gates[] = {"OFF", "NOTE", "RETRIGGER", "STEP", "FX TRIGGER"};
static const char *const scales[] = {"OFF",    "CHROMATIC", "MAJOR",     "MINOR",
                                     "DORIAN", "MIN PENTA", "MAJ PENTA", "WHOLE TONE"};
static const char *const glide_names[] = {"0 MS",   "10 MS",  "30 MS",  "60 MS",
                                          "120 MS", "250 MS", "500 MS", "1000 MS"};
static const char *const references[] = {"4 MHZ", "2 MHZ", "1 MHZ"};
static const char *const clock_modes[] = {"OFF",    "MANUAL", "RISE",     "TRIANGLE",
                                          "SQUARE", "RANDOM", "SEQUENCE", "DIVE"};
static const char *const arp_names[] = {"OFF", "UP", "DOWN", "UP DOWN", "RANDOM", "CHORD"};
static const char *const dir_names[] = {"FORWARD", "REVERSE", "RANDOM"};
static const char *const lfo_names[] = {"SINE", "TRIANGLE", "SQUARE", "SAMPLE HOLD"};
static const char *const test_names[] = {"OFF", "SINGLE", "REPEAT", "HOLD"};
static const char *const rates[] = {"0.5 HZ", "1 HZ",  "2 HZ",  "4 HZ",
                                    "8 HZ",   "16 HZ", "32 HZ", "64 HZ"};
static void toast(Ui *u, const char *s, uint32_t now) {
    snprintf(u->toast, sizeof(u->toast), "%s", s);
    u->toast_until = now + 1400;
}
static int read_item(const Item *i) {
    if (i->type == I8)
        return *(int8_t *)i->ptr;
    if (i->type == U16)
        return *(uint16_t *)i->ptr;
    return *(uint8_t *)i->ptr;
}
static void write_item(const Item *i, void *ptr, int v) {
    if (i->type == I8)
        *(int8_t *)ptr = (int8_t)v;
    else if (i->type == U16)
        *(uint16_t *)ptr = (uint16_t)v;
    else
        *(uint8_t *)ptr = (uint8_t)v;
}
static bool sound_at(Ui *u, unsigned slot, StSound *sound, char name[9]) {
    if (slot < ST_SOUNDS) {
        if (!(u->settings->sound_used & (1u << slot)))
            return false;
        *sound = u->settings->sounds[slot];
        memcpy(name, u->settings->sound_name[slot], 9);
    } else if (slot < ST_SOUNDS * 2)
        studio_factory(slot - ST_SOUNDS, sound, name);
    else
        return false;
    return true;
}
static bool sound_matches(Ui *u, unsigned slot) {
    StSound sound;
    char name[9];
    StPart *p = &u->engine->preset.studio.part[u->part];
    return sound_at(u, slot, &sound, name) && !memcmp(&p->sound, &sound, sizeof(sound)) &&
           !memcmp(p->name, name, 9);
}
static void sync_sound(Ui *u) {
    if (sound_matches(u, u->active_sound))
        return;
    for (unsigned slot = 0; slot < ST_SOUNDS * 2; slot++)
        if (sound_matches(u, slot)) {
            u->active_sound = (uint8_t)slot;
            return;
        }
}
static unsigned receive_channel(const Engine *e, unsigned part) {
    for (unsigned channel = 0; channel < 16; channel++)
        if ((e->preset.studio.route[0][channel] | e->preset.studio.route[1][channel]) &
            (1u << part))
            return channel;
    return 0;
}
static bool select_sound(Ui *u, unsigned slot, uint32_t now) {
    StSound sound;
    char name[9];
    if (!sound_at(u, slot, &sound, name)) {
        toast(u, "EMPTY SLOT", now);
        return false;
    }
    if (!studio_sound_load(u->engine, u->part, &sound, name)) {
        toast(u, "NO ROOM: USE SINGLE", now);
        return false;
    }
    u->active_sound = (uint8_t)slot;
    if (slot < ST_SOUNDS)
        u->library_slot = (uint8_t)slot;
    u->dirty = true;
    if (u->test_mode)
        studio_test(u->engine, u->part, u->test_note, u->test_mode);
    return true;
}
static unsigned build(Ui *u, Item a[32]) {
    Engine *e = u->engine;
    StScene *scene = &e->preset.studio;
    StPart *p = &scene->part[u->part];
    StSound *s = &p->sound;
    unsigned n = 0;
#define V(l, p, t, lo, hi, st, opt)                                                                \
    a[n++] = (Item) {                                                                              \
        l, p, lo, hi, st, opt, t, 0, false                                                         \
    }
#define S(l, p, t, lo, hi, st, opt)                                                                \
    do {                                                                                           \
        V(l, p, t, lo, hi, st, opt);                                                               \
        a[n - 1].structural = true;                                                                \
    } while (0)
#define L(l, page)                                                                                 \
    a[n++] = (Item) {                                                                              \
        .label = l, .type = LINK, .key = page                                                      \
    }
#define A(l, id)                                                                                   \
    a[n++] = (Item) {                                                                              \
        .label = l, .type = ACTION, .key = id                                                      \
    }
#define PART() V("PART", &u->part, U8, 0, ST_PARTS - 1, 1, NULL)
    switch (u->page) {
    case UI_PLAY:
        PART();
        for (unsigned k = 0; k < 4; k++)
            V(st_param_names[s->macro_dest[k]], &s->macro[k], I8, -100, 100, 1, NULL);
        V("TEST", &u->test_mode, U8, 0, 3, 1, test_names);
        V("TEST NOTE", &u->test_note, U8, 0, 127, 1, NULL);
        V("SEQUENCE", &e->studio.running, U8, 0, 1, 1, yesno);
        V("MUTE", &e->studio.mute[u->part], U8, 0, 1, 1, yesno);
        V("MASTER", &e->preset.mix[0], U8, 0, 100, 1, NULL);
        L("CHIP MIX", UI_MIX);
        A("ALL NOTES OFF", A_PANIC);
        break;
    case UI_SOUND:
        if (s->model == ST_SAA_ENV || s->model == ST_SAA_AM) {
            V("WAVE", &s->shape, U8, 0, 7, 1, shape_names);
            V("STEPS", &s->steps, U8, 8, 16, 8, NULL);
        } else if (s->model >= ST_SN_NOISE || s->model == ST_SAA_NOISE || s->model == ST_SAA_MIX) {
            if (s->model == ST_SN_METAL)
                V("TONE MIX", &s->noise_mix, U8, 0, 100, 1, NULL);
            else
                S("NOISE RATE", &s->noise_rate, U8, 0, 3, 1, noise_rates);
            V("DECAY MS", &s->decay, U16, 0, 8000, 10, NULL);
        } else {
            if (s->model >= ST_SN_TWIN)
                V("DETUNE CENT", &s->detune, U8, 0, 50, 1, NULL);
            else
                V("SWEEP ST", &s->sweep, I8, -48, 48, 1, NULL);
            V("VIBRATO ST", &s->vibrato, U8, 0, 100, 1, NULL);
        }
        V("ATTACK MS", &s->attack, U16, 0, 8000, 5, NULL);
        V("RELEASE MS", &s->release, U16, 0, 8000, 10, NULL);
        V("LEVEL", &s->level, U8, 0, 100, 1, NULL);
        L("AMP ENVELOPE", UI_AMP);
        L("WAVE DETAILS", UI_OSC);
        L("VOICE / PITCH", UI_VOICE);
        if (s->model >= ST_SN_TWIN)
            L("SN CLOCK: BOTH", UI_SN_CLOCK);
        L("SAVE SOUND", UI_SAVE_SOUND);
        L("AUDITION", UI_PLAY);
        L("MACROS / LFO", UI_MACROS);
        break;
    case UI_OSC:
        if (s->model == ST_SAA_ENV || s->model == ST_SAA_AM) {
            V("WAVE", &s->shape, U8, 0, 7, 1, shape_names);
            V("STEPS", &s->steps, U8, 8, 16, 8, NULL);
            V("R INVERT", &s->invert, U8, 0, 1, 1, yesno);
            V("KEY RESET", &s->retrigger, U8, 0, 1, 1, yesno);
            if (s->model == ST_SAA_AM)
                V("ENV CLK ST", &s->noise_mix, U8, 0, 100, 1, NULL);
        } else if (s->model == ST_SAA_NOISE || s->model == ST_SAA_MIX || s->model >= ST_SN_NOISE) {
            if (s->model >= ST_SN_NOISE)
                V("WHITE NOISE", &s->shape, U8, 0, 1, 1, yesno);
            if (s->model != ST_SN_METAL)
                S("NOISE RATE", &s->noise_rate, U8, 0, 3, 1, noise_rates);
            if (s->model == ST_SN_METAL)
                V("TONE MIX", &s->noise_mix, U8, 0, 100, 1, NULL);
        }
        V("SWEEP ST", &s->sweep, I8, -48, 48, 1, NULL);
        S("ENGINE", &s->model, U8, 0, ST_MODELS - 1, 1, st_model_names);
        L("RESOURCE MAP", UI_RESOURCES);
        break;
    case UI_AMP:
        V("ATTACK MS", &s->attack, U16, 0, 8000, 5, NULL);
        V("DECAY MS", &s->decay, U16, 0, 8000, 10, NULL);
        V("SUSTAIN", &s->sustain, U8, 0, 100, 1, NULL);
        V("RELEASE MS", &s->release, U16, 0, 8000, 10, NULL);
        V("LEVEL", &s->level, U8, 0, 100, 1, NULL);
        V("PAN", &s->pan, U8, 0, 100, 1, NULL);
        break;
    case UI_VOICE:
        S("POLYPHONY", &p->poly, U8, 1, (int)studio_limit(s->model), 1, NULL);
        V("MODE", &s->mode, U8, 0, 2, 1, modes);
        V("GLIDE", &s->glide, U8, 0, 7, 1, glide_names);
        V("TRANSPOSE", &s->transpose, I8, -48, 48, 1, NULL);
        V("FINE CENT", &s->fine, I8, -50, 50, 1, NULL);
        V("DETUNE CENT", &s->detune, U8, 0, 50, 1, NULL);
        V("LOW NOTE", &p->low, U8, 0, 127, 1, NULL);
        V("HIGH NOTE", &p->high, U8, 0, 127, 1, NULL);
        V("VELOCITY", &p->velocity_curve, U8, 0, 3, 1, NULL);
        L("RESOURCE MAP", UI_RESOURCES);
        break;
    case UI_MACROS:
        V("MACRO", &u->macro, U8, 0, 3, 1, NULL);
        V("DESTINATION", &s->macro_dest[u->macro], U8, 0, ST_PARAMS - 1, 1, st_param_names);
        V("VALUE", &s->macro[u->macro], I8, -100, 100, 1, NULL);
        V("VIBRATO ST", &s->vibrato, U8, 0, 100, 1, NULL);
        V("TREMOLO", &s->tremolo, U8, 0, 100, 1, NULL);
        V("LFO RATE", &s->lfo_rate, U8, 0, 7, 1, rates);
        V("LFO SHAPE", &p->lfo_shape, U8, 0, 3, 1, lfo_names);
        break;
    case UI_ROUTE:
        L("USB MIDI", UI_USB);
        L("TRS MIDI", UI_TRS);
        A("CV / GATE 1", UI_CV);
        A("CV / GATE 2", UI_GATE);
        L("MOD MATRIX", UI_MOD);
        break;
    case UI_USB:
    case UI_TRS:
        u->target_part = (scene->route[u->page == UI_TRS][u->channel] & (1u << u->part)) != 0;
        V("CHANNEL", &u->channel, U8, 0, 15, 1, NULL);
        PART();
        V("TO PART", &u->target_part, U8, 0, 1, 1,
          yesno); /* virtual bit value; refreshed by ui_open/edit */
        V("LOW NOTE", &p->low, U8, 0, 127, 1, NULL);
        V("HIGH NOTE", &p->high, U8, 0, 127, 1, NULL);
        V("TRANSPOSE", &s->transpose, I8, -48, 48, 1, NULL);
        if (u->page == UI_TRS)
            V("TO USB", &e->preset.route[1].forward, U8, 0, 1, 1, yesno);
        break;
    case UI_CV: {
        StCv *c = &scene->cv[u->cv_input];
        V("MODE", &c->mode, U8, 0, 2, 1, cv_modes);
        if (c->mode == 2 && c->parameter >= ST_CLOCK_BASE) {
            L("GLOBAL BOTH SN", UI_SN_CLOCK);
        } else
            V("TO PART", &c->part, U8, 0, ST_PARTS - 1, 1, NULL);
        if (c->mode == 1) {
            V("0V NOTE", &c->base_note, U8, 0, 127, 1, NULL);
            V("SCALE", &c->scale, U8, 0, CV_SCALES - 1, 1, scales);
            V("ROOT", &c->root, U8, 0, 11, 1, NULL);
            V("GLIDE", &c->glide, U8, 0, 7, 1, glide_names);
        }
        if (c->mode == 2) {
            V("PARAMETER", &c->parameter, U8, 0, ST_PARAMS - 1, 1, st_param_names);
            V("AMOUNT", &c->amount, I8, -100, 100, 1, NULL);
            V("10V RANGE", &c->range, U8, 0, 1, 1, yesno);
        }
        L("GATE ROUTE", UI_GATE);
        break;
    }
    case UI_GATE: {
        StCv *c = &scene->cv[u->cv_input];
        V("FUNCTION", &c->gate, U8, 0, 4, 1, gates);
        V("TO PART", &c->gate_part, U8, 0, ST_PARTS - 1, 1, NULL);
        V("FIXED NOTE", &c->gate_note, U8, 0, 127, 1, NULL);
        L("CV ROUTE", UI_CV);
        break;
    }
    case UI_MOTION:
        PART();
        L("SN CLOCK: BOTH", UI_SN_CLOCK);
        L("ARPEGGIATOR", UI_ARP);
        L("SEQUENCER", UI_SEQ);
        L("MOD MATRIX", UI_MOD);
        L("TEMPO / SYNC", UI_TEMPO);
        break;
    case UI_SN_CLOCK:
        V("BASE", &e->preset.sn_reference, U8, 0, 2, 1, references);
        V("FX", &e->preset.sn_clock.mode, U8, 0, SN_CLOCK_MODES - 1, 1, clock_modes);
        V("BASE DROP ST", &e->preset.sn_clock.base, U8, 0, 48, 1, NULL);
        V("DEPTH ST", &e->preset.sn_clock.depth, U8, 0, 48, 1, NULL);
        V("RATE", &e->preset.sn_clock.rate, U8, 0, 15, 1, NULL);
        V("SYNC DIV", &e->preset.sn_clock.sync, U8, 0, 8, 1, NULL);
        V("RETRIGGER", &e->preset.sn_clock.retrigger, U8, 0, 1, 1, yesno);
        V("TRIGGER SRC", &e->preset.sn_trigger_source, U8, 0, 7, 1, NULL);
        L("CLOCK STEPS", UI_CLOCK_STEPS);
        break;
    case UI_SEQ:
        V("PART ENABLE", &p->sequence, U8, 0, 1, 1, yesno);
        V("TRANSPORT", &e->studio.running, U8, 0, 1, 1, yesno);
        V("LENGTH", &p->length, U8, 1, 16, 1, NULL);
        V("TICKS/STEP", &p->division, U8, 1, 24, 1, NULL);
        V("DIRECTION", &p->direction, U8, 0, 2, 1, dir_names);
        A("NOTE STEPS", A_GRID_NOTE);
        A("GATE STEPS", A_GRID_GATE);
        A("VEL STEPS", A_GRID_VEL);
        A("MACRO1 STEPS", A_GRID_MOD);
        break;
    case UI_ARP:
        V("MODE", &p->arp, U8, 0, 5, 1, arp_names);
        V("OCTAVES", &p->arp_octaves, U8, 1, 4, 1, NULL);
        V("TICKS/STEP", &p->division, U8, 1, 24, 1, NULL);
        break;
    case UI_MOD:
        V("SLOT", &u->mod_slot, U8, 0, ST_MODS - 1, 1, NULL);
        V("SOURCE", &scene->mod[u->mod_slot].source, U8, 0, 4 + ST_PARTS * 2, 1, NULL);
        V("TO PART", &scene->mod[u->mod_slot].part, U8, 0, ST_PARTS - 1, 1, NULL);
        V("PARAMETER", &scene->mod[u->mod_slot].parameter, U8, 0, ST_PARAMS - 1, 1, st_param_names);
        V("AMOUNT", &scene->mod[u->mod_slot].amount, I8, -100, 100, 1, NULL);
        break;
    case UI_TEMPO:
        V("BPM", &e->preset.tempo, U8, 0, 210, 1, NULL);
        V("CLOCK POLICY", &e->preset.clock_policy, U8, 0, 3, 1, NULL);
        V("MIDI SOURCE", &e->preset.clock_source, U8, 0, 1, 1, NULL);
        V("GATE CLOCK", &scene->gate_clock, U8, 0, 1, 1, NULL);
        break;
    case UI_SCENE:
        V("SLOT", &u->slot, U8, 0, PRESETS - 1, 1, NULL);
        A("SAVE SETUP", A_SAVE);
        A("LOAD SETUP", A_LOAD);
        A("NAME", A_NAME_SCENE);
        L("PART SETUP", UI_PARTS);
        L("SOUND LIBRARY", UI_LIBRARY);
        A("NEW MULTI SETUP", A_INIT);
        break;
    case UI_PARTS:
        PART();
        S("ENABLED", &p->enabled, U8, 0, 1, 1, yesno);
        S("MODEL", &s->model, U8, 0, ST_MODELS - 1, 1, st_model_names);
        S("POLYPHONY", &p->poly, U8, 1, (int)studio_limit(s->model), 1, NULL);
        A("PART NAME", A_NAME_PART);
        V("COPY TO", &u->target_part, U8, 0, ST_PARTS - 1, 1, NULL);
        A("COPY SOUND", A_COPY);
        L("RESOURCE MAP", UI_RESOURCES);
        break;
    case UI_LIBRARY:
        V("SLOT", &u->library_slot, U8, 0, ST_SOUNDS - 1, 1, NULL);
        A("LOAD SOUND", A_LOAD_SOUND);
        A("SAVE SOUND", A_SAVE_SOUND);
        A("NAME SOUND", A_NAME_SOUND);
        A("SAVE LIBRARY", A_SAVE_SYSTEM);
        break;
    case UI_MENU:
        L("EDIT SOUND", UI_SOUND);
        L("SAVE SOUND", UI_SAVE_SOUND);
        L("MIDI INPUT", UI_MIDI);
        L("PARTS / SETUP", UI_SETUP);
        L("TOOLS / AUDITION", UI_PLAY);
        L("SYSTEM", UI_SYSTEM);
        break;
    case UI_MIDI:
        V("RECEIVE CH", &u->channel, U8, 0, 15, 1, NULL);
        L("USB ROUTING", UI_USB);
        L("TRS ROUTING", UI_TRS);
        L("CV / GATE", UI_ROUTE);
        L("INPUT MONITOR", UI_MONITOR);
        break;
    case UI_SETUP:
        PART();
        A("SINGLE PLAY", A_SINGLE);
        L("PART SETUP", UI_PARTS);
        L("MIDI / CV ROUTING", UI_ROUTE);
        L("SAVE / LOAD SETUP", UI_SCENE);
        L("CHIP MIX", UI_MIX);
        L("MOTION / SEQUENCE", UI_MOTION);
        break;
    case UI_SAVE_SOUND:
        V("USER SLOT", &u->library_slot, U8, 0, ST_SOUNDS - 1, 1, NULL);
        A("SOUND NAME", A_NAME_PART);
        A("SAVE SOUND", A_SAVE_SOUND);
        L("FACTORY SOUNDS", UI_FACTORY);
        break;
    case UI_FACTORY:
        V("FACTORY", &u->factory_slot, U8, 0, ST_SOUNDS - 1, 1, NULL);
        A("LOAD FACTORY", A_FACTORY);
        break;
    case UI_SYSTEM:
        L("RESOURCE MAP", UI_RESOURCES);
        L("CV CALIBRATION", UI_CAL);
        L("INPUT MONITOR", UI_MONITOR);
        V("BRIGHTNESS", &u->settings->brightness, U8, 10, 100, 5, NULL);
        A("SAVE SYSTEM", A_SAVE_SYSTEM);
        A("USB UPDATE", A_BOOT);
        A("VERSION", A_VERSION);
        break;
    case UI_CAL:
        A("CV1 SET 0V", A_CAL0);
        A("CV1 SET 1V", A_CAL1);
        A("CV2 SET 0V", A_CAL2);
        A("CV2 SET 1V", A_CAL3);
        A("SAVE CAL", A_SAVE_SYSTEM);
        break;
    case UI_CLOCK_STEPS:
        V("STEP", &u->sequence_step, U8, 0, 7, 1, NULL);
        V("VALUE", &e->preset.clock_sequence[u->sequence_step], U8, 0, 100, 1, NULL);
        break;
    case UI_MIX:
        V("MASTER", &e->preset.mix[0], U8, 0, 100, 1, NULL);
        V("SAA", &e->preset.mix[1], U8, 0, 100, 1, NULL);
        V("SN LEFT", &e->preset.mix[2], U8, 0, 100, 1, NULL);
        V("SN RIGHT", &e->preset.mix[3], U8, 0, 100, 1, NULL);
        break;
    case UI_GRID:
        if (u->grid_lane == 0)
            V("NOTE", &p->note[u->grid_step], U8, 0, 127, 1, NULL);
        if (u->grid_lane == 1)
            V("GATE", &p->gate[u->grid_step], U8, 0, 100, 1, NULL);
        if (u->grid_lane == 2)
            V("VELOCITY", &p->velocity[u->grid_step], U8, 0, 127, 1, NULL);
        if (u->grid_lane == 3)
            V("MACRO 1", &p->motion[u->grid_step], I8, -100, 100, 1, NULL);
        break;
    default:
        break;
    }
#undef V
#undef S
#undef L
#undef A
#undef PART
    return n;
}
static Item scratch[32];
unsigned ui_count(Ui *u) {
    return build(u, scratch);
}
void ui_open(Ui *u, unsigned page) {
    if (page >= UI_PAGES)
        return;
    for (unsigned i = 1; i < u->depth; i++)
        if (u->path[i] == page) {
            u->depth = (uint8_t)i;
            u->page = (uint8_t)page;
            u->field = u->cursor[i];
            u->edit = false;
            return;
        }
    if (u->depth < 6) {
        u->path[u->depth] = u->page;
        u->cursor[u->depth] = u->field;
        u->depth++;
    }
    u->page = (uint8_t)page;
    if (page == UI_MIDI)
        u->channel = (uint8_t)receive_channel(u->engine, u->part);
    u->field = u->memory[page];
    u->edit = false;
    u->menu = true;
    unsigned count = ui_count(u);
    if (count && u->field >= count)
        u->field = 0;
}
void ui_init(Ui *u, Settings *settings, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx) {
    memset(u, 0, sizeof(*u));
    u->settings = settings;
    u->engine = e;
    u->save = save;
    u->calibrate = calibrate;
    u->ctx = ctx;
    u->slot = settings->selected;
    u->test_note = 48;
    e->library = settings;
    bool fresh = !e->preset.studio.enabled;
    studio_activate(e);
    u->active_sound = UINT8_MAX;
    if (fresh) {
        StSound sound;
        char name[9];
        studio_factory(0, &sound, name);
        e->preset.studio.part[0].sound = sound;
        memcpy(e->preset.studio.part[0].name, name, 9);
        (void)studio_single(e, 0, 0);
        /* Establish a clean RAM baseline; initialization never writes flash. */
        settings->preset[settings->selected] = e->preset;
    }
    for (unsigned p = 0; p < ST_PARTS; p++)
        if (e->preset.studio.part[p].enabled) {
            u->part = (uint8_t)p;
            break;
        }
    sync_sound(u);
    if (u->active_sound < ST_SOUNDS)
        u->library_slot = u->active_sound;
    for (unsigned i = 0; i < 2; i++)
        e->preset.route[i].channel = 0;
}
static bool set_value(Ui *u, const Item *item, int value) {
    Engine *e = u->engine;
    unsigned previous_part = u->part;
    uintptr_t ptr = (uintptr_t)item->ptr, begin = (uintptr_t)&e->preset.studio,
              end = begin + sizeof(StScene);
    if (ptr >= begin && ptr < end) {
        u->candidate = e->preset.studio;
        write_item(item, (uint8_t *)&u->candidate + (ptr - begin), value);
        if (item->ptr == &e->preset.studio.part[u->part].sound.model &&
            value != e->preset.studio.part[u->part].sound.model) {
            StPart *p = &u->candidate.part[u->part];
            StSound defaults;
            studio_sound_default(&defaults, (unsigned)value);
            memcpy(p->sound.macro_dest, defaults.macro_dest, 4);
            memset(p->sound.macro, 0, 4);
            p->poly =
                (uint8_t)(p->poly > studio_limit((unsigned)value) ? studio_limit((unsigned)value)
                                                                  : p->poly);
            if (value == ST_SAA_ENV || value == ST_SAA_AM)
                p->sound.shape = 5;
            else if (value >= ST_SN_NOISE) {
                p->sound.shape = value == ST_SN_METAL ? 0 : 1;
                if (value == ST_SN_METAL)
                    p->sound.noise_rate = 3;
            }
        }
        return studio_apply(e, &u->candidate);
    }
    if (item->ptr == &u->channel && u->page == UI_MIDI) {
        if (u->edit) {
            engine_panic(e, -1);
            for (unsigned src = 0; src < SOURCES; src++) {
                for (unsigned c = 0; c < 16; c++)
                    e->preset.studio.route[src][c] &= (uint16_t) ~(1u << u->part);
                e->preset.studio.route[src][value] |= (uint16_t)(1u << u->part);
            }
        }
    }
    write_item(item, item->ptr, value);
    if (item->ptr == &u->part && u->part != previous_part && e->studio.test_on)
        studio_test(e, u->part, u->test_note, u->test_mode);
    if (item->ptr == &u->settings->brightness)
        u->system_dirty = true;
    if (item->ptr == &u->target_part && (u->page == UI_USB || u->page == UI_TRS)) {
        uint16_t *route = &e->preset.studio.route[u->page == UI_TRS][u->channel];
        if (value)
            *route |= (uint16_t)(1u << u->part);
        else
            *route &= (uint16_t) ~(1u << u->part);
    }
    if (item->ptr == &e->studio.running)
        studio_run(e, value != 0);
    if (item->ptr == &u->test_mode || item->ptr == &u->test_note)
        studio_test(e, u->part, u->test_note, u->test_mode);
    return true;
}
static void back(Ui *u, uint32_t now) {
    if (u->confirm) {
        u->confirm = false;
        return;
    }
    if (u->edit) {
        Item *list = scratch;
        unsigned n = build(u, list);
        if (u->page == UI_NAME)
            u->name[u->name_cursor] = (char)u->original;
        else if (u->previewed && u->field < n)
            (void)set_value(u, &list[u->field], u->original);
        u->edit = false;
        u->previewed = false;
        u->dirty = u->was_dirty;
        u->system_dirty = u->was_system_dirty;
        return;
    }
    if (u->depth) {
        u->memory[u->page] = u->field;
        u->depth--;
        u->page = u->path[u->depth];
        u->field = u->cursor[u->depth];
        u->menu = u->depth != 0;
    }
    (void)now;
}
static void perform(Ui *u, unsigned action, uint32_t now) {
    Engine *e = u->engine;
    Settings *s = u->settings;
    switch (action) {
    case A_SELECT_SOUND:
        (void)select_sound(u, u->pending_sound, now);
        break;
    case A_FACTORY:
        (void)select_sound(u, ST_SOUNDS + u->factory_slot, now);
        break;
    case A_SINGLE:
        if (studio_single(e, u->part, receive_channel(e, u->part))) {
            u->test_mode = 0;
            u->dirty = true;
            toast(u, "SINGLE MIDI PLAY", now);
        }
        break;
    case A_PANIC:
        engine_panic(e, -1);
        u->test_mode = 0;
        u->edit = u->confirm = u->previewed = false;
        toast(u, "ALL NOTES OFF", now);
        return;
    case A_SAVE: {
        u->undo = s->preset[u->slot];
        uint8_t old = s->selected;
        s->preset[u->slot] = e->preset;
        s->selected = u->slot;
        if (u->save && u->save(u->ctx)) {
            u->dirty = false;
            u->system_dirty = false;
            toast(u, "SETUP SAVED", now);
        } else {
            s->preset[u->slot] = u->undo;
            s->selected = old;
            toast(u, "SAVE FAILED", now);
        }
        break;
    }
    case A_LOAD:
        engine_preset(e, &s->preset[u->slot]);
        studio_activate(e);
        s->selected = u->slot;
        u->test_mode = 0;
        u->dirty = false;
        u->active_sound = UINT8_MAX;
        for (unsigned p = 0; p < ST_PARTS; p++)
            if (e->preset.studio.part[p].enabled) {
                u->part = (uint8_t)p;
                break;
            }
        sync_sound(u);
        toast(u, "SETUP LOADED", now);
        break;
    case A_INIT:
        u->candidate = e->preset.studio;
        studio_default(&u->candidate);
        u->candidate.enabled = 1;
        (void)studio_apply(e, &u->candidate);
        u->dirty = true;
        toast(u, "NEW MULTI SETUP", now);
        break;
    case A_SAVE_SOUND: {
        StSound old = s->sounds[u->library_slot];
        char name[9];
        memcpy(name, s->sound_name[u->library_slot], 9);
        uint16_t used = s->sound_used;
        bool single = studio_is_single(e, u->part);
        u->undo = s->preset[s->selected];
        if (single)
            s->preset[s->selected] = e->preset;
        s->sounds[u->library_slot] = e->preset.studio.part[u->part].sound;
        memcpy(s->sound_name[u->library_slot], e->preset.studio.part[u->part].name, 9);
        s->sound_used |= (uint16_t)(1u << u->library_slot);
        if (u->save && u->save(u->ctx)) {
            u->system_dirty = false;
            u->active_sound = u->library_slot;
            if (single)
                u->dirty = false;
            toast(u, "SOUND SAVED", now);
        } else {
            s->sounds[u->library_slot] = old;
            memcpy(s->sound_name[u->library_slot], name, 9);
            s->sound_used = used;
            s->preset[s->selected] = u->undo;
            toast(u, "SAVE FAILED", now);
        }
        break;
    }
    case A_LOAD_SOUND:
        (void)select_sound(u, u->library_slot, now);
        break;
    case A_COPY:
        u->candidate = e->preset.studio;
        u->candidate.part[u->target_part].sound = e->preset.studio.part[u->part].sound;
        if (studio_apply(e, &u->candidate)) {
            u->dirty = true;
            toast(u, "SOUND COPIED", now);
        } else
            toast(u, "NO ROOM: RESOURCES", now);
        break;
    case A_SAVE_SYSTEM:
        if (u->save && u->save(u->ctx)) {
            u->system_dirty = false;
            toast(u, "SYSTEM SAVED", now);
        } else
            toast(u, "SAVE FAILED", now);
        break;
    case A_BOOT:
        engine_panic(e, -1);
        u->boot_requested = true;
        break;
    default:
        break;
    }
}
static void action(Ui *u, unsigned a, uint32_t now) {
    if (a == A_VERSION) {
        toast(u, "V0.6.1 ALPHA", now);
        return;
    }
    if (u->page == UI_ROUTE && (a == UI_CV || a == UI_GATE)) {
        u->cv_input = (uint8_t)(a == UI_GATE);
        ui_open(u, UI_CV);
        return;
    }
    if (a >= A_CAL0 && a <= A_CAL3) {
        unsigned k = a - A_CAL0;
        toast(u,
              u->calibrate && u->calibrate(u->ctx, k / 2, (k & 1) != 0) ? "CAL OK: SAVE SYSTEM"
                                                                        : "CAL FAILED",
              now);
        return;
    }
    if (a >= A_GRID_NOTE && a <= A_GRID_MOD) {
        u->grid_lane = (uint8_t)(a - A_GRID_NOTE);
        u->grid_step = 0;
        ui_open(u, UI_GRID);
        return;
    }
    if (a == A_NAME_SCENE || a == A_NAME_PART || a == A_NAME_SOUND) {
        u->name_target = (uint8_t)(a - A_NAME_SCENE);
        memcpy(u->name,
               a == A_NAME_SCENE  ? u->engine->preset.studio.name
               : a == A_NAME_PART ? u->engine->preset.studio.part[u->part].name
                                  : u->settings->sound_name[u->library_slot],
               9);
        if (!u->name[0])
            memcpy(u->name, "SOUND   ", 9);
        u->name_cursor = 0;
        ui_open(u, UI_NAME);
        return;
    }
    if (a == A_PANIC) {
        perform(u, a, now);
        return;
    }
    u->confirm = true;
    u->confirm_yes = false;
    u->confirm_action = (uint8_t)a;
}
void ui_click(Ui *u, unsigned button, bool long_press, uint32_t now) {
    if (button > 2)
        return;
    if (long_press && button == 1) {
        if (!u->depth && !u->confirm)
            ui_open(u, UI_MENU);
        else
            back(u, now);
        return;
    }
    int delta = button == 0 ? -1 : 1;
    if (u->confirm) {
        if (button == 1) {
            bool yes = u->confirm_yes;
            unsigned a = u->confirm_action;
            u->confirm = false;
            if (yes)
                perform(u, a, now);
        } else
            u->confirm_yes = !u->confirm_yes;
        return;
    }
    if (u->page == UI_NAME && u->depth) {
        static const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
        if (u->edit) {
            if (button == 1)
                u->edit = false;
            else {
                const char *p = strchr(chars, u->name[u->name_cursor]);
                int i = p ? (int)(p - chars) : 0;
                u->name[u->name_cursor] =
                    chars[(i + delta + (int)sizeof(chars) - 1) % ((int)sizeof(chars) - 1)];
            }
        } else if (button == 1) {
            if (u->name_cursor == 8) {
                if (u->name_target == 0)
                    memcpy(u->engine->preset.studio.name, u->name, 9);
                else if (u->name_target == 1)
                    memcpy(u->engine->preset.studio.part[u->part].name, u->name, 9);
                else {
                    memcpy(u->settings->sound_name[u->library_slot], u->name, 9);
                    u->system_dirty = true;
                }
                u->dirty = true;
                back(u, now);
            } else {
                u->edit = true;
                u->original = u->name[u->name_cursor];
            }
        } else
            u->name_cursor = (uint8_t)((u->name_cursor + delta + 9) % 9);
        return;
    }
    Item *list = scratch;
    unsigned count = build(u, list);
    if (u->edit) {
        if (u->field >= count) {
            u->edit = false;
            return;
        }
        Item *i = &list[u->field];
        if (button == 1) {
            if (set_value(u, i, u->draft)) {
                u->edit = false;
                u->dirty = true;
            } else {
                unsigned conflict = 0;
                char message[22];
                if (i->ptr == &u->engine->preset.studio.part[u->part].poly) {
                    unsigned max = (unsigned)u->draft;
                    while (max > 1) {
                        u->candidate.part[u->part].poly = (uint8_t)--max;
                        if (studio_plan(&u->candidate, NULL, NULL, NULL))
                            break;
                    }
                    snprintf(message, sizeof(message), "%c MAX %u VOICES", 'A' + u->part, max % 10);
                } else if (!studio_plan(&u->candidate, NULL, NULL, &conflict))
                    snprintf(message, sizeof(message), "NO ROOM: PART %c", 'A' + conflict);
                else
                    snprintf(message, sizeof(message), "INVALID SETTING");
                toast(u, message, now);
            }

        } else {
            int multiplier =
                (!i->names && i->max - i->min >= 100 && u->acceleration) ? u->acceleration : 1;
            int value = u->draft + delta * i->step * multiplier;
            if (value < i->min)
                value = i->min;
            if (value > i->max)
                value = i->max;
            u->draft = value;
            if (!i->structural && !(u->page == UI_MIDI && i->ptr == &u->channel) &&
                set_value(u, i, value))
                u->previewed = true;
        }
        return;
    }
    if (!u->depth) {
        if (button == 1) {
            ui_open(u, UI_SOUND);
        } else {
            sync_sound(u);
            unsigned current =
                u->active_sound < ST_SOUNDS ? u->active_sound : (delta > 0 ? ST_SOUNDS - 1 : 0);
            for (unsigned tries = 0; tries < ST_SOUNDS; tries++) {
                current = (unsigned)(((int)current + delta + ST_SOUNDS) % ST_SOUNDS);
                if (u->settings->sound_used & (1u << current)) {
                    if (sound_matches(u, u->active_sound))
                        (void)select_sound(u, current, now);
                    else {
                        u->pending_sound = (uint8_t)current;
                        action(u, A_SELECT_SOUND, now);
                    }
                    break;
                }
            }
        }
        return;
    }
    if (u->page == UI_GRID && button != 1) {
        unsigned len = u->engine->preset.studio.part[u->part].length;
        u->grid_step = (uint8_t)((u->grid_step + delta + (int)len) % (int)len);
        return;
    }
    if (button != 1) {
        if (count)
            u->field = (uint8_t)((u->field + delta + (int)count) % (int)count);
        return;
    }
    if (!count)
        return;
    if (u->field >= count)
        u->field = 0;
    Item *i = &list[u->field];
    if (i->type == LINK) {
        ui_open(u, i->key);
        return;
    }
    if (i->type == ACTION) {
        action(u, i->key, now);
        return;
    }
    if ((u->page == UI_USB || u->page == UI_TRS) && i->ptr == &u->target_part)
        u->target_part =
            (u->engine->preset.studio.route[u->page == UI_TRS][u->channel] & (1u << u->part)) != 0;
    u->edit = true;
    u->previewed = false;
    u->was_dirty = u->dirty;
    u->was_system_dirty = u->system_dirty;
    u->original = read_item(i);
    u->undo = u->engine->preset;
    u->draft = u->original;
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
            perform(u, A_PANIC, now);
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
static void footer(Ui *u, const char *s) {
    line(u, 0, 53, 127, 53);
    text(u, 1, 56, s, 1, true);
}
static void header(Ui *u, const char *s) {
    char title[18];
    snprintf(title, sizeof(title), "%.17s", s);
    text(u, 0, 0, title, 1, true);
    if (u->engine->studio.test_on)
        text(u, 107, 0, "T", 1, true);
    if (u->dirty)
        text(u, 121, 0, "*", 1, true);
    line(u, 0, 10, 127, 10);
}
static void note_name(char out[12], int n) {
    static const char *const pitch[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                        "F#", "G",  "G#", "A",  "A#", "B"};
    snprintf(out, 12, "%s%d", pitch[n % 12], n / 12 - 1);
}
static void format(Ui *u, const Item *i, int v, char out[24]) {
    Engine *e = u->engine;
    if (i->names && v >= i->min && v <= i->max) {
        snprintf(out, 24, "%s", i->names[v - i->min]);
        return;
    }
    bool part_value = i->ptr == &u->part || (i->ptr == &u->target_part && u->page == UI_PARTS);
    if (u->page == UI_CV)
        part_value |= i->ptr == &e->preset.studio.cv[u->cv_input].part;
    if (u->page == UI_GATE)
        part_value |= i->ptr == &e->preset.studio.cv[u->cv_input].gate_part;
    if (u->page == UI_MOD)
        part_value |= i->ptr == &e->preset.studio.mod[u->mod_slot].part;
    if (part_value) {
        snprintf(out, 24, "%c%s", 'A' + v, e->preset.studio.part[v].enabled ? "" : " OFF");
        return;
    }
    if (!strcmp(i->label, "FIXED NOTE") || !strcmp(i->label, "TEST NOTE") ||
        !strcmp(i->label, "0V NOTE") || !strcmp(i->label, "LOW NOTE") ||
        !strcmp(i->label, "HIGH NOTE") || (u->page == UI_GRID && u->grid_lane == 0)) {
        char tmp[12];
        note_name(tmp, v);
        snprintf(out, 24, "%s", tmp);
        return;
    }
    if (i->ptr == &e->preset.tempo) {
        snprintf(out, 24, "%d BPM", v + 30);
        return;
    }
    if (i->ptr == &e->preset.clock_policy) {
        static const char *const names[] = {"AUTO", "EXTERNAL", "INTERNAL", "GATE 1/16"};
        snprintf(out, 24, "%s", names[v]);
        return;
    }
    if (i->ptr == &e->preset.clock_source) {
        snprintf(out, 24, "%s", v ? "TRS" : "USB");
        return;
    }
    if (u->page == UI_MOD && i->ptr == &e->preset.studio.mod[u->mod_slot].source) {
        if (!v)
            snprintf(out, 24, "OFF");
        else if (v <= 4) {
            static const char *const names[] = {"CV 1", "CV 2", "CC 1", "PRESSURE"};
            snprintf(out, 24, "%s", names[v - 1]);
        } else if (v < 5 + ST_PARTS)
            snprintf(out, 24, "LFO %c", 'A' + v - 5);
        else
            snprintf(out, 24, "STEP %c", 'A' + v - 5 - ST_PARTS);
        return;
    }
    if (i->ptr == &e->preset.sn_clock.sync) {
        static const char *const names[] = {"FREE", "1/96", "1/48", "1/32", "1/24",
                                            "1/16", "1/12", "1/8",  "1/4"};
        snprintf(out, 24, "%s", names[v]);
        return;
    }
    if (i->ptr == &e->preset.sn_trigger_source) {
        static const char *const names[] = {"ALL",  "MIDI", "CV1",   "CV2",
                                            "CH10", "TONE", "NOISE", "MELODY"};
        snprintf(out, 24, "%s", names[v]);
        return;
    }
    if (u->page == UI_OSC && !strcmp(i->label, "NOISE RATE")) {
        static const char *const names[] = {"FIX HIGH", "FIX MID", "FIX LOW", "TONE FOLLOW"};
        snprintf(out, 24, "%s", names[v]);
        return;
    }
    if (!strcmp(i->label, "ENV CLK ST")) {
        snprintf(out, 24, "%+d ST", v - 48);
        return;
    }
    if (i->ptr == &e->preset.sn_clock.rate) {
        unsigned rate = sn_clock_rate_millihz[v];
        if (rate < 1000)
            snprintf(out, 24, "0.%02u HZ", rate / 10);
        else
            snprintf(out, 24, "%u HZ", rate / 1000);
        return;
    }
    if (!strcmp(i->label, "TICKS/STEP")) {
        if (96 % v == 0)
            snprintf(out, 24, "1/%d", 96 / v);
        else
            snprintf(out, 24, "%d TICKS", v);
        return;
    }
    if (u->page == UI_VOICE && !strcmp(i->label, "VELOCITY")) {
        static const char *const names[] = {"NORMAL", "SOFT", "HARD", "FIXED 110"};
        snprintf(out, 24, "%s", names[v]);
        return;
    }
    bool one = i->ptr == &u->channel || i->ptr == &u->slot || i->ptr == &u->library_slot ||
               i->ptr == &u->factory_slot || i->ptr == &u->macro || i->ptr == &u->mod_slot ||
               i->ptr == &u->sequence_step || i->ptr == &e->preset.studio.gate_clock;
    snprintf(out, 24, "%d", v + (one ? 1 : 0));
}
static void waveform(Ui *u, int y, int height) {
    StSound s = u->engine->studio.effective[u->part];
    int previous = 0;
    for (int x = 0; x < 122; x++) {
        float phase = (float)(x % 61) / 61, value;
        if (s.model == ST_SAA_ENV || s.model == ST_SAA_AM) {
            switch (s.shape) {
            case 0:
                value = 0;
                break;
            case 1:
                value = 1;
                break;
            case 2:
            case 3:
                value = 1 - phase;
                break;
            case 4:
            case 5:
                value = 1 - fabsf(phase * 2 - 1);
                break;
            default:
                value = phase;
                break;
            }
            value = roundf(value * (s.steps - 1)) / (s.steps - 1);
        } else if (s.model == ST_SAA_NOISE || s.model >= ST_SN_NOISE)
            value = (float)((x * x * 19 + x * 73) % 101) / 100;
        else
            value = phase < .5f ? 1 : 0;
        int yy = y + (int)lroundf((1 - value) * (float)(height - 1));
        if (x)
            line(u, x + 1, previous, x + 2, yy);
        previous = yy;
    }
}
static const char *page_title(unsigned page) {
    static const char *const titles[] = {"PLAY",
                                         "SOUND",
                                         "ROUTE",
                                         "MOTION",
                                         "SETUP",
                                         "SYSTEM",
                                         "OSC",
                                         "AMP",
                                         "VOICE",
                                         "MACROS",
                                         "USB MIDI",
                                         "TRS MIDI",
                                         "CV",
                                         "GATE",
                                         "MOD MATRIX",
                                         "BOTH SN / CLOCK",
                                         "SEQUENCER",
                                         "STEP EDIT",
                                         "ARPEGGIATOR",
                                         "TEMPO / SYNC",
                                         "PART SETUP",
                                         "SOUND LIBRARY",
                                         "NAME",
                                         "VOICE RESERVATION",
                                         "CV CALIBRATION",
                                         "INPUT MONITOR",
                                         "SN CLOCK STEPS",
                                         "CHIP MIX",
                                         "MENU",
                                         "MIDI INPUT",
                                         "PARTS / SETUP",
                                         "SAVE SOUND",
                                         "FACTORY"};
    return page < UI_PAGES ? titles[page] : "PLAY";
}
void ui_render(Ui *u, const UiStatus *status, uint32_t now) {
    memset(u->frame, 0, sizeof(u->frame));
    Engine *e = u->engine;
    u->dirty = u->system_dirty ||
               memcmp(&e->preset, &u->settings->preset[u->settings->selected], sizeof(Preset)) != 0;
    char title[40], value[24];
    if (!e->studio.test_on)
        u->test_mode = 0;
    if (u->confirm) {
        header(u, "CONFIRM");
        const char *name = u->confirm_action == A_SAVE           ? "SAVE SETUP?"
                           : u->confirm_action == A_LOAD         ? "LOAD SETUP?"
                           : u->confirm_action == A_BOOT         ? "ENTER USB UPDATE?"
                           : u->confirm_action == A_LOAD_SOUND   ? "LOAD SOUND?"
                           : u->confirm_action == A_SELECT_SOUND ? "DISCARD SOUND EDITS?"
                           : u->confirm_action == A_SINGLE       ? "USE SINGLE PLAY?"
                           : u->confirm_action == A_FACTORY      ? "LOAD FACTORY SOUND?"
                           : u->confirm_action == A_SAVE_SOUND   ? "SAVE SOUND?"
                           : u->confirm_action == A_INIT         ? "NEW MULTI SETUP?"
                                                                 : "SAVE / APPLY?";
        text(u, 1, 15, name, 1, true);
        if (u->confirm_action == A_SELECT_SOUND)
            snprintf(title, sizeof(title), "%02u %.8s", u->pending_sound + 1,
                     u->settings->sound_name[u->pending_sound]);
        else if (u->confirm_action == A_SAVE_SOUND)
            snprintf(title, sizeof(title), "USER %02u OVERWRITE", u->library_slot + 1);
        else if (u->confirm_action == A_SINGLE)
            snprintf(title, sizeof(title), "OTHER PARTS / CV OFF");
        else
            title[0] = 0;
        text(u, 1, 25, title, 1, true);
        box(u, u->confirm_yes ? 72 : 6, 35, 48, 13, true);
        text(u, 18, 38, "NO", 1, u->confirm_yes);
        text(u, 84, 38, "YES", 1, !u->confirm_yes);
        footer(u, "OK:SET HOLD:CANCEL");
    } else if (!u->depth) {
        sync_sound(u);
        StPart *part = &e->preset.studio.part[u->part];
        bool single = studio_is_single(e, u->part);
        if (u->active_sound < ST_SOUNDS)
            snprintf(title, sizeof(title), "SOUND %02u / 16", u->active_sound + 1);
        else if (u->active_sound < ST_SOUNDS * 2)
            snprintf(title, sizeof(title), "FACTORY %02u", u->active_sound - ST_SOUNDS + 1);
        else
            snprintf(title, sizeof(title), "CUSTOM SOUND");
        /* A star here means this sound differs from its stored sound, rather
           than that some unrelated setup parameter has changed. */
        bool setup_dirty = u->dirty;
        u->dirty = !sound_matches(u, u->active_sound);
        header(u, title);
        u->dirty = setup_dirty;
        text(u, 2, 15, part->name, 2, true);
        snprintf(title, sizeof(title), "%s %s", st_model_names[part->sound.model],
                 single ? "" : "MULTI");
        text(u, 2, 33, title, 1, true);
        unsigned notes = studio_note_count(e, u->part);
        unsigned capacity = part->sound.mode ? 1 : part->poly;
        uint16_t channels = 0;
        for (unsigned c = 0; c < 16; c++)
            if ((e->preset.studio.route[0][c] | e->preset.studio.route[1][c]) & (1u << u->part))
                channels |= (uint16_t)(1u << c);
        if (!part->enabled)
            snprintf(title, sizeof(title), "PART OFF");
        else if (!channels)
            snprintf(title, sizeof(title), "NO MIDI");
        else if (channels & (channels - 1))
            snprintf(title, sizeof(title), "MULTI %u/%u", notes, capacity);
        else
            snprintf(title, sizeof(title), "CH%u %u/%u", receive_channel(e, u->part) + 1, notes,
                     capacity);
        text(u, 2, 44, title, 1, true);
        unsigned src = 2;
        for (unsigned i = 0; i < 2; i++)
            if (e->studio.midi_seen[i] && e->now - e->studio.midi_time[i] < 350)
                src = i;
        if (src < 2)
            snprintf(title, sizeof(title), "%s CH%u", src ? "TRS" : "USB",
                     e->studio.midi_channel[src] + 1);
        else
            snprintf(title, sizeof(title), "%s", status->usb ? "USB+TRS" : "TRS MIDI");
        text(u, 78, 44, title, 1, true);
        footer(u, (now / 3000) % 2 ? "HOLD OK:MENU" : "-+:SOUND OK:EDIT");
    } else if (u->page == UI_NAME) {
        header(u, "NAME");
        text(u, 4, 21, u->name, 2, true);
        if (u->name_cursor < 8)
            line(u, 4 + u->name_cursor * 12, 39, 13 + u->name_cursor * 12, 39);
        else
            box(u, 98, 42, 28, 9, true);
        text(u, 100, 43, "DONE", 1, u->name_cursor != 8);
        footer(u, u->edit ? "OK:SET HOLD:CANCEL" : "OK:EDIT HOLD:BACK");
    } else if (u->page == UI_RESOURCES) {
        header(u, "VOICE RESERVATION");
        for (unsigned row = 0; row < 3; row++) {
            text(u, 0, 15 + (int)row * 13, row == 0 ? "SAA" : row == 1 ? "SNL" : "SNR", 1, true);
            unsigned count = row ? 4 : 6;
            for (unsigned i = 0; i < count; i++) {
                unsigned v = row == 0   ? i
                             : row == 1 ? (i == 3 ? 12 : 6 + i)
                                        : (i == 3 ? 13 : 9 + i);
                unsigned p = studio_owner(e, v);
                int x = 28 + (int)i * (row ? 21 : 16), y = 14 + (int)row * 13;
                box(u, x, y, row ? 18 : 13, 10, true);
                char label[2] = {(char)(p < ST_PARTS
                                            ? ((e->studio.outputs[p] & (1u << v)) ? 'A' + p : 'K')
                                            : '-'),
                                 0};
                text(u, x + (row ? 6 : 4), y + 2, label, 1, false);
                if (e->voice[v].active)
                    pixel(u, x, y + 10, true);
            }
        }
        footer(u, "K:CLOCK DOT:ACTIVE");
    } else if (u->page == UI_MONITOR) {
        header(u, "INPUT MONITOR");
        snprintf(title, sizeof(title), "USB %s ADC %s", status->usb ? "ON" : "OFF",
                 status->adc_ok ? "OK" : "--");
        text(u, 0, 15, title, 1, true);
        for (unsigned i = 0; i < 2; i++) {
            snprintf(title, sizeof(title), "CV%u %6d G%u %s", i + 1, status->cv_raw[i], i + 1,
                     status->gates[i] ? "ON" : "OFF");
            text(u, 0, 27 + (int)i * 12, title, 1, true);
        }
        footer(u, "HOLD:BACK");
    } else {
        Item *list = scratch;
        unsigned count = build(u, list);
        if (count && u->field >= count)
            u->field = 0;
        if (studio_is_single(e, u->part))
            snprintf(title, sizeof(title), "%s", page_title(u->page));
        else
            snprintf(title, sizeof(title), "%c / %s", 'A' + u->part, page_title(u->page));
        if (u->page == UI_FACTORY) {
            StSound sound;
            char name[9];
            studio_factory(u->factory_slot, &sound, name);
            snprintf(title, sizeof(title), "F%02u %.8s", u->factory_slot + 1, name);
        }
        if (u->page == UI_SAVE_SOUND)
            snprintf(title, sizeof(title), "SAVE %.8s", e->preset.studio.part[u->part].name);
        if (u->page == UI_LIBRARY)
            snprintf(title, sizeof(title), "L%02u %.8s", u->library_slot + 1,
                     u->settings->sound_name[u->library_slot]);
        if (u->page == UI_SCENE)
            snprintf(title, sizeof(title), "S%02u %.8s", u->slot + 1,
                     u->settings->preset[u->slot].studio.enabled
                         ? u->settings->preset[u->slot].studio.name
                         : "NEW     ");
        if (u->page == UI_CV || u->page == UI_GATE)
            snprintf(title, sizeof(title), "%s %u", page_title(u->page), u->cv_input + 1);
        if (u->page == UI_SN_CLOCK || u->page == UI_CLOCK_STEPS)
            snprintf(title, sizeof(title), "BOTH SN / CLOCK");
        if (u->page == UI_GRID) {
            snprintf(title, sizeof(title), "%c STEP %02u/%02u", 'A' + u->part, u->grid_step + 1,
                     e->preset.studio.part[u->part].length);
            header(u, title);
            Item *i = &list[0];
            format(u, i, u->edit ? u->draft : read_item(i), value);
            text(u, 1, 17, value, strlen(value) < 10 ? 2 : 1, true);
            for (unsigned k = 0; k < e->preset.studio.part[u->part].length; k++) {
                int x = 4 + (int)(k % 8) * 15, y = k < 8 ? 36 : 45;
                box(u, x, y, 10, 5, true);
                if (k != u->grid_step)
                    box(u, x + 1, y + 1, 8, 3, false);
            }
            footer(u, u->edit ? "OK:SET HOLD:CANCEL" : "OK:EDIT HOLD:BACK");
        } else if (u->edit && count) {
            Item *i = &list[u->field];
            if (studio_is_single(e, u->part))
                snprintf(title, sizeof(title), "%s", i->label);
            else
                snprintf(title, sizeof(title), "%c:%s", 'A' + u->part, i->label);
            if (u->page == UI_SN_CLOCK)
                snprintf(title, sizeof(title), "BOTH SN:%s", i->label);
            if (i->ptr == &u->library_slot)
                snprintf(title, sizeof(title), "L%02u %.8s", u->library_slot + 1,
                         u->settings->sound_name[u->library_slot]);
            if (i->ptr == &u->factory_slot) {
                StSound sound;
                char name[9];
                studio_factory(u->factory_slot, &sound, name);
                snprintf(title, sizeof(title), "F%02u %.8s", u->factory_slot + 1, name);
            }
            if (i->ptr == &u->slot)
                snprintf(title, sizeof(title), "S%02u %.8s", u->slot + 1,
                         u->settings->preset[u->slot].studio.enabled
                             ? u->settings->preset[u->slot].studio.name
                             : "NEW     ");
            header(u, title);
            format(u, i, u->draft, value);
            text(u, 1, 17, value, strlen(value) < 11 ? 2 : 1, true);
            if (u->page == UI_OSC)
                waveform(u, 35, 16);
            else if (u->page == UI_AMP) {
                StSound *s = &e->preset.studio.part[u->part].sound;
                int a = 5 + (int)(s->attack / 320), d = a + 8 + (int)(s->decay / 500),
                    sustain = 51 - (int)(s->sustain * 17 / 100);
                if (d > 75)
                    d = 75;
                line(u, 3, 50, a, 32);
                line(u, a, 32, d, sustain);
                line(u, d, sustain, 90, sustain);
                line(u, 90, sustain, 123, 50);
            } else if (u->page == UI_SN_CLOCK) {
                int prev = 0;
                for (int x = 0; x < 122; x++) {
                    float phase = (float)x / 121, v = e->preset.sn_clock.mode == SN_CLOCK_OFF ? .5f
                                                      : e->preset.sn_clock.mode == SN_CLOCK_DIVE
                                                          ? phase
                                                          : 1 - fabsf(phase * 2 - 1);
                    int yy = 34 + (int)(v * 15);
                    if (x)
                        line(u, x + 1, prev, x + 2, yy);
                    prev = yy;
                }
            } else if (i->max > i->min)
                box(u, 2, 43, (u->draft - i->min) * 123 / (i->max - i->min), 5, true);
            footer(u, "OK:SET HOLD:CANCEL");
        } else {
            header(u, title);
            unsigned start = u->field ? u->field - 1 : 0;
            if (count > 3 && start > count - 3)
                start = count - 3;
            for (unsigned k = start; k < count && k < start + 3; k++) {
                Item *i = &list[k];
                int y = 15 + (int)(k - start) * 12;
                bool selected = k == u->field;
                if (selected)
                    box(u, 0, y - 1, 128, 10, true);
                if (i->type == LINK || i->type == ACTION) {
                    snprintf(title, sizeof(title), "%.20s", i->label);
                    text(u, 2, y, title, 1, !selected);
                    if (i->type == LINK)
                        text(u, 120, y, ">", 1, !selected);
                } else {
                    format(u, i, read_item(i), value);
                    unsigned width = (unsigned)strlen(value);
                    if (width > 10) {
                        value[10] = 0;
                        width = 10;
                    }
                    unsigned maxlabel = 20 - width;
                    snprintf(title, sizeof(title), "%.*s", (int)maxlabel, i->label);
                    text(u, 2, y, title, 1, !selected);
                    text(u, 126 - (int)width * 6, y, value, 1, !selected);
                }
            }
            footer(u, "OK:OPEN HOLD:BACK");
        }
    }
    if (u->toast_until && (int32_t)(u->toast_until - now) > 0) {
        box(u, 1, 23, 126, 15, false);
        line(u, 1, 23, 126, 23);
        line(u, 1, 37, 126, 37);
        text(u, (128 - (int)strlen(u->toast) * 6) / 2, 27, u->toast, 1, true);
    }
}
