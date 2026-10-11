#include "ui.h"
#include <stdio.h>
#include <string.h>
static const char *groups[] = {"MIX",    "USB MIDI", "TRS MIDI",  "CV/GATE 1",   "CV/GATE 2",
                               "PRESET", "ARPEGGIO", "CALIBRATE", "DIAGNOSTICS", "SN CLOCK"};
#define GROUPS (sizeof(groups) / sizeof(groups[0]))
static const char *clock_modes[] = {"OFF",    "MANUAL", "RISE",     "TRIANGLE",
                                    "SQUARE", "RANDOM", "SEQUENCE", "DIVE"};
static const char *targets[] = {"AUTO", "SAA1099", "SN1 LEFT", "SN2 RIGHT", "LAYER"};
static const uint8_t counts[] = {4, 3, 4, 4, 4, 4, 3, 4, 5, 8};
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
        *label = "TO USB";
        *maximum = 1;
        return &r->forward;
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
        *label = "0V NOTE";
        *maximum = 127;
        return &cv->base_note;
    }
    if (u->group == 5) {
        *maximum = PRESETS - 1;
        *label = "SLOT";
        return f == 0 ? &u->settings->selected : NULL;
    }
    if (u->group == 6) {
        if (f == 2) {
            *maximum = 1;
            *label = "CLOCK USB/TRS";
            return &p->clock_source;
        }
        *maximum = f == 0 ? 3 : 16;
        *label = f == 0 ? "CHORD" : "CLOCKS/STEP";
        return f == 0 ? &p->arp_mode : &p->arp_rate;
    }
    if (u->group == 9) {
        SnClockConfig *clock = &p->sn_clock;
        static const char *names[] = {
            "MODE",           "BASE DROP",       "DEPTH",  "RATE", "MIDI CLOCKS/CYCLE",
            "NOTE RETRIGGER", "CLOCK/CC SOURCE", "TRIGGER"};
        *label = names[f];
        *maximum = f == 0             ? SN_CLOCK_MODES - 1
                   : f == 1 || f == 2 ? 48
                   : f == 3           ? SN_CLOCK_RATES - 1
                   : f == 4           ? 8
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
        default:
            return NULL;
        }
    }
    *label = "ACTION";
    return NULL;
}
static bool cv_patch_ok(int v, int p) {
    if (patches[p].hw_env)
        return v == 2 || v == 5;
    if (v >= 12)
        return patches[p].noise == 1;
    if (v >= 6)
        return !patches[p].noise;
    return !(patches[p].noise && patches[p].noise_rate == 3);
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
    if (u->group == 0) {
        next = (int)old + delta * 5;
        if (next < 0)
            next = 0;
        if (next > 100)
            next = 100;
    }
    if (u->group == 3 || u->group == 4) {
        int input = u->group - 3;
        CvConfig *cv = &p->cv[input], *other = &p->cv[1 - input];
        if (u->field == 0 && next && other->enabled && cv->voice == other->voice) {
            toast(u, "VOICE IN USE", now);
            return;
        }
        if (u->field == 1 && other->enabled && next == other->voice) {
            next += delta;
            if (next < 0)
                next = max;
            if (next > max)
                next = 0;
        }
        if (u->field == 2) {
            for (int n = 0; n < PATCHES && !cv_patch_ok(cv->voice, next); n++) {
                next += delta;
                if (next < 0)
                    next = max;
                if (next > max)
                    next = 0;
            }
        }
    }
    if (u->group != 0 && u->group != 6 && u->group != 9)
        engine_panic(u->engine, -1);
    *v = (uint8_t)next;
    if (u->group == 3 || u->group == 4) {
        CvConfig *cv = &p->cv[u->group - 3];
        if (!cv_patch_ok(cv->voice, cv->program))
            cv->program = cv->voice >= 12 ? 8 : 0;
    }
    if ((u->group == 1 || u->group == 2) && u->field == 2)
        for (int c = 0; c < 16; c++)
            u->engine->channel[u->group - 1][c].program = *v;
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
            Settings fresh;
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
        snprintf(line, sizeof(line), "SLOT %u  VOICES %u", u->settings->selected + 1,
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
                if ((u->group == 1 || u->group == 2) && u->field == 1)
                    snprintf(line, sizeof(line), "%s", targets[*v]);
                if (u->field == 2 && u->group >= 1 && u->group <= 4)
                    snprintf(line, sizeof(line), "%u %s", *v, patches[*v].name);
                if ((u->group == 2 && u->field == 3) ||
                    ((u->group == 3 || u->group == 4) && u->field == 0))
                    snprintf(line, sizeof(line), "%s", *v ? "ON" : "OFF");
                if ((u->group == 3 || u->group == 4) && u->field == 1) {
                    if (*v < 6)
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
                    static const char *modes[] = {"OFF", "MAJOR", "MINOR", "OCTAVE"};
                    snprintf(line, sizeof(line), "%s", modes[*v]);
                }
                if (u->group == 6 && u->field == 2)
                    snprintf(line, sizeof(line), "%s", *v ? "TRS" : "USB");
                if (u->group == 9) {
                    if (u->field == 0)
                        snprintf(line, sizeof(line), "%s", clock_modes[*v]);
                    if (u->field == 1)
                        snprintf(line, sizeof(line), "%u SEMI %luKHZ", *v,
                                 (unsigned long)(sn_clock_hz(sn_clock_half((uint16_t)(*v * 256))) /
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
    } else if (u->menu && u->group == 9) {
        snprintf(line, sizeof(line), "SN1+SN2 %luKHZ", (unsigned long)(s->clock_hz / 1000));
        text(u, 0, 56, line);
    } else
        text(u, 0, 56, "HOLD OK BACK - PANIC");
}
