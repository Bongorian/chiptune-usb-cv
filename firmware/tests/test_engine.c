#include "engine.h"
#include "midi.h"
#include "store.h"
#include "ui.h"
#include "power.h"
#include "cv.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Engine e;
static Settings settings;
static uint8_t registers[32];
static uint8_t sn_bytes[4096];
static unsigned sn_count;
static uint16_t sn_registers[2][8];
static uint8_t sn_latch[2];
static void saa_log(void *ctx, uint8_t r, uint8_t value) {
    (void)ctx;
    assert(r < 32);
    registers[r] = value;
}
static void sn_log(void *ctx, uint8_t chip, uint8_t value) {
    (void)ctx;
    assert(chip < 2);
    if (value & 0x80) {
        sn_latch[chip] = (value >> 4) & 7;
        sn_registers[chip][sn_latch[chip]] =
            (uint16_t)((sn_registers[chip][sn_latch[chip]] & 0x3f0) | (value & 15));
    } else {
        unsigned r = sn_latch[chip];
        sn_registers[chip][r] = (uint16_t)((sn_registers[chip][r] & 15) | ((value & 63) << 4));
    }

    if (sn_count < sizeof(sn_bytes))
        sn_bytes[sn_count++] = value;
}
static void setup(void) {
    settings_default(&settings);
    sn_count = 0;
    engine_init(&e, &settings.preset[0], (SoundSink){saa_log, sn_log, NULL});
}
static void msg(int s, int status, int a, int b) {
    uint8_t data[] = {(uint8_t)status, (uint8_t)a, (uint8_t)b};
    engine_message(&e, (uint8_t)s, data, 3);
}
static int find(int source, int note) {
    for (int i = 0; i < VOICES; i++)
        if (e.voice[i].active && e.voice[i].source == source && e.voice[i].note == note)
            return i;
    return -1;
}
static void test_pitch(void) {
    bool clamped;
    assert(sn_divider(440, &clamped) == 284 && !clamped);
    assert(sn_divider(55, &clamped) == 1023 && clamped);
    assert(sn_divider(NAN, &clamped) == 1023 && clamped);
    assert(sn_attenuation(1) == 0);
    assert(sn_attenuation(0) == 15);
    assert(sn_attenuation(0.1f) == 10);
    for (int note = 24; note < 120; note++) {
        float hz = 440 * exp2f((note - 69) / 12.0f);
        uint8_t o, r;
        saa_frequency(hz, &o, &r, &clamped);
        float actual = 8000000.0f * (1u << o) / (512 * (511 - r));
        assert(o < 8);
        if (!clamped)
            assert(fabsf(1200 * log2f(actual / hz)) < 4.0f);
    }
}
static void test_midi_engine(void) {
    setup();
    msg(0, 0x90, 60, 100);
    int v = find(0, 60);
    assert(v >= 0);
    assert(v != 0 && v != 6);
    engine_tick(&e, 10);
    assert(engine_active(&e) == 1);
    msg(1, 0x80, 60, 0);
    assert(e.voice[v].held); /* separate sources */
    msg(0, 0xb0, 64, 127);
    msg(0, 0x80, 60, 0);
    assert(e.voice[v].sustained);
    msg(0, 0xb0, 64, 0);
    assert(e.voice[v].releasing);
    engine_tick(&e, 100);
    assert(!e.voice[v].active);
    setup();
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0x90, 60, 100);
    msg(0, 0x90, 60, 100);
    assert(engine_active(&e) == 2);
    msg(0, 0x80, 60, 0);
    unsigned held = 0;
    for (int i = 0; i < VOICES; i++)
        held += e.voice[i].active && e.voice[i].held;
    assert(held == 1);
    /* Exhaust SN1's two unreserved tone slots; an old NoteOff must not kill its replacement. */
    setup();
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0x90, 60, 100);
    msg(0, 0x90, 62, 100);
    msg(0, 0x90, 64, 100);
    assert(e.steals == 1);
    msg(0, 0x80, 60, 0);
    assert(find(0, 64) >= 0 && e.voice[find(0, 64)].held);
    msg(0, 0xb0, 120, 0);
    engine_tick(&e, 1);
    for (int n = 1; n < 8; n += 2)
        assert((sn_registers[0][n] & 15) == 15);
    assert(engine_active(&e) == 0);
    assert(e.sn_atten[0][0] == 15 && e.sn_atten[0][1] == 15 && e.sn_atten[0][2] == 15);
    setup();
    msg(0, 0x90, 69, 127);
    engine_tick(&e, 3);
    v = find(0, 69);
    uint8_t amp = registers[v];
    assert(amp);
    e.preset.mix[1] = 0;
    engine_tick(&e, 1);
    assert(registers[v] == 0); /* SAA mix is independent of master */
    setup();
    e.preset.route[0].channel = 2;
    msg(0, 0x90, 60, 100);
    assert(!engine_active(&e));
    msg(0, 0x91, 60, 100);
    assert(engine_active(&e));
}
static void test_resources(void) {
    setup();
    msg(0, 0xc0, 3, 0);
    msg(0, 0x90, 60, 100);
    assert(e.voice[2].active);
    assert(!e.voice[1].active);
    assert(registers[0x18] == 0x84);
    msg(0, 0xc0, 0, 0);
    for (int i = 0; i < 3; i++)
        msg(0, 0x90, 64 + i, 100);
    assert(!e.voice[1].active);
    setup();
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0xc0, 9, 0);
    msg(0, 0x90, 60, 100);
    assert(e.voice[12].active);
    assert(!e.voice[8].active);
    msg(0, 0xc0, 0, 0);
    msg(0, 0x90, 61, 100);
    assert(e.voice[7].active);
    msg(0, 0x90, 62, 100);
    assert(!e.voice[8].active);
    setup();
    e.preset.cv[1].voice = 8;
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0xc0, 9, 0);
    msg(0, 0x90, 60, 100);
    assert(e.refused == 1 && !e.voice[12].active);
    setup();
    e.preset.route[0].target = TARGET_SAA;
    msg(0, 0xc0, 7, 0);
    msg(0, 0x90, 60, 100);
    msg(0, 0xc0, 8, 0);
    msg(0, 0x90, 61, 100);
    assert(find(0, 60) / 3 != find(0, 61) / 3);
    setup();
    engine_cv(&e, 0, 60, true);
    assert(e.voice[0].source == 2);
    engine_tick(&e, 10);
    engine_cv(&e, 0, 64, true);
    assert(e.voice[0].pitch == 64);
    engine_cv(&e, 0, 64, false);
    assert(e.voice[0].releasing);
    engine_cv(&e, 0, 65, true);
    assert(e.voice[0].held && !e.voice[0].releasing);
    engine_panic(&e, -1);
    assert(!engine_active(&e));
}
static uint8_t messages[64][3], lengths[64], cins[64];
static unsigned count;
static void receive(void *ctx, const uint8_t *d, uint8_t len, uint8_t cin) {
    (void)ctx;
    assert(count < 64 && len <= 3);
    memcpy(messages[count], d, len);
    lengths[count] = len;
    cins[count++] = cin;
}
static void test_parser(void) {
    MidiParser p;
    count = 0;
    midi_init(&p, receive, NULL);
    uint8_t stream[] = {0x90, 60,   0xf8, 100, 61, 0xfe, 110, 0xf0, 1,    0xf8, 2,    3,
                        0xf7, 0xf2, 1,    2,   64, 0xc1, 7,   8,    0x91, 70,   0xf6, 120};
    for (unsigned i = 0; i < sizeof(stream); i++)
        midi_byte(&p, stream[i], i);
    assert(count == 11);
    assert(messages[1][0] == 0x90 && messages[1][1] == 60 && messages[1][2] == 100);
    assert(messages[3][0] == 0x90 && messages[3][1] == 61);
    assert(cins[5] == 4 && messages[5][0] == 0xf0);
    assert(cins[6] == 6 && messages[6][1] == 0xf7);
    assert(cins[7] == 3);
    assert(messages[8][0] == 0xc1 && messages[9][1] == 8);
    assert(midi_sensing_expired(&p, 400));
    assert(!midi_sensing_expired(&p, 401));
    assert(p.errors >= 2);
}
static void test_store(void) {
    Settings s;
    settings_default(&s);
    assert(settings_valid(&s));
    StoreRecord a, b;
    store_record(&a, &s, 10);
    store_record(&b, &s, 11);
    assert(store_select(&a, &b) == 1);
    for (unsigned i = 0; i < sizeof(b); i++) {
        StoreRecord corrupt = b;
        ((uint8_t *)&corrupt)[i] ^= 1;
        assert(!store_valid(&corrupt));
    }
    /* Simulate interrupted erase/program: old bank remains recoverable for every prefix. */
    for (unsigned prefix = 0; prefix < sizeof(b); prefix++) {
        StoreRecord partial;
        memset(&partial, 0xff, sizeof(partial));
        memcpy((uint8_t *)&partial + 4, (uint8_t *)&b + 4, prefix > 4 ? prefix - 4 : 0);
        assert(store_select(&a, &partial) == 0);
    }
    b.sequence = 9;
    store_record(&b, &s, 9);
    assert(store_select(&a, &b) == 0);
    store_record(&a, &s, UINT32_MAX);
    store_record(&b, &s, 0);
    assert(store_select(&a, &b) == 1);
    s.cv_codes_per_volt[0] = NAN;
    assert(!settings_valid(&s));
    settings_default(&s);
    s.preset[0].cv[1].voice = s.preset[0].cv[0].voice;
    assert(!settings_valid(&s));
}
static uint32_t random_state = 0x12345678;
static uint32_t random_u32(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}
static void test_random(void) {
    setup();
    MidiParser p;
    midi_init(&p, NULL, NULL);
    for (unsigned i = 0; i < 30000; i++) {
        uint32_t r = random_u32();
        int s = r & 1, kind = (r >> 1) % 6, c = (r >> 5) & 15, n = (r >> 9) & 127;
        if (kind < 2)
            msg(s, 0x90 | c, n, (r >> 16) & 127);
        else if (kind == 2)
            msg(s, 0x80 | c, n, 0);
        else if (kind == 3)
            msg(s, 0xb0 | c, ((r >> 16) & 1) ? 64 : 120, (r >> 17) & 127);
        else if (kind == 4)
            msg(s, 0xc0 | c, (r >> 16) % PATCHES, 0);
        else
            msg(s, 0xe0 | c, (r >> 16) & 127, (r >> 23) & 127);
        midi_byte(&p, (uint8_t)r, i);
        engine_tick(&e, 1);
        for (int v = 0; v < VOICES; v++)
            if (e.voice[v].active) {
                assert(e.voice[v].program < PATCHES);
                assert(isfinite(e.voice[v].level) && e.voice[v].level >= 0 &&
                       e.voice[v].level <= 1.001f);
            }
    }
    engine_panic(&e, -1);
    assert(engine_active(&e) == 0);
}
static void test_boundaries(void) {
    setup();
    e.preset.reserve_cv = 0;
    e.preset.route[0].target = TARGET_SAA;
    engine_cv(&e, 0, 60, true);
    msg(0, 0x90, 65, 100);
    /* Force a MIDI voice onto CV's slot, then continuous CV must leave it there. */
    e.voice[0] = e.voice[find(0, 65)];
    engine_cv_pitch(&e, 0, 90);
    assert(e.voice[0].source == 0 && e.voice[0].pitch == 65);
    engine_cv(&e, 0, 60, false);
    assert(e.voice[0].held);
    setup();
    e.preset.reserve_cv = 0;
    e.preset.route[0].target = TARGET_SAA;
    msg(0, 0xc0, 3, 0);
    msg(0, 0x90, 60, 100);
    assert(registers[0x18] == 0x84);
    msg(0, 0xc0, 0, 0);
    for (int i = 0; i < 5; i++)
        msg(0, 0x90, 61 + i, 100);
    assert(e.voice[2].program == 0 && registers[0x18] == 0); /* envelope disabled when stolen */
    setup();
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0xc0, 9, 0);
    msg(0, 0x90, 69, 127);
    engine_tick(&e, 10);
    assert(e.sn_div[0][2] == 19);
    assert(sn_registers[0][4] == 19 &&
           sn_registers[0][6] == 3); /* 15-step periodic ring compensation */
    msg(0, 0x90, 72, 100);
    assert(e.voice[12].note == 72 && e.refused == 0); /* own clock can be reused */
    setup();
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0xc0, 0, 0);
    msg(0, 0x90, 60, 100);
    msg(0, 0x90, 61, 100);
    assert(e.voice[8].active);
    msg(0, 0xc0, 9, 0);
    msg(0, 0x90, 48, 100);
    assert(e.voice[12].active && !e.voice[8].active);
    assert(e.steals == 1);
    assert(sn_attenuation(0.001f) == 15);
    setup();
    uint8_t clock = 0xf8, stop = 0xfc, start = 0xfa;
    engine_message(&e, 1, &clock, 1);
    assert(!e.clocks);
    engine_message(&e, 0, &clock, 1);
    assert(e.clocks == 1);
    engine_message(&e, 0, &stop, 1);
    engine_message(&e, 0, &clock, 1);
    assert(e.clocks == 1);
    engine_message(&e, 0, &start, 1);
    engine_message(&e, 0, &clock, 1);
    assert(e.clocks == 1 && e.clock_running);
    PowerPolicy power = {0};
    assert(!power_allowed(&power, false, false, 0.4f, 0));
    assert(!power_allowed(&power, false, false, 0.9f, 1));
    assert(!power_allowed(&power, false, false, 0.9f, 250));
    assert(power_allowed(&power, false, false, 0.9f, 251));
    assert(!power_allowed(&power, true, true, 1.8f, 252));
    assert(!power_allowed(&power, false, false, 0.6f, 253));
    assert(power_allowed(&power, true, false, 0.4f, 254));
    assert(!power_allowed(&power, false, false, NAN, 255));
}
static void test_cv_calibration(void) {
    Settings s;
    settings_default(&s);
    bool zero[2] = {0};
    int16_t samples[64];
    for (int i = 0; i < 64; i++)
        samples[i] = 10;
    assert(!cv_calibration(&s, 0, samples, true, zero));
    assert(cv_calibration(&s, 0, samples, false, zero));
    assert(zero[0]);
    for (int i = 0; i < 64; i++)
        samples[i] = 1465;
    assert(cv_calibration(&s, 0, samples, true, zero));
    assert(!zero[0]);
    assert(fabsf(cv_pitch(&s, 0, 1465, 48) - 60) < 0.001f);
    assert(fabsf(cv_pitch(&s, 0, -1445, 48) - 36) < 0.001f);
    for (int i = 0; i < 64; i++)
        samples[i] = (i & 1) ? 20 : -20;
    assert(!cv_calibration(&s, 1, samples, false, zero));
    for (int i = 0; i < 64; i++)
        samples[i] = 600;
    assert(!cv_calibration(&s, 1, samples, false, zero));
    assert(isnan(cv_pitch(&s, 2, 0, 48)));
    assert(settings_valid(&s));
}
static Ui test_ui;
static unsigned save_calls, cal_calls;
static bool ui_save(void *ctx) {
    (void)ctx;
    save_calls++;
    return settings_valid(&settings);
}
static bool ui_cal(void *ctx, unsigned input, bool one) {
    (void)ctx;
    (void)one;
    assert(input < 2);
    cal_calls++;
    return true;
}
static void press(Ui *u, int b, uint32_t *now, bool hold) {
    bool down[3] = {0};
    down[b] = true;
    ui_buttons(u, down, *now);
    ui_buttons(u, down, *now + 21);
    ui_buttons(u, down, *now + (hold ? 750 : 60));
    down[b] = false;
    ui_buttons(u, down, *now + (hold ? 780 : 80));
    ui_buttons(u, down, *now + (hold ? 801 : 101));
    *now += hold ? 900 : 120;
}
static void test_ui_controls(void) {
    setup();
    Ui *u = &test_ui;
    ui_init(u, &settings, &e, ui_save, ui_cal, NULL);
    uint32_t now = 100;
    msg(0, 0x90, 60, 100);
    press(u, 1, &now, false);
    assert(u->menu);
    press(u, 1, &now, false);
    assert(u->edit);
    press(u, 2, &now, false);
    assert(e.preset.mix[0] == 75);
    assert(engine_active(&e) == 1); /* live mixer */
    press(u, 1, &now, true);
    assert(!u->menu && !u->edit);
    press(u, 0, &now, true);
    assert(!engine_active(&e));
    u->menu = true;
    u->group = 5;
    u->field = 1;
    press(u, 1, &now, false);
    assert(save_calls == 1 && settings.preset[0].mix[0] == 75);
    e.preset.mix[0] = 30;
    u->field = 2;
    press(u, 1, &now, false);
    assert(e.preset.mix[0] == 75);
    u->group = 7;
    u->field = 3;
    press(u, 1, &now, false);
    assert(cal_calls == 1);
    UiStatus status = {.power = true, .usb = true, .adc_ok = true};
    for (int g = 0; g < 10; g++)
        for (int f = 0; f < 8; f++) {
            u->group = (uint8_t)g;
            u->field = (uint8_t)(f % (g == 9                                                     ? 8
                                      : g == 0 || g == 2 || g == 3 || g == 4 || g == 5 || g == 7 ? 4
                                      : g == 1 || g == 6                                         ? 3
                                                         : 5));
            u->edit = false;
            ui_render(u, &status, now);
        }
    /* Exercise real navigation, debounce, wrapping and editing; every save must stay valid. */
    for (int n = 0; n < 5000; n++) {
        press(u, (int)(random_u32() % 3), &now, (random_u32() % 30) == 0);
        ui_render(u, &status, now);
    }
    settings.preset[settings.selected] = e.preset;
    assert(settings_valid(&settings));
}
static void test_sn_clock(void) {
    SnClockConfig c = {SN_CLOCK_OFF, 0, 24, 3, 0, 1};
    SnClock s;
    sn_clock_init(&s, &c);
    assert(sn_clock_sample(&s) == 16 && sn_clock_hz(16) == 4000000);
    for (unsigned drop = 0; drop <= 48 * 256; drop++) {
        uint16_t half = sn_clock_half((uint16_t)drop);
        assert(half >= 16 && half <= 256);
        if (drop)
            assert(half >= sn_clock_half((uint16_t)(drop - 1)));
        float actual = (float)sn_clock_hz(half);
        float desired = 4000000 * exp2f(-(float)drop / (256 * 12));
        assert(fabsf(actual / desired - 1) < 0.0315f);
    }
    assert(sn_clock_half(12 * 256) == 32 && sn_clock_half(48 * 256) == 256);
    assert(sn_clock_hz(256) == 250000 && sn_clock_hz(0) == 0);
    c.mode = SN_CLOCK_MANUAL;
    c.base = 12;
    sn_clock_configure(&s, &c);
    assert(sn_clock_sample(&s) == 32);
    c.mode = SN_CLOCK_SWEEP;
    c.base = 0;
    sn_clock_configure(&s, &c);
    assert(sn_clock_sample(&s) == 16); /* idle sweep has no modulation */
    sn_clock_trigger(&s);
    assert(sn_clock_sample(&s) == 64);
    uint16_t before = 64;
    for (int tick = 0; tick < 2001; tick++) {
        uint16_t half = sn_clock_tick(&s);
        assert(half <= before);
        before = half;
    }
    assert(before == 16 && s.sweep == 0);
    c.mode = SN_CLOCK_DIVE;
    sn_clock_configure(&s, &c);
    sn_clock_trigger(&s);
    assert(sn_clock_sample(&s) == 16);
    for (int tick = 0; tick < 2001; tick++)
        sn_clock_tick(&s);
    assert(sn_clock_sample(&s) == 64);
    for (unsigned mode = SN_CLOCK_TRIANGLE; mode <= SN_CLOCK_SEQUENCE; mode++) {
        c.mode = (uint8_t)mode;
        c.rate = 15;
        c.depth = 48;
        sn_clock_configure(&s, &c);
        uint16_t min = 256, max = 16;
        for (int tick = 0; tick < 2000; tick++) {
            uint16_t half = sn_clock_tick(&s);
            if (half < min)
                min = half;
            if (half > max)
                max = half;
            assert(half >= 16 && half <= 256);
            assert(sn_clock_write_us(&c) >= half / 2u + 8u);
        }
        assert(max > min);
    }
    c = (SnClockConfig){SN_CLOCK_TRIANGLE, 0, 24, 3, 4, 1};
    sn_clock_configure(&s, &c);
    sn_clock_midi(&s, 1);
    assert(sn_clock_sample(&s) == 32);
    sn_clock_midi(&s, 2);
    assert(sn_clock_sample(&s) == 64);
    sn_clock_trigger(&s);
    assert(sn_clock_sample(&s) == 16);
    sn_clock_midi(&s, 3);
    assert(sn_clock_sample(&s) == 32);
    uint32_t phase = s.phase;
    for (int n = 0; n < 100; n++)
        sn_clock_tick(&s);
    assert(s.phase == phase); /* no free-running drift with MIDI sync or Stop */
    sn_clock_midi(&s, 0);
    assert(s.phase == 0); /* Start counter reset */
    s.clocks = UINT32_MAX;
    sn_clock_midi(&s, 0);
    assert(s.sync_step == 1); /* counter wrap is one clock, not a transport reset */
    c.base = 49;
    assert(!sn_clock_valid(&c));
    c.base = 0;
    c.depth = 49;
    assert(!sn_clock_valid(&c));
    c.depth = 24;
    c.mode = 8;
    assert(!sn_clock_valid(&c));
    c.mode = 1;
    c.rate = 16;
    assert(!sn_clock_valid(&c));
    c.rate = 4;
    c.sync = 9;
    assert(!sn_clock_valid(&c));
    c.sync = 0;
    c.retrigger = 2;
    assert(!sn_clock_valid(&c));
    setup();
    msg(1, 0xb0, 20, 5);
    assert(e.preset.sn_clock.mode == SN_CLOCK_OFF);
    msg(0, 0xb0, 20, 127);
    assert(e.preset.sn_clock.mode == SN_CLOCK_DIVE);
    msg(0, 0xb0, 21, 127);
    assert(e.preset.sn_clock.base == 48);
    msg(0, 0xb0, 22, 0);
    assert(e.preset.sn_clock.depth == 0);
    msg(0, 0xb0, 23, 127);
    assert(e.preset.sn_clock.rate == 15);
    msg(0, 0xb0, 24, 127);
    assert(e.preset.sn_clock.sync == 8);
    msg(0, 0xb0, 25, 0);
    assert(!e.preset.sn_clock.retrigger);
    msg(0, 0xb0, 26, 127);
    assert(e.sn_manual_trigger == 1);
    msg(0, 0x90, 69, 100);
    assert(e.sn_note_trigger == 0); /* SAA doesn't trigger SN clock */
    e.preset.route[0].target = TARGET_SN1;
    msg(0, 0x90, 69, 100);
    assert(e.sn_note_trigger == 1);
    engine_tick(&e, 10);
    assert(e.sn_div[0][1] == 284); /* clock FX never compensates away the clock-induced pitch */
    setup();
    e.preset.route[0].target = TARGET_SN1;
    e.preset.sn_clock = (SnClockConfig){SN_CLOCK_MANUAL, 48, 24, 4, 0, 1};
    msg(0, 0x90, 69, 100);
    engine_tick(&e, 1);
    assert(e.sn_div[0][1] == 284); /* fresh attacks don't wait for the slow-bus refresh */
    msg(0, 0xe0, 127, 127);
    engine_tick(&e, 1);
    assert(e.sn_div[0][1] == 284);
    engine_tick(&e, 2);
    assert(e.sn_div[0][1] < 284);
    Settings copy = settings;
    copy.preset[0].sn_clock.mode = 8;
    assert(!settings_valid(&copy));
}
static void test_v1_migration(void) {
    /* Construct the persisted bytes, independent of the current C struct layout. */
    uint8_t raw[236] = {0};
    setup();
    settings.selected = 3;
    settings.cv_zero[0] = 15;
    uint32_t n = STORE_COMMIT;
    memcpy(raw, &n, 4);
    n = 1;
    memcpy(raw + 4, &n, 4);
    memcpy(raw + 20, &n, 4);
    n = 99;
    memcpy(raw + 8, &n, 4);
    n = 216;
    memcpy(raw + 12, &n, 4);
    for (int i = 0; i < PRESETS; i++) {
        memcpy(raw + 24 + i * 24, &settings.preset[i], 24);
    }
    memcpy(raw + 216, settings.cv_zero, 8);
    memcpy(raw + 224, settings.cv_codes_per_volt, 8);
    raw[232] = settings.selected;
    n = store_crc(raw + 4, sizeof(raw) - 4);
    memcpy(raw + 16, &n, 4);
    Settings migrated;
    uint32_t seq;
    assert(store_load(raw, &migrated, &seq) && seq == 99 && migrated.selected == 3);
    assert(migrated.cv_zero[0] == 15 && settings_valid(&migrated));
    for (int i = 0; i < PRESETS; i++) {
        assert(!memcmp(&migrated.preset[i], &settings.preset[i], 24));
        assert(migrated.preset[i].sn_clock.mode == SN_CLOCK_OFF);
    }
    StoreRecord current;
    store_record(&current, &migrated, 100);
    assert(store_load(&current, &migrated, &seq) && seq == 100);
    assert(store_select((const StoreRecord *)raw, &current) == 1);
    for (unsigned i = 0; i < sizeof(raw); i++) {
        raw[i] ^= 1;
        assert(!store_load(raw, &migrated, &seq));
        raw[i] ^= 1;
    }
    setup();
    Ui u;
    uint32_t now = 0;
    ui_init(&u, &settings, &e, NULL, NULL, NULL);
    u.group = 9;
    u.menu = true;
    press(&u, 1, &now, false);
    press(&u, 2, &now, false);
    assert(e.preset.sn_clock.mode == SN_CLOCK_MANUAL);
    press(&u, 1, &now, false);
    u.field = 7;
    press(&u, 1, &now, false);
    assert(e.sn_manual_trigger == 1);
}
int main(void) {
    test_sn_clock();
    test_v1_migration();
    test_pitch();
    test_midi_engine();
    test_resources();
    test_parser();
    test_store();
    test_random();
    test_boundaries();
    test_ui_controls();
    test_cv_calibration();
    puts("PASS: pitch, MIDI ownership, sustain, steals, shared resources, CV, parser, flash "
         "journal, SN clock FX, v1 migration, MIDI clock, power, UI; 30000 MIDI events / 5000 "
         "button gestures");
    return 0;
}
