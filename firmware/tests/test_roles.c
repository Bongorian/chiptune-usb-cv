#include "engine.h"
#include "ui.h"
#include "store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Engine e;
static Settings settings;
static uint8_t saa_regs[32], latch[2];
static uint16_t sn_regs[2][8];
static void saa(void *ctx, uint8_t r, uint8_t v) {
    (void)ctx;
    saa_regs[r] = v;
}
static void sn(void *ctx, uint8_t chip, uint8_t v) {
    (void)ctx;
    if (v & 128) {
        latch[chip] = (v >> 4) & 7;
        sn_regs[chip][latch[chip]] = (uint16_t)((sn_regs[chip][latch[chip]] & 0x3f0) | (v & 15));
    } else
        sn_regs[chip][latch[chip]] =
            (uint16_t)((sn_regs[chip][latch[chip]] & 15) | ((v & 63) << 4));
}
static void setup(void) {
    settings_default(&settings);
    assert(settings_valid(&settings));
    engine_init(&e, &settings.preset[0], (SoundSink){saa, sn, NULL});
}
static void msg(unsigned source, uint8_t status, uint8_t a, uint8_t b) {
    uint8_t bytes[] = {status, a, b};
    engine_message(&e, (uint8_t)source, bytes, 3);
}
static void pairs(void) {
    for (unsigned i = 0; i < VOICES; i++)
        if (e.voice[i].active && e.voice[i].pair) {
            unsigned mate = e.voice[i].pair - 1;
            assert(mate < VOICES && e.voice[mate].active && e.voice[mate].pair == i + 1);
            assert(e.voice[i].source == e.voice[mate].source &&
                   e.voice[i].channel == e.voice[mate].channel);
            assert(e.voice[i].note == e.voice[mate].note &&
                   e.voice[i].token == e.voice[mate].token);
        }
}
static void no_cv(void) {
    e.preset.cv[0].enabled = e.preset.cv[1].enabled = 0;
}
static void route(uint8_t target, uint8_t patch) {
    e.preset.channel_map[0] = 0;
    e.preset.route[0].target = target;
    e.preset.route[0].program = patch;
    engine_routes_reset(&e, 0);
}
static void test_factory(void) {
    setup();
    for (unsigned i = 0; i < PRESETS; i++) {
        engine_preset(&e, &settings.preset[i]);
        assert(engine_cv_config_valid(&e.preset));
        for (unsigned s = 0; s < 2; s++)
            for (unsigned c = 0; c < 16; c++) {
                msg(s, (uint8_t)(0x90 | c), c == 9 ? 38 : 60, 100);
                engine_tick(&e, 10);
                pairs();
                msg(s, (uint8_t)(0x80 | c), c == 9 ? 38 : 60, 0);
                engine_tick(&e, 100);
                pairs();
            }
        engine_panic(&e, -1);
    }
    setup();
    msg(0, 0x90, 60, 100); /* Ch1=SAA */
    assert(engine_active(&e) == 1 && e.voice[1].active);
    msg(0, 0x91, 36, 100); /* Ch2=paired SN bass, CV pair is reserved */
    assert(e.voice[7].active && e.voice[10].active && !e.voice[6].active && !e.voice[9].active);
    engine_tick(&e, 10);
    pairs();
    assert(e.sn_div[0][1] == 956 && e.sn_div[1][1] == 956);
    assert(engine_active_notes(&e) == 2 && engine_active(&e) == 3);
    msg(0, 0x94, 60, 100);
    assert(engine_active(&e) == 3); /* unused channel OFF */
    msg(0, 0x99, 38, 100);
    assert(e.voice[12].active && e.voice[13].active);
    pairs();
    msg(0, 0x99, 42, 100);
    assert(e.voice[12].program == 8 && e.voice[13].program == 8);
    pairs();
    msg(0, 0xc1, 15, 0);
    msg(0, 0x91, 48, 100);
    assert(e.voice[8].program == 15 && e.voice[11].program == 15);
    assert(e.channel[1][1].program == 2); /* TRS program independent */
    uint8_t f = saa_regs[9];
    msg(0, 0xb0, 20, SN_CLOCK_RANDOM);
    msg(0, 0xb0, 22, 95);
    SnClockConfig c = engine_clock_config(&e.preset);
    SnClock clock;
    sn_clock_init(&clock, &c);
    assert(sn_clock_sample(&clock) > 32);
    engine_tick(&e, 10);
    assert(saa_regs[9] == f); /* SAA doesn't follow SN clock FX */
}
static void test_pitch_and_clock(void) {
    bool clamp;
    assert(sn_divider_clock(440, 2000000, &clamp) == 142 && !clamp);
    assert(sn_divider_clock(65.4064f, 2000000, &clamp) == 956 && !clamp);
    sn_divider_clock(41.2034f, 2000000, &clamp);
    assert(clamp);
    sn_divider_clock(41.2034f, 1000000, &clamp);
    assert(!clamp);
    setup();
    SnClockConfig c = engine_clock_config(&e.preset);
    SnClock clock;
    sn_clock_init(&clock, &c);
    assert(sn_clock_hz(sn_clock_sample(&clock)) == 2000000);
    e.preset.sn_reference = 2;
    c = engine_clock_config(&e.preset);
    sn_clock_configure(&clock, &c);
    assert(sn_clock_hz(sn_clock_sample(&clock)) == 1000000);
    e.preset.sn_clock.mode = SN_CLOCK_MANUAL;
    e.preset.sn_clock.base = 12;
    c = engine_clock_config(&e.preset);
    sn_clock_configure(&clock, &c);
    assert(sn_clock_hz(sn_clock_sample(&clock)) == 500000);
    no_cv();
    route(TARGET_SN_PAIR, 15);
    e.preset.sn_reference = 1;
    msg(0, 0x90, 69, 127);
    engine_tick(&e, 20);
    assert(e.sn_div[0][0] == 142 &&
           e.sn_div[1][0] == 142); /* FX doesn't compensate away modulation */
    e.preset.sn_width = 30;
    engine_tick(&e, 10);
    assert(e.sn_div[0][0] > e.sn_div[1][0]);
    msg(0, 0xb0, 10, 0);
    engine_tick(&e, 10);
    assert(e.sn_atten[1][0] == 15 && e.sn_atten[0][0] < 15);
    msg(0, 0xb0, 27, 2);
    assert(engine_active(&e) == 0 && e.preset.sn_reference == 2);
    msg(0, 0xb0, 28, 127);
    assert(e.preset.sn_width == 30);
    msg(0, 0xb0, 29, 4);
    assert(e.preset.sn_trigger_source == 4);
    msg(0, 0x90, 60, 100);
    assert(e.sn_note_trigger == 2); /* only previous note, melody now ignored */
    msg(0, 0x99, 36, 100);
    assert(e.sn_note_trigger == 4); /* stereo drum triggers */
}
static void test_arp_scope(void) {
    setup();
    no_cv();
    e.preset.arp_mode = 1;
    e.preset.arp_rate = 1;
    e.preset.clock_source = 0;
    e.preset.arp_target = 1;
    e.preset.part[0][0].program = e.preset.part[0][1].program = 15;
    engine_routes_reset(&e, 0);
    msg(0, 0x90, 60, 100);
    msg(0, 0x91, 48, 100);
    engine_tick(&e, 1);
    uint16_t divisor = e.sn_div[0][0];
    uint8_t frequency = saa_regs[8];
    uint8_t clock = 0xf8;
    engine_message(&e, 0, &clock, 1);
    engine_tick(&e, 4);
    assert(e.sn_div[0][0] == divisor && saa_regs[8] != frequency);
    e.preset.arp_target = 2;
    engine_tick(&e, 4);
    assert(e.sn_div[0][0] != divisor);
}
static void test_ownership_and_atomicity(void) {
    setup();
    no_cv();
    route(TARGET_SN_PAIR, 15);
    msg(0, 0x90, 60, 100);
    msg(0, 0x90, 60, 100);
    assert(engine_active(&e) == 4 && engine_active_notes(&e) == 2);
    pairs();
    msg(1, 0x80, 60, 0);
    assert(e.voice[6].held && e.voice[9].held);
    msg(0, 0xb0, 64, 127);
    msg(0, 0x80, 60, 0);
    assert(e.voice[6].sustained && e.voice[9].sustained);
    msg(0, 0xb0, 64, 0);
    assert(e.voice[6].releasing && e.voice[9].releasing);
    engine_tick(&e, 20);
    assert(engine_active(&e) == 2);
    pairs();
    engine_panic(&e, -1);
    assert(!engine_active(&e));
    setup();
    no_cv();
    route(TARGET_SN_PAIR, 15);
    msg(0, 0x90, 60, 100);
    msg(0, 0x90, 62, 100);
    msg(0, 0x90, 64, 100);
    route(TARGET_SN1, 15);
    msg(0, 0x90, 67, 100);
    assert(!e.voice[9].active && e.sn_atten[1][0] == 15);
    pairs();
    msg(0, 0x80, 60, 0);
    assert(e.voice[6].held && e.voice[6].note == 67);
    route(TARGET_SN_PAIR, 9);
    msg(0, 0x90, 69, 100);
    assert(e.voice[12].active && e.voice[13].active && !e.voice[8].active && !e.voice[11].active);
    pairs();
    /* Right-side-only lock: failure must not leave a left-only partial attack. */
    setup();
    e.preset.cv_pair[0] = e.preset.cv_pair[1] = 0;
    e.preset.cv[0] = (CvConfig){1, 6, 2, 48};
    e.preset.cv[1] = (CvConfig){1, 10, 2, 48};
    route(TARGET_SN2, 9);
    msg(0, 0x90, 69, 100);
    route(TARGET_SN_PAIR, 15);
    msg(0, 0x90, 60, 100);
    assert(engine_active(&e) == 1 && !e.voice[8].active && e.refused == 1);
    setup();
    no_cv();
    route(TARGET_FULL, 15);
    msg(0, 0x90, 60, 100);
    assert(engine_active(&e) == 3 && engine_active_notes(&e) == 1);
    pairs();
    for (unsigned i = 0; i < VOICES; i++)
        if (e.voice[i].active)
            assert(e.voice[i].hybrid);
    route(TARGET_SAA, 15);
    for (unsigned n = 0; n < 6; n++)
        msg(0, 0x90, (uint8_t)(70 + n), 100);
    assert(!e.voice[6].active &&
           !e.voice[9].active); /* stealing hybrid's SAA also retires SN pair */
    setup();
    route(TARGET_FULL, 3);
    msg(0, 0x90, 60, 100);
    assert(!engine_active(&e) &&
           e.refused == 1); /* unsupported HW envelope doesn't create partial layer */
}
static void test_cv(void) {
    setup();
    engine_cv(&e, 1, 36, true);
    engine_tick(&e, 10);
    pairs();
    assert(e.voice[6].source == 3 && e.voice[9].source == 3 && e.sn_div[0][0] == 956 &&
           e.sn_div[1][0] == 956);
    engine_cv_pitch(&e, 1, 48);
    engine_tick(&e, 10);
    assert(e.sn_div[0][0] == 478 && e.sn_div[1][0] == 478);
    msg(0, 0x91, 60, 100);
    assert(e.voice[6].source == 3 && e.voice[9].source == 3);
    pairs();
    engine_cv(&e, 1, 48, false);
    assert(e.voice[6].releasing && e.voice[9].releasing);
    engine_cv(&e, 1, 49, true);
    assert(e.voice[6].held && e.voice[9].held);
    pairs();
    e.preset.reserve_cv = 0;
    route(TARGET_SN2, 15);
    for (unsigned n = 0; n < 3; n++)
        msg(0, 0x90, (uint8_t)(70 + n), 100);
    assert(!e.voice[6].active); /* takeover on right also stops left CV */
    engine_cv_pitch(&e, 1, 60);
    assert(!e.voice[6].active);
    pairs();
    setup();
    engine_preset(&e, &settings.preset[5]);
    assert(engine_cv_mask(&e.preset, 1, true) ==
           ((1u << 12) | (1u << 13) | (1u << 8) | (1u << 11)));
    engine_cv(&e, 1, 48, true);
    assert(e.voice[12].active && e.voice[13].active);
    pairs();
    Preset p = e.preset;
    p.cv[0] = (CvConfig){1, 11, 2, 48};
    assert(!engine_cv_config_valid(&p));
}
static void gesture(Ui *u, unsigned b, uint32_t *now) {
    bool down[3] = {0};
    down[b] = true;
    ui_buttons(u, down, *now);
    *now += 25;
    ui_buttons(u, down, *now);
    down[b] = false;
    *now += 25;
    ui_buttons(u, down, *now);
    *now += 25;
    ui_buttons(u, down, *now);
}
static void test_ui_and_store(void) {
    setup();
    Ui u;
    uint32_t now = 0;
    ui_init(&u, &settings, &e, NULL, NULL, NULL);
    u.group = 9;
    u.menu = u.edit = true;
    u.field = 8;
    gesture(&u, 2, &now);
    assert(e.preset.sn_reference == 2);
    u.field = 9;
    gesture(&u, 2, &now);
    assert(e.preset.sn_width == 1);
    u.field = 10;
    gesture(&u, 2, &now);
    assert(e.preset.sn_trigger_source == 0);
    u.group = 1;
    u.field = 4;
    gesture(&u, 2, &now);
    assert(u.route_channel[0] == 1);
    u.field = 5;
    gesture(&u, 2, &now);
    assert(e.preset.part[0][1].target == TARGET_FULL);
    u.field = 6;
    gesture(&u, 2, &now);
    assert(e.preset.part[0][1].program == 5 && e.channel[0][1].program == 5);
    u.group = 4;
    u.field = 1;
    gesture(&u, 2, &now);
    assert(e.preset.cv[1].voice == 7);
    gesture(&u, 2, &now);
    gesture(&u, 2, &now);
    assert(e.preset.cv[1].voice == 12 && e.preset.cv[1].program == 8);
    settings.preset[0] = e.preset;
    assert(settings_valid(&settings));
    StoreRecord r;
    store_record(&r, &settings, 1);
    Settings copy;
    uint32_t sequence;
    assert(store_load(&r, &copy, &sequence) && !memcmp(&settings, &copy, sizeof(copy)));
    settings.preset[0].cv[0] = (CvConfig){1, 13, 8, 48};
    assert(!settings_valid(&settings));
    setup();
    /* v2 byte layout: preserve FX, 4MHz reference, old route and mono CV. */
    uint8_t raw[284] = {0};
    uint32_t n = STORE_COMMIT;
    memcpy(raw, &n, 4);
    n = 2;
    memcpy(raw + 4, &n, 4);
    memcpy(raw + 20, &n, 4);
    n = 100;
    memcpy(raw + 8, &n, 4);
    n = 264;
    memcpy(raw + 12, &n, 4);
    for (unsigned i = 0; i < PRESETS; i++)
        memcpy(raw + 24 + i * 30, &settings.preset[i], 30);
    memcpy(raw + 264, settings.cv_zero, 8);
    memcpy(raw + 272, settings.cv_codes_per_volt, 8);
    raw[280] = settings.selected;
    n = store_crc(raw + 4, sizeof(raw) - 4);
    memcpy(raw + 16, &n, 4);
    assert(store_load(raw, &copy, &sequence) && sequence == 100);
    for (unsigned i = 0; i < PRESETS; i++) {
        assert(!memcmp(&copy.preset[i], &settings.preset[i], 30));
        assert(copy.preset[i].sn_reference == 0 && !copy.preset[i].cv_pair[1] &&
               !copy.preset[i].channel_map[0]);
    }
    for (unsigned i = 0; i < sizeof(raw); i++) {
        raw[i] ^= 1;
        assert(!store_load(raw, &copy, &sequence));
        raw[i] ^= 1;
    }
}
static void test_role_stress(void) {
    setup();
    uint32_t rng = 0x10997648;
    for (unsigned n = 0; n < 15000; n++) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        unsigned source = rng & 1, channel = (rng >> 4) & 15;
        uint8_t note = (uint8_t)(24 + ((rng >> 12) % 72));
        unsigned kind = (rng >> 24) % 5;
        if (kind == 0)
            msg(source, (uint8_t)(0x90 | channel), note, 100);
        if (kind == 1)
            msg(source, (uint8_t)(0x80 | channel), note, 0);
        if (kind == 2)
            msg(source, (uint8_t)(0xb0 | channel), 64, rng & 2 ? 127 : 0);
        if (kind == 3)
            msg(source, (uint8_t)(0xc0 | channel), (uint8_t)((rng >> 16) % PATCHES), 0);
        if (kind == 4)
            msg(source, (uint8_t)(0xb0 | channel), 10, (uint8_t)((rng >> 17) & 127));
        if (n % 53 == 0)
            engine_cv(&e, source, (float)note, !!(rng & 2));
        if (n % 991 == 0)
            engine_preset(&e, &settings.preset[(rng >> 8) % PRESETS]);
        engine_tick(&e, 1);
        pairs();
        uint16_t reserved = engine_cv_mask(&e.preset, 0, true) | engine_cv_mask(&e.preset, 1, true);
        for (unsigned v = 0; v < VOICES; v++)
            if (e.voice[v].active && e.voice[v].source < SOURCES) {
                assert(!e.preset.reserve_cv || !(reserved & (1u << v)));
            }
    }
}
int main(void) {
    test_arp_scope();
    test_role_stress();
    test_factory();
    test_pitch_and_clock();
    test_ownership_and_atomicity();
    test_cv();
    test_ui_and_store();
    puts("PASS: factory roles, per-channel MIDI, stereo pairs/hybrid ownership, reference pitch, "
         "CV reservations, UI, v2 migration");
}
