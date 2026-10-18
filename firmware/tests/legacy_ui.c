/* v0.4 UI fixture for legacy engine regressions; production UI is src/ui.c. */
#include "ui.h"
#include <stdio.h>
#include <string.h>
static const char *groups[] = {"MIX",         "USB MIDI", "TRS MIDI",  "CV/GATE 1",
                               "CV/GATE 2",   "PRESET",   "ARPEGGIO",  "CALIBRATE",
                               "DIAGNOSTICS", "SN CLOCK", "TONE EDIT", "CLOCK STEPS"};
#define GROUPS (sizeof(groups) / sizeof(groups[0]))
_Static_assert(GROUPS == UI_GROUPS, "UI group inventory must stay in sync");
static const char *clock_modes[] = {"OFF",    "MANUAL", "RISE",     "TRIANGLE",
                                    "SQUARE", "RANDOM", "SEQUENCE", "DIVE"};
static const char *targets[] = {"AUTO",       "SAA1099",   "SN1 LEFT",      "SN2 RIGHT",
                                "LAYER MONO", "SN STEREO", "SAA+SN STEREO", "OFF"};
static const uint8_t counts[] = {4, 11, 12, 10, 10, 4, 6, 4, 5, 11, 7, 2};
static const char *cv_roles[] = {"PITCH 1V/OCT", "SN BASE DROP", "SN FX DEPTH",  "SN PAIR WIDTH",
                                 "MASTER LEVEL", "SAA PAN",      "TREMOLO DEPTH"};
static const char *cv_scales[] = {"OFF",    "CHROMATIC",   "MAJOR",       "NATURAL MINOR",
                                  "DORIAN", "MINOR PENTA", "MAJOR PENTA", "WHOLE TONE"};
static const unsigned glide_ms[] = {0, 10, 30, 60, 120, 250, 500, 1000};
static const char *arp_names[] = {"OFF",     "MAJOR",   "MINOR",   "OCTAVE",
                                  "MAJOR 7", "MINOR 7", "MINOR 6", "POWER"};
/* A small 5x7 alphabet, column bits from top to bottom. */
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
static void text(Ui *u, int x, int y, const char *s) {
    for (; *s && x < 124; s++, x += 6) {
        uint8_t g[5] = {0};
        char c = *s;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c >= '0' && c <= '9')
            memcpy(g, glyph[c - '0'], 5);
        else if (c >= 'A' && c <= 'Z')
            memcpy(g, glyph[10 + c - 'A'], 5);
        else if (c == '#') {
            g[0] = 0x14;
            g[1] = 0x7f;
            g[2] = 0x14;
            g[3] = 0x7f;
            g[4] = 0x14;
        } else if (c == '-')
            g[0] = g[1] = g[2] = g[3] = g[4] = 8;
        else if (c == '.')
            g[2] = 0x40;
        else if (c == ':')
            g[2] = 0x14;
        else if (c == '/') {
            g[0] = 0x40;
            g[1] = 0x20;
            g[2] = 0x10;
            g[3] = 8;
            g[4] = 4;
        } else if (c == '>') {
            g[1] = 0x41;
            g[2] = 0x22;
            g[3] = 0x14;
            g[4] = 8;
        } else if (c == '+') {
            g[1] = 8;
            g[2] = 0x3e;
            g[3] = 8;
        }
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] & (1 << row)) {
                    int yy = y + row;
                    if (x + col >= 0 && x + col < 128 && yy >= 0 && yy < 64)
                        u->frame[(yy / 8) * 128 + x + col] |= (uint8_t)(1 << (yy % 8));
                }
    }
}
static void toast(Ui *u, const char *s, uint32_t now) {
    snprintf(u->toast, sizeof(u->toast), "%s", s);
    u->toast_until = now + 1600;
}
void ui_init(Ui *u, Settings *s, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx) {
    memset(u, 0, sizeof(*u));
    u->settings = s;
    u->engine = e;
    u->save = save;
    u->calibrate = calibrate;
    u->ctx = ctx;
}
static uint8_t *value(Ui *u, uint8_t *maximum, const char **label) {
    Preset *p = &u->engine->preset;
    uint8_t f = u->field;
    *maximum = 100;
    if (u->group == 0) {
        static const char *names[] = {"MASTER", "SAA MIX", "SN1 MIX", "SN2 MIX"};
        *label = names[f];
        return &p->mix[f];
    }
    if (u->group == 1 || u->group == 2) {
        Route *r = &p->route[u->group - 1];
        if (f == 0) {
            *label = "CHANNEL";
            *maximum = 16;
            return &r->channel;
        }
        if (f == 1) {
            *label = "TARGET";
            *maximum = TARGET_COUNT - 1;
            return &r->target;
        }
        if (f == 2) {
            *label = "PATCH";
            *maximum = PATCHES - 1;
            return &r->program;
        }
        unsigned source = u->group - 1;
        if (f == (source == 0 ? 3 : 7)) {
            *label = "CHANNEL MAP";
            *maximum = 1;
            return &p->channel_map[source];
        }
        if (f == 3) {
            *label = "TO USB";
            *maximum = 1;
            return &r->forward;
        }
        if (f == 4) {
            *label = "EDIT CHANNEL";
            *maximum = 15;
            return &u->route_channel[source];
        }
        MidiPart *part = &p->part[source][u->route_channel[source]];
        if (f == 5) {
            *label = "PART TARGET";
            *maximum = TARGET_COUNT - 1;
            return &part->target;
        }
        if (f == 6) {
            *label = "PART PATCH";
            *maximum = PATCHES - 1;
            return &part->program;
        }
        unsigned index = f - (source == 0 ? 7u : 8u);
        PartPerformance *performance = &p->performance[source][u->route_channel[source]];
        static const char *names[] = {"PART TRANSPOSE", "LOW NOTE", "HIGH NOTE", "VELOCITY"};
        *label = names[index];
        *maximum = index == 0 ? 48 : index == 3 ? 3 : 127;
        return index == 0   ? &performance->transpose
               : index == 1 ? &performance->low
               : index == 2 ? &performance->high
                            : &performance->velocity;
    }
    if (u->group == 3 || u->group == 4) {
        CvConfig *cv = &p->cv[u->group - 3];
        if (f == 0) {
            *label = "ENABLE";
            *maximum = 1;
            return &cv->enabled;
        }
        if (f == 1) {
            *label = "VOICE";
            *maximum = VOICES - 1;
            return &cv->voice;
        }
        if (f == 2) {
            *label = "PATCH";
            *maximum = PATCHES - 1;
            return &cv->program;
        }
        if (f == 3) {
            *label = "0V NOTE";
            *maximum = 127;
            return &cv->base_note;
        }
        unsigned input = u->group - 3;
        static const char *names[] = {"SN STEREO",  "CV ROLE", "SCALE",
                                      "SCALE ROOT", "GLIDE",   "MOD RANGE"};
        *label = names[f - 4];
        *maximum = f == 5 ? CV_ROLES - 1 : f == 6 ? CV_SCALES - 1 : f == 7 ? 11 : f == 8 ? 7 : 1;
        return f == 4   ? &p->cv_pair[input]
               : f == 5 ? &p->cv_role[input]
               : f == 6 ? &p->cv_scale[input]
               : f == 7 ? &p->cv_root[input]
               : f == 8 ? &p->cv_glide[input]
                        : &p->cv_range[input];
    }
    if (u->group == 5) {
        *maximum = PRESETS - 1;
        *label = "SLOT";
        return f == 0 ? &u->settings->selected : NULL;
    }
    if (u->group == 6) {
        if (f == 4) {
            *label = "INTERNAL BPM";
            *maximum = 210;
            return &p->tempo;
        }
        if (f == 5) {
            *label = "CLOCK MODE";
            *maximum = 2;
            return &p->clock_policy;
        }
        if (f == 3) {
            *label = "APPLY TO";
            *maximum = 2;
            return &p->arp_target;
        }
        if (f == 2) {
            *maximum = 1;
            *label = "CLOCK USB/TRS";
            return &p->clock_source;
        }
        *maximum = f == 0 ? ARP_MODES - 1 : 16;
        *label = f == 0 ? "CHORD" : "CLOCKS/STEP";
        return f == 0 ? &p->arp_mode : &p->arp_rate;
    }
    if (u->group == 9) {
        SnClockConfig *clock = &p->sn_clock;
        static const char *names[] = {
            "MODE",           "BASE DROP",       "DEPTH",   "RATE",      "MIDI CLOCKS/CYCLE",
            "NOTE RETRIGGER", "CLOCK/CC SOURCE", "TRIGGER", "REFERENCE", "PAIR WIDTH",
            "TRIGGER FROM"};
        *label = names[f];
        *maximum = f == 0             ? SN_CLOCK_MODES - 1
                   : f == 1 || f == 2 ? 48
                   : f == 3           ? SN_CLOCK_RATES - 1
                   : f == 4           ? 8
                   : f == 8           ? 2
                   : f == 9           ? 30
                   : f == 10          ? 7
                                      : 1;
        switch (f) {
        case 0:
            return &clock->mode;
        case 1:
            return &clock->base;
        case 2:
            return &clock->depth;
        case 3:
            return &clock->rate;
        case 4:
            return &clock->sync;
        case 5:
            return &clock->retrigger;
        case 6:
            return &p->clock_source;
        case 8:
            return &p->sn_reference;
        case 9:
            return &p->sn_width;
        case 10:
            return &p->sn_trigger_source;
        default:
            return NULL;
        }
    }
    if (u->group == 10) {
        static const char *names[] = {"SOUND GROUP",   "ATTACK SCALE", "DECAY SCALE", "SUSTAIN",
                                      "RELEASE SCALE", "TREMOLO",      "TREMOLO RATE"};
        *label = names[f];
        *maximum = f == 0 ? 1 : f == 5 ? 100 : f == 6 ? 7 : 127;
        ToneControl *control = &p->tone[u->tone_chip];
        return f == 0   ? &u->tone_chip
               : f == 1 ? &control->attack
               : f == 2 ? &control->decay
               : f == 3 ? &control->sustain
               : f == 4 ? &control->release
               : f == 5 ? &control->tremolo
                        : &control->rate;
    }
    if (u->group == 11) {
        *label = f == 0 ? "STEP" : "DROP PERCENT";
        *maximum = f == 0 ? 7 : 100;
        return f == 0 ? &u->sequence_step : &p->clock_sequence[u->sequence_step];
    }
    *label = "ACTION";
    return NULL;
}
static void change(Ui *u, int delta, uint32_t now) {
    uint8_t max;
    const char *label;
    uint8_t *v = value(u, &max, &label);
    if (!v)
        return;
    Preset *p = &u->engine->preset;
    uint8_t old = *v;
    int next = (int)old + delta;
    int min = u->group == 6 && u->field == 1 ? 1 : 0;
    if (next < min)
        next = max;
    if (next > max)
        next = min;
    if (u->group == 10 && u->field == 3 && next > 100 && next < 127)
        next = delta > 0 ? 127 : 100;
    if (u->group == 0) {
        next = (int)old + delta * 5;
        if (next < 0)
            next = 0;
        if (next > 100)
            next = 100;
    }
    if ((u->group == 1 || u->group == 2) && (u->field == 2 || u->field == 6)) {
        unsigned source = u->group - 1;
        uint8_t target = u->field == 2 ? p->route[source].target
                                       : p->part[source][u->route_channel[source]].target;
        for (int n = 0; n < PATCHES && !engine_target_patch_ok(target, next); n++)
            next = (next + PATCHES + delta) % PATCHES;
    }
    if (u->group == 3 || u->group == 4) {
        unsigned input = u->group - 3;
        CvConfig old_cv = p->cv[input];
        uint8_t old_pair = p->cv_pair[input];
        CvConfig *cv = &p->cv[input];
        if (u->field == 1 && old_pair) {
            static const uint8_t choices[] = {6, 7, 8, 12};
            unsigned index = 0;
            while (index < 3 && choices[index] != old)
                index++;
            next = choices[(index + 4 + delta) % 4];
        }
        if (u->field == 2)
            for (int n = 0; n < PATCHES && !engine_cv_patch_ok(cv->voice, next); n++) {
                next = (next + PATCHES + delta) % PATCHES;
            }
        *v = (uint8_t)next;
        if (u->field == 4 && next) {
            if (cv->voice < 6) {
                p->cv_pair[input] = old_pair;
                toast(u, "SELECT SN VOICE", now);
                return;
            }
            if (cv->voice >= 9 && cv->voice < 12)
                cv->voice -= 3;
            if (cv->voice == 13)
                cv->voice = 12;
        }
        if (u->field == 1 && cv->voice < 6)
            p->cv_pair[input] = 0;
        if (!engine_cv_patch_ok(cv->voice, cv->program))
            cv->program = cv->voice >= 12 ? 8 : 0;
        if (!engine_cv_config_valid(p)) {
            p->cv[input] = old_cv;
            p->cv_pair[input] = old_pair;
            *v = old;
            toast(u, "RESOURCE IN USE", now);
            return;
        }
        engine_panic(u->engine, -1);
        return;
    }
    if ((u->group == 1 || u->group == 2) && u->field >= (u->group == 1 ? 7 : 8)) {
        unsigned source = u->group - 1;
        PartPerformance *performance = &p->performance[source][u->route_channel[source]];
        if ((v == &performance->low && next > performance->high) ||
            (v == &performance->high && next < performance->low)) {
            toast(u, "INVALID NOTE RANGE", now);
            return;
        }
    }
    if (u->group != 0 && u->group != 6 && u->group != 9 && u->group != 10 && u->group != 11 &&
        !((u->group == 1 || u->group == 2) && u->field == 4))
        engine_panic(u->engine, -1);
    if (u->group == 9 && u->field == 8)
        engine_panic(u->engine, -1);
    *v = (uint8_t)next;
    if ((u->group == 6 && (u->field == 2 || u->field == 5)) || (u->group == 9 && u->field == 6))
        engine_transport_reset(u->engine);
    if (u->group == 1 || u->group == 2) {
        unsigned source = u->group - 1;
        if (u->field == 1 &&
            !engine_target_patch_ok(p->route[source].target, p->route[source].program))
            p->route[source].program = 0;
        if (u->field == 5) {
            MidiPart *part = &p->part[source][u->route_channel[source]];
            if (!engine_target_patch_ok(part->target, part->program))
                part->program = 0;
        }
        if (u->field == 1 || u->field == 2 || u->field == 5 || u->field == 6 ||
            u->field == (source == 0 ? 3 : 7))
            engine_routes_reset(u->engine, source);
    }
}
static void action(Ui *u, uint32_t now) {
    if (u->group == 9 && u->field == 7) {
        u->engine->sn_manual_trigger++;
        toast(u, "CLOCK TRIGGERED", now);
        return;
    }
    if (u->group == 5 && u->field > 0) {
        unsigned slot = u->settings->selected;
        if (u->field == 1) {
            u->settings->preset[slot] = u->engine->preset;
            toast(u, u->save && u->save(u->ctx) ? "SAVED" : "SAVE FAILED", now);
        }
        if (u->field == 2) {
            engine_preset(u->engine, &u->settings->preset[slot]);
            toast(u, "LOADED", now);
        }
        if (u->field == 3) {
            static Settings fresh;
            settings_default(&fresh);
            engine_preset(u->engine, &fresh.preset[slot]);
            toast(u, "INIT - NOT SAVED", now);
        }
        return;
    }
    if (u->group == 7) {
        unsigned input = u->field / 2;
        bool one = u->field % 2;
        toast(u,
              u->calibrate && u->calibrate(u->ctx, input, one)
                  ? (one ? "CAL OK - SAVE SLOT" : "ZERO OK - APPLY 1V")
                  : "CAL FAILED",
              now);
        return;
    }
    if (u->group == 8) {
        if (u->field)
            return;
        u->engine->preset.reserve_cv = !u->engine->preset.reserve_cv;
        engine_panic(u->engine, -1);
        toast(u, u->engine->preset.reserve_cv ? "CV VOICES RESERVED" : "LAST NOTE WINS", now);
        return;
    }
    u->edit = !u->edit;
}
static void short_press(Ui *u, int b, uint32_t now) {
    if (!u->menu) {
        if (b == 1) {
            u->menu = true;
            u->field = 0;
        } else
            u->group = (uint8_t)((u->group + (b == 0 ? GROUPS - 1 : 1)) % GROUPS);
        return;
    }
    if (b == 1) {
        action(u, now);
        return;
    }
    int delta = b == 0 ? -1 : 1;
    if (u->edit)
        change(u, delta, now);
    else
        u->field = (uint8_t)((u->field + counts[u->group] + delta) % counts[u->group]);
}
void ui_buttons(Ui *u, const bool down[3], uint32_t now) {
    for (int b = 0; b < 3; b++) {
        if (down[b] != u->raw[b]) {
            u->raw[b] = down[b];
            u->changed[b] = now;
        }
        if (now - u->changed[b] >= 20 && u->stable[b] != u->raw[b]) {
            u->stable[b] = u->raw[b];
            if (u->stable[b]) {
                u->pressed[b] = now;
                u->repeat[b] = now;
                u->long_sent[b] = false;
            } else if (!u->long_sent[b])
                short_press(u, b, now);
        }
        if (u->stable[b] && !u->long_sent[b] && now - u->pressed[b] >= 700) {
            u->long_sent[b] = true;
            if (b == 0 && !u->edit) {
                engine_panic(u->engine, -1);
                toast(u, "PANIC", now);
            } else if (b == 1 || !u->edit) {
                u->edit = false;
                u->menu = false;
            } else
                short_press(u, b, now);
        }
        if (u->stable[b] && u->edit && b != 1 && now - u->pressed[b] >= 700 &&
            now - u->repeat[b] >= 100) {
            u->repeat[b] = now;
            short_press(u, b, now);
        }
    }
}
void ui_render(Ui *u, const UiStatus *s, uint32_t now) {
    memset(u->frame, 0, sizeof(u->frame));
    char line[64];
    text(u, 0, 0, groups[u->group]);
    for (int x = 0; x < 128; x++)
        u->frame[128 + x] = 1;
    if (!u->menu) {
        snprintf(line, sizeof(line), "NOTES %u VOICES %u", engine_active_notes(u->engine),
                 engine_active(u->engine));
        text(u, 0, 19, line);
        snprintf(line, sizeof(line), "USB %s  CV %s", s->usb ? "ON" : "OFF",
                 s->adc_ok ? "OK" : "ERR");
        text(u, 0, 31, line);
        text(u, 0, 43, "-/+ MENU  OK OPEN");
    } else if (u->group == 7) {
        static const char *a[] = {"CV1 APPLY 0V", "CV1 APPLY 1V", "CV2 APPLY 0V", "CV2 APPLY 1V"};
        text(u, 0, 20, a[u->field]);
        text(u, 0, 32, "STABLE SOURCE THEN OK");
        snprintf(line, sizeof(line), "RAW %d / %d", s->cv_raw[0], s->cv_raw[1]);
        text(u, 0, 44, line);
    } else if (u->group == 8) {
        if (u->field == 0) {
            snprintf(line, sizeof(line), "CV RESERVE %s",
                     u->engine->preset.reserve_cv ? "ON" : "OFF");
            text(u, 0, 20, line);
            text(u, 0, 34, "OK TO TOGGLE");
        } else if (u->field == 1) {
            snprintf(line, sizeof(line), "STEAL %lu", (unsigned long)u->engine->steals);
            text(u, 0, 19, line);
            snprintf(line, sizeof(line), "REFUSED %lu", (unsigned long)u->engine->refused);
            text(u, 0, 30, line);
            snprintf(line, sizeof(line), "PITCH CLAMP %lu", (unsigned long)u->engine->pitch_clamps);
            text(u, 0, 41, line);
        } else if (u->field == 2) {
            snprintf(line, sizeof(line), "CV %d / %d", s->cv_raw[0], s->cv_raw[1]);
            text(u, 0, 19, line);
            snprintf(line, sizeof(line), "GATE %u / %u", s->gates[0], s->gates[1]);
            text(u, 0, 32, line);
        } else if (u->field == 3) {
            snprintf(line, sizeof(line), "MAX TICK %luUS", (unsigned long)s->tick_us);
            text(u, 0, 19, line);
            snprintf(line, sizeof(line), "OVERRUN %lu", (unsigned long)s->tick_overruns);
            text(u, 0, 32, line);
        } else {
            snprintf(line, sizeof(line), "UART ERROR %lu", (unsigned long)s->uart_overflow);
            text(u, 0, 19, line);
            snprintf(line, sizeof(line), "USB DROP %lu", (unsigned long)s->usb_dropped);
            text(u, 0, 32, line);
        }
    } else {
        uint8_t max;
        const char *label;
        uint8_t *v = value(u, &max, &label);
        (void)max;
        if (u->group == 9 && u->field == 7) {
            text(u, 0, 23, "TRIGGER CLOCK");
            text(u, 0, 36, "OK TO EXECUTE");
        } else if (u->group == 5 && u->field > 0) {
            static const char *actions[] = {"SLOT", "SAVE ALL TO FLASH", "LOAD SLOT",
                                            "INIT SLOT IN RAM"};
            text(u, 0, 23, actions[u->field]);
            text(u, 0, 36, "OK TO EXECUTE");
        } else {
            text(u, 0, 20, label);
            if (v) {
                snprintf(line, sizeof(line), "%u", *v);
                if (u->group == 5)
                    snprintf(line, sizeof(line), "%u", *v + 1);
                if ((u->group == 1 || u->group == 2) && u->field == 0 && !*v)
                    snprintf(line, sizeof(line), "OMNI");
                if ((u->group == 1 || u->group == 2) && (u->field == 1 || u->field == 5))
                    snprintf(line, sizeof(line), "%s", targets[*v]);
                if ((u->field == 2 && u->group >= 1 && u->group <= 4) ||
                    (u->field == 6 && (u->group == 1 || u->group == 2)))
                    snprintf(line, sizeof(line), "%u %s", *v, patches[*v].name);
                if ((u->group == 2 && u->field == 3) ||
                    ((u->group == 3 || u->group == 4) && u->field == 0))
                    snprintf(line, sizeof(line), "%s", *v ? "ON" : "OFF");
                if ((u->group == 1 || u->group == 2) && u->field == 4)
                    snprintf(line, sizeof(line), "%u", *v + 1);
                if ((u->group == 1 && u->field == 3) || (u->group == 2 && u->field == 7))
                    snprintf(line, sizeof(line), "%s", *v ? "BY CHANNEL" : "WHOLE INPUT");
                if ((u->group == 3 || u->group == 4) && u->field == 4)
                    snprintf(line, sizeof(line), "%s", *v ? "PAIRED LEFT+RIGHT" : "SINGLE VOICE");
                if ((u->group == 3 || u->group == 4) && u->field == 1) {
                    if (u->engine->preset.cv_pair[u->group - 3]) {
                        if (*v == 12)
                            snprintf(line, sizeof(line), "12 SN NOISE PAIR");
                        else
                            snprintf(line, sizeof(line), "%u SN PAIR T%u", *v, *v - 5);
                    } else if (*v < 6)
                        snprintf(line, sizeof(line), "%u SAA T%u", *v, *v + 1);
                    else if (*v < 12)
                        snprintf(line, sizeof(line), "%u SN%u T%u", *v, (*v - 6) / 3 + 1,
                                 (*v - 6) % 3 + 1);
                    else
                        snprintf(line, sizeof(line), "%u SN%u NOISE", *v, *v - 11);
                }
                if ((u->group == 3 || u->group == 4) && u->field == 3) {
                    static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                                  "F#", "G",  "G#", "A",  "A#", "B"};
                    snprintf(line, sizeof(line), "%u %s%d", *v, names[*v % 12], *v / 12 - 1);
                }
                if (u->group == 6 && u->field == 0) {
                    snprintf(line, sizeof(line), "%s", arp_names[*v]);
                }
                if (u->group == 6 && u->field == 3) {
                    static const char *scopes[] = {"ALL", "SAA", "SN"};
                    snprintf(line, sizeof(line), "%s", scopes[*v]);
                }
                if (u->group == 6 && u->field == 2)
                    snprintf(line, sizeof(line), "%s", *v ? "TRS" : "USB");
                if ((u->group == 1 && u->field >= 7) || (u->group == 2 && u->field >= 8)) {
                    unsigned index = u->field - (u->group == 1 ? 7u : 8u);
                    if (index == 0)
                        snprintf(line, sizeof(line), "%+d SEMITONES", (int)*v - 24);
                    if (index == 3) {
                        static const char *names[] = {"LINEAR", "SOFT", "HARD", "FIXED 110"};
                        snprintf(line, sizeof(line), "%s", names[*v]);
                    }
                }
                if (u->group == 3 || u->group == 4) {
                    if (u->field == 5)
                        snprintf(line, sizeof(line), "%s", cv_roles[*v]);
                    if (u->field == 6)
                        snprintf(line, sizeof(line), "%s", cv_scales[*v]);
                    if (u->field == 7) {
                        static const char *names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                                      "F#", "G",  "G#", "A",  "A#", "B"};
                        snprintf(line, sizeof(line), "%s", names[*v]);
                    }
                    if (u->field == 8)
                        snprintf(line, sizeof(line), "%u MS", glide_ms[*v]);
                    if (u->field == 9)
                        snprintf(line, sizeof(line), "0-%u V", *v ? 10 : 5);
                }
                if (u->group == 6 && u->field == 4)
                    snprintf(line, sizeof(line), "%u BPM", *v + 30);
                if (u->group == 6 && u->field == 5) {
                    static const char *names[] = {"AUTO", "EXTERNAL", "INTERNAL"};
                    snprintf(line, sizeof(line), "%s", names[*v]);
                }
                if (u->group == 10) {
                    if (u->field == 0)
                        snprintf(line, sizeof(line), "%s", *v ? "SN1+SN2" : "SAA1099");
                    if (u->field == 1 || u->field == 2 || u->field == 4)
                        snprintf(line, sizeof(line), "%u PERCENT",
                                 engine_time_scale(*v) * 100 / 256);
                    if (u->field == 3 && *v == 127)
                        snprintf(line, sizeof(line), "PATCH VALUE");
                    if (u->field == 6) {
                        static const char *rate[] = {"0.5", "1", "2", "4", "8", "16", "32", "64"};
                        snprintf(line, sizeof(line), "%s HZ", rate[*v]);
                    }
                }
                if (u->group == 11 && u->field == 0)
                    snprintf(line, sizeof(line), "%u OF 8", *v + 1);
                if (u->group == 9) {
                    if (u->field == 0)
                        snprintf(line, sizeof(line), "%s", clock_modes[*v]);
                    if (u->field == 1)
                        snprintf(
                            line, sizeof(line), "%u SEMI %luKHZ", *v,
                            (unsigned long)(sn_clock_hz(sn_clock_half(
                                                (uint16_t)((*v +
                                                            u->engine->preset.sn_reference * 12u) *
                                                           256))) /
                                            1000));
                    if (u->field == 2)
                        snprintf(line, sizeof(line), "%u SEMITONES", *v);
                    if (u->field == 3) {
                        uint32_t rate = sn_clock_rate_millihz[*v];
                        snprintf(line, sizeof(line), "%lu.%02lu HZ", (unsigned long)(rate / 1000),
                                 (unsigned long)((rate % 1000) / 10));
                    }
                    if (u->field == 4)
                        snprintf(line, sizeof(line), "%s%u", *v ? "CLOCKS " : "FREE ",
                                 sn_clock_sync_clocks[*v]);
                    if (u->field == 5)
                        snprintf(line, sizeof(line), "%s", *v ? "ON" : "OFF");
                    if (u->field == 6)
                        snprintf(line, sizeof(line), "%s", *v ? "TRS" : "USB");
                    if (u->field == 8)
                        snprintf(
                            line, sizeof(line), "%lu MHZ",
                            (unsigned long)(engine_sn_reference(&u->engine->preset) / 1000000));
                    if (u->field == 9)
                        snprintf(line, sizeof(line), "%u CENTS TOTAL", *v);
                    if (u->field == 10) {
                        static const char *sources[] = {"ANY SN",   "MIDI SN",   "CV1",
                                                        "CV2",      "DRUM CH10", "SN TONES",
                                                        "SN NOISE", "MELODY/CV"};
                        snprintf(line, sizeof(line), "%s", sources[*v]);
                    }
                }
                text(u, 0, 34, line);
            }
            text(u, 0, 45, u->edit ? "> EDIT -/+  OK DONE" : "OK EDIT  -/+ SELECT");
        }
    }
    if ((int32_t)(u->toast_until - now) > 0) {
        for (int i = 7 * 128; i < 1024; i++)
            u->frame[i] = 0;
        text(u, 0, 56, u->toast);
    } else if (u->menu && (u->group == 1 || u->group == 2) && u->field >= 4) {
        unsigned source = u->group - 1;
        snprintf(line, sizeof(line), "CH %u MAP %s", u->route_channel[source] + 1,
                 u->engine->preset.channel_map[source] ? "ON" : "OFF");
        text(u, 0, 56, line);
    } else if (u->menu && u->group == 9) {
        snprintf(line, sizeof(line), "SN1+SN2 %luKHZ", (unsigned long)(s->clock_hz / 1000));
        text(u, 0, 56, line);
    } else
        text(u, 0, 56, "HOLD OK BACK - PANIC");
}
