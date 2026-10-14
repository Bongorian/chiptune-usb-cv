#include "engine.h"
#include "cv.h"
#include "store.h"
#include "ui.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Settings settings, copy;
static Engine e;
static Ui ui;
static uint8_t saa[32];
static void write_saa(void *ctx, uint8_t r, uint8_t v) {
    (void)ctx;
    saa[r] = v;
}
static void setup(void) {
    settings_default(&settings);
    assert(settings_valid(&settings));
    engine_init(&e, &settings.preset[0], (SoundSink){write_saa, NULL, NULL});
}
static void msg(uint8_t s, uint8_t status, uint8_t a, uint8_t b) {
    uint8_t m[] = {status, a, b};
    engine_message(&e, s, m, 3);
}
static int voice(int note) {
    for (int v = 0; v < VOICES; v++)
        if (e.voice[v].active && e.voice[v].source == 0 && e.voice[v].note == note)
            return v;
    return -1;
}
static void step(unsigned n) {
    while (n--)
        engine_tick(&e, 1);
}
static void test_quantizer_glide(void) {
    for (unsigned scale = 1; scale < CV_SCALES; scale++)
        for (unsigned root = 0; root < 12; root++) {
            float last = -1;
            for (int n = -30; n < 1400; n++) {
                float q = engine_quantize(n / 10.0f, scale, root);
                assert(isfinite(q) && q >= 0 && q <= 127 && q == roundf(q) && q >= last);
                assert(engine_quantize(q, scale, root) == q);
                last = q;
            }
        }
    assert(engine_quantize(61, 2, 0) == 60); /* C major: C#/Db tie goes to C */
    assert(engine_quantize(63, 2, 0) == 62);
    assert(engine_quantize(63, 3, 0) == 63);
    assert(engine_quantize(60.25f, 0, 0) == 60.25f);
    assert(isnan(engine_quantize(NAN, 0, 0)));
    setup();
    e.preset.cv_scale[1] = 1;
    engine_cv(&e, 1, 60.49f, true);
    assert(e.voice[6].pitch == 60 && e.voice[9].pitch == 60);
    engine_cv_pitch(&e, 1, 60.55f);
    assert(e.voice[6].pitch == 60); /* ADC chatter across the midpoint is held */
    engine_cv_pitch(&e, 1, 60.7f);
    assert(e.voice[6].pitch == 61);
    e.preset.cv_glide[1] = 4;
    engine_cv_pitch(&e, 1, 72);
    engine_tick(&e, 60);
    assert(e.voice[6].pitch > 61 && e.voice[6].pitch < 72);
    assert(e.voice[6].pitch == e.voice[9].pitch);
    msg(0, 0x81, 60, 0); /* MIDI must not release the CV pair */
    assert(e.voice[6].held);
    engine_cv(&e, 1, 72, false);
    float release_pitch = e.voice[6].pitch;
    engine_cv_pitch(&e, 1, 84);
    engine_tick(&e, 5);
    assert(e.voice[6].pitch == release_pitch);
    assert(fabsf(cv_volts(&settings, 0, 1455) - 1.0003125f) < 0.001f);
}
static void test_cv_modulation(void) {
    for (unsigned role = 1; role < CV_ROLES; role++) {
        setup();
        e.preset.cv_role[0] = (uint8_t)role;
        assert(engine_cv_config_valid(&e.preset));
        assert(engine_cv_mask(&e.preset, 0, true) == 0);
        Preset saved = e.preset;
        engine_cv_control(&e, 0, 2.5f, true);
        assert(e.cv_mod[0] == 0.5f && e.sn_manual_trigger == 1);
        engine_cv_control(&e, 0, 2.5f, true);
        assert(e.sn_manual_trigger == 1);
        engine_cv_control(&e, 0, 2.5f, false);
        engine_cv_control(&e, 0, 2.5f, true);
        assert(e.sn_manual_trigger == 2);
        assert(!memcmp(&saved, &e.preset, sizeof(saved)));
        if (role == CV_SN_BASE) {
            SnClockConfig c = engine_effective_clock(&e);
            assert(c.mode == SN_CLOCK_MANUAL && c.base == 36);
        }
        e.preset.cv_range[0] = 1;
        engine_cv_control(&e, 0, 2.5f, true);
        assert(e.cv_mod[0] == 0.25f);
        engine_cv_control(&e, 0, -1, true);
        assert(e.cv_mod[0] == 0);
        engine_cv_control(&e, 0, 50, true);
        assert(e.cv_mod[0] == 1);
        engine_cv_clear(&e, 0);
        assert(!e.cv_mod_valid[0]);
        assert(engine_effective_clock(&e).base == 12);
        e.preset.cv_role[1] = (uint8_t)role;
        assert(!engine_cv_config_valid(&e.preset));
    }
    setup();
    e.preset.cv_role[0] = CV_SAA_PAN;
    e.preset.part[0][0].program = 15;
    engine_routes_reset(&e, 0);
    msg(0, 0x90, 60, 127);
    int v = voice(60);
    assert(v >= 0);
    engine_cv_control(&e, 0, 0, false);
    engine_tick(&e, 10);
    assert((saa[v] & 15) > 0 && (saa[v] >> 4) == 0);
    engine_cv_control(&e, 0, 5, false);
    engine_tick(&e, 10);
    assert((saa[v] & 15) == 0 && (saa[v] >> 4) > 0);
    e.preset.cv_role[0] = CV_MASTER;
    engine_cv_control(&e, 0, 0, false);
    engine_tick(&e, 10);
    assert(saa[v] == 0);
    engine_cv_clear(&e, 0);
    engine_tick(&e, 10);
    assert(saa[v] > 0);
}
static void test_midi_performance(void) {
    setup();
    PartPerformance *p = &e.preset.performance[0][1];
    p->transpose = 12;
    p->low = 48;
    p->high = 72;
    msg(0, 0x91, 47, 127);
    assert(!engine_active(&e));
    msg(0, 0x91, 60, 80);
    int v = voice(60);
    assert(v >= 0);
    engine_tick(&e, 10);
    bool clamped;
    assert(e.sn_div[0][(v - 6) % 3] == sn_divider_clock(130.8128f, 2000000, &clamped));
    msg(0, 0x81, 60, 0);
    assert(e.voice[v].releasing); /* Note Off uses original note, not sounding note */
    engine_panic(&e, -1);
    p->velocity = 1;
    msg(0, 0x91, 60, 64);
    v = voice(60);
    assert(e.voice[v].velocity > 64);
    engine_panic(&e, -1);
    p->velocity = 2;
    msg(0, 0x91, 60, 64);
    v = voice(60);
    assert(e.voice[v].velocity < 64 && e.voice[v].velocity > 0);
    engine_panic(&e, -1);
    p->velocity = 3;
    msg(0, 0x91, 60, 1);
    v = voice(60);
    assert(e.voice[v].velocity == 110);
    msg(0, 0xb1, 6, 12);
    assert(e.channel[0][1].bend_range == 2); /* RPN initially null */
    msg(0, 0xb1, 101, 0);
    msg(0, 0xb1, 100, 0);
    msg(0, 0xb1, 6, 12);
    msg(0, 0xb1, 38, 50);
    assert(e.channel[0][1].bend_range == 12 && e.channel[0][1].bend_cents == 50);
    assert(e.channel[1][1].bend_range == 2);
    msg(0, 0xb1, 99, 1);
    msg(0, 0xb1, 6, 24);
    assert(e.channel[0][1].bend_range == 12); /* NRPN cannot accidentally change RPN 0 */
    msg(0, 0xb1, 101, 127);
    msg(0, 0xb1, 100, 127);
    msg(0, 0xb1, 6, 24);
    assert(e.channel[0][1].bend_range == 12);
    msg(0, 0xd1, 100, 0);
    assert(e.channel[0][1].pressure == 100);
    msg(0, 0xa1, 60, 110);
    assert(e.voice[v].pressure == 110 && e.voice[e.voice[v].pair - 1].pressure == 110);
    msg(0, 0xb1, 73, 127);
    msg(0, 0xb1, 75, 0);
    msg(0, 0xb1, 72, 127);
    msg(0, 0xb1, 70, 40);
    assert(e.channel[0][1].attack == 127 && e.channel[0][1].decay == 0);
    msg(0, 0xb1, 121, 0);
    assert(e.channel[0][1].attack == 64 && e.channel[0][1].pressure == 0 &&
           e.voice[v].pressure == 0);
    /* Filtered duplicate Note On must not consume the accepted note's Note Off token. */
    setup();
    msg(0, 0x90, 60, 100);
    v = voice(60);
    e.preset.performance[0][0].low = 61;
    msg(0, 0x90, 60, 100);
    msg(0, 0x80, 60, 0);
    assert(e.voice[v].releasing);
    msg(0, 0x80, 60, 0);
}
static void test_tone_and_timing(void) {
    setup();
    e.preset.part[0][0].program = 0;
    engine_routes_reset(&e, 0);
    e.preset.tone[0].attack = 127;
    msg(0, 0x90, 60, 127);
    int v = voice(60);
    engine_tick(&e, 3);
    assert(e.voice[v].level > 0 && e.voice[v].level < 0.5f);
    e.preset.tone[0].sustain = 100;
    e.preset.tone[0].tremolo = 100;
    e.preset.tone[0].rate = 3; /* 4Hz; at 62ms the sine reaches its peak: silent */
    step(59);
    assert(e.voice[v].level == 1 && saa[v] == 0);
    e.preset.tone[0].tremolo = 0;
    engine_tick(&e, 1);
    assert(saa[v] > 0);
    setup();
    e.preset.clock_policy = 2;
    step(1000);
    assert(engine_transport_clocks(&e) == 48);
    uint8_t stop = 0xfc;
    engine_message(&e, 0, &stop, 1);
    step(500);
    assert(engine_transport_clocks(&e) == 72); /* INTERNAL ignores transport */
    e.preset.clock_policy = 1;
    engine_transport_reset(&e);
    step(1000);
    assert(engine_transport_clocks(&e) == 0);
    uint8_t start = 0xfa, clock = 0xf8, cont = 0xfb;
    engine_message(&e, 0, &start, 1);
    for (int i = 0; i < 6; i++) {
        engine_message(&e, 0, &clock, 1);
        engine_tick(&e, 20);
    }
    assert(engine_transport_clocks(&e) == 6);
    engine_message(&e, 0, &stop, 1);
    step(1000);
    assert(engine_transport_clocks(&e) == 6);
    e.preset.clock_policy = 0;
    step(1000);
    assert(engine_transport_clocks(&e) == 6); /* AUTO honors an explicit Stop indefinitely */
    engine_message(&e, 0, &cont, 1);
    step(1000);
    assert(engine_transport_clocks(&e) > 6);
    e.preset.tempo = 210;
    e.preset.clock_policy = 2;
    engine_transport_reset(&e);
    step(1000);
    assert(engine_transport_clocks(&e) == 96);
    SnClock fx;
    SnClockConfig c = {SN_CLOCK_SEQUENCE, 0, 24, 0, 0, 0};
    sn_clock_init(&fx, &c);
    uint8_t pattern[8] = {0, 100, 0, 100, 0, 100, 0, 100};
    sn_clock_sequence(&fx, pattern);
    fx.phase = 0;
    assert(sn_clock_sample(&fx) == 16);
    fx.phase = 1u << 29;
    assert(sn_clock_sample(&fx) == 64);
    setup();
    msg(0, 0xb0, 30, 3);
    msg(0, 0xb0, 31, 127);
    assert(e.preset.clock_sequence[3] == 100);
    msg(1, 0xb0, 31, 0);
    assert(e.preset.clock_sequence[3] == 100);
}
static void test_v3_migration(void) {
    setup();
    uint8_t raw[860] = {0};
    uint32_t n = STORE_COMMIT;
    memcpy(raw, &n, 4);
    n = 3;
    memcpy(raw + 4, &n, 4);
    memcpy(raw + 20, &n, 4);
    n = 42;
    memcpy(raw + 8, &n, 4);
    n = 840;
    memcpy(raw + 12, &n, 4);
    for (unsigned i = 0; i < PRESETS; i++)
        memcpy(raw + 24 + i * 102, &settings.preset[i], 102);
    memcpy(raw + 840, settings.cv_zero, 8);
    memcpy(raw + 848, settings.cv_codes_per_volt, 8);
    raw[856] = 5;
    n = store_crc(raw + 4, sizeof(raw) - 4);
    memcpy(raw + 16, &n, 4);
    uint32_t seq;
    assert(store_load(raw, &copy, &seq) && seq == 42 && copy.selected == 5);
    for (unsigned i = 0; i < PRESETS; i++) {
        assert(!memcmp(&settings.preset[i], &copy.preset[i], 102));
        assert(copy.preset[i].tone[0].attack == 64 && copy.preset[i].tempo == 90);
        assert(copy.preset[i].performance[0][0].transpose == 24);
    }
    for (unsigned i = 0; i < sizeof(raw); i++) {
        raw[i] ^= 1;
        assert(!store_load(raw, &copy, &seq));
        raw[i] ^= 1;
    }
    StoreRecord record;
    store_record(&record, &copy, 43);
    assert(sizeof(record) < STORE_BANK_BYTES && store_load(&record, &settings, &seq) && seq == 43);
    /* Invalid appended values must be rejected even with a correct CRC. */
    settings.preset[0].cv_scale[0] = CV_SCALES;
    store_record(&record, &settings, 44);
    assert(!store_valid(&record));
}
static void press(unsigned button, uint32_t *now) {
    bool down[3] = {0};
    down[button] = true;
    ui_buttons(&ui, down, *now);
    *now += 25;
    ui_buttons(&ui, down, *now);
    down[button] = false;
    *now += 25;
    ui_buttons(&ui, down, *now);
    *now += 25;
    ui_buttons(&ui, down, *now);
}
static void test_ui(void) {
    setup();
    ui_init(&ui, &settings, &e, NULL, NULL, NULL);
    uint32_t now = 0;
    ui.menu = ui.edit = true;
    ui.group = 3;
    ui.field = 5;
    press(2, &now);
    assert(e.preset.cv_role[0] == CV_SN_BASE);
    ui.field = 6;
    press(2, &now);
    assert(e.preset.cv_scale[0] == 1);
    ui.group = 1;
    ui.field = 7;
    press(2, &now);
    assert(e.preset.performance[0][0].transpose == 25);
    ui.group = 10;
    ui.field = 3;
    press(0, &now);
    assert(e.preset.tone[0].sustain == 100);
    press(2, &now);
    assert(e.preset.tone[0].sustain == 127);
    e.clocks = 20;
    ui.group = 9;
    ui.field = 6;
    press(2, &now);
    assert(e.preset.clock_source == 1 && e.clocks == 0);
    ui.group = 11;
    ui.field = 0;
    press(2, &now);
    assert(ui.sequence_step == 1);
    ui.field = 1;
    press(2, &now);
    assert(e.preset.clock_sequence[1] == 51);
    /* All new menu entries are rendered and edited, including duplicate-role rejection. */
    static const unsigned counts[] = {4, 11, 12, 10, 10, 4, 6, 4, 5, 11, 7, 2};
    UiStatus status = {.usb = true, .adc_ok = true};
    for (unsigned group = 0; group < UI_GROUPS; group++)
        for (unsigned f = 0; f < counts[group]; f++) {
            ui.group = (uint8_t)group;
            ui.field = (uint8_t)f;
            ui.menu = ui.edit = true;
            if (group != 5 && group != 7 && group != 8) {
                press(2, &now);
                press(0, &now);
            }
            ui_render(&ui, &status, now);
            settings.preset[settings.selected] = e.preset;
            assert(settings_valid(&settings));
        }
}
int main(void) {
    test_quantizer_glide();
    test_cv_modulation();
    test_midi_performance();
    test_tone_and_timing();
    test_v3_migration();
    test_ui();
    puts(
        "PASS: CV scales/hysteresis/paired glide, voltage modulation/fallback, MIDI "
        "zones/transpose/"
        "velocity/RPN/pressure, envelope/tremolo, internal/external transport, custom clock steps, "
        "v3 migration/CRC corruption, all 12 UI groups");
}
