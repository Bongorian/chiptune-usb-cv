#include "studio.h"
#include "ui.h"
#include "store.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Settings settings, loaded;
static Engine e;
static Ui ui;
static StoreRecord record;
static unsigned calls, save_calls;
static uint8_t registers[32];
static void ws(void *ctx, uint8_t r, uint8_t value) {
    (void)ctx;
    registers[r] = value;
    calls++;
}
static void wn(void *ctx, uint8_t c, uint8_t value) {
    (void)ctx;
    (void)c;
    (void)value;
    calls++;
}
static void setup(void) {
    settings_default(&settings);
    memset(registers, 0, sizeof(registers));
    engine_init(&e, &settings.preset[0], (SoundSink){ws, wn, NULL});
    studio_activate(&e);
    e.library = &settings;
    assert(studio_valid(&e.preset.studio));
    calls = 0;
}
static void msg(unsigned src, unsigned status, unsigned a, unsigned b) {
    uint8_t m[] = {(uint8_t)status, (uint8_t)a, (uint8_t)b};
    engine_message(&e, (uint8_t)src, m, 3);
}
static void tick(unsigned count) {
    while (count--)
        engine_tick(&e, 1);
}
static int find(unsigned p, unsigned src, unsigned note) {
    for (unsigned v = 0; v < 14; v++)
        if (e.voice[v].active && e.voice[v].part == p && e.voice[v].source == src &&
            e.voice[v].note == note)
            return (int)v;
    return -1;
}
static void only(unsigned model, unsigned poly) {
    for (unsigned p = 0; p < ST_PARTS; p++)
        e.preset.studio.part[p].enabled = 0;
    StPart *a = &e.preset.studio.part[0];
    studio_sound_default(&a->sound, model);
    a->enabled = 1;
    a->poly = (uint8_t)poly;
    studio_reset(&e);
}
static void test_plan(void) {
    setup();
    assert(e.studio.resources[0] == 0x36 && e.studio.outputs[0] == 0x24);
    assert(e.studio.resources[1] == 9);
    assert(e.studio.outputs[2] == 0x6c0);
    assert(e.studio.resources[3] == 0x3900);
    uint16_t used = 0;
    for (unsigned p = 0; p < ST_PARTS; p++) {
        assert(!(used & e.studio.resources[p]));
        used |= e.studio.resources[p];
    }
    assert(used == 0x3fff);
    static StScene scene;
    scene = e.preset.studio;
    scene.part[2].poly = 3;
    unsigned conflict;
    assert(!studio_plan(&scene, NULL, NULL, &conflict));
    msg(0, 0x90, 48, 100);
    int v = find(0, 0, 48);
    assert(v == 2);
    unsigned before = calls;
    assert(!studio_apply(&e, &scene));
    assert(e.voice[v].active && calls == before && e.preset.studio.part[2].poly == 2);
    for (unsigned m = 0; m < ST_MODELS; m++) {
        setup();
        only(m, studio_limit(m));
        assert(studio_valid(&e.preset.studio));
        for (unsigned n = 0; n < studio_limit(m); n++)
            msg(0, 0x90, 48 + n * 2, 100);
        tick(10);
        assert(studio_note_count(&e, 0) == studio_limit(m));
        assert(calls > 0);
        engine_panic(&e, -1);
        assert(!engine_active(&e));
    }
}
static float saa_hz(unsigned v) {
    unsigned o = (registers[0x10 + v / 2] >> ((v & 1) * 4)) & 7;
    return 8000000.0f * (float)(1u << o) / (512 * (511 - registers[8 + v]));
}
static void test_env(void) {
    setup();
    msg(0, 0x90, 48, 100);
    tick(10);
    assert(registers[0x18] == 0x8a);
    assert(!(registers[0x14] & (1u << 2)));
    assert(!registers[0x15]);
    assert(registers[2] && !registers[1]);
    float hz = saa_hz(1);
    assert(fabsf(hz - 130.8128f * 16) / (130.8128f * 16) < .005f);
    e.preset.studio.part[0].sound.steps = 8;
    engine_tick(&e, 1);
    assert(registers[0x18] == 0x9a);
    assert(fabsf(saa_hz(1) - hz / 2) / (hz / 2) < .005f);
    e.preset.studio.part[0].sound.shape = 3;
    engine_tick(&e, 1);
    assert(fabsf(saa_hz(1) - 130.8128f * 4) / (130.8128f * 4) < .005f);
    e.preset.studio.part[0].sound.invert = 1;
    engine_tick(&e, 1);
    assert(registers[0x18] & 1);
    setup();
    msg(0, 0x90, 100, 100);
    tick(5);
    assert(!registers[2] && e.pitch_clamps);
    setup();
    e.preset.studio.part[0].sound.model = ST_SAA_AM;
    studio_reset(&e);
    msg(0, 0x90, 48, 100);
    tick(5);
    assert(registers[0x14] & (1u << 2));
    assert(registers[0x18] & 0x80);
    assert(studio_env_clock(100, 5, 16) == 1600 && studio_env_clock(100, 3, 8) == 400);
}
static void test_polyphony_and_owners(void) {
    setup();
    msg(0, 0x90, 48, 100);
    msg(0, 0x90, 52, 100);
    msg(0, 0x91, 48, 100);
    msg(0, 0x91, 55, 100);
    msg(0, 0x92, 40, 100);
    msg(0, 0x92, 43, 100);
    msg(0, 0x93, 38, 100);
    tick(5);
    assert(engine_active_notes(&e) == 7 && engine_active(&e) == 10);
    assert(registers[0x14] == 9);
    int bass = find(2, 0, 40);
    assert(bass >= 6);
    msg(0, 0x90, 60, 100);
    assert(e.voice[bass].active && e.voice[bass].note == 40);
    assert(studio_note_count(&e, 0) == 2);
    setup();
    msg(0, 0x90, 48, 100);
    msg(1, 0x90, 48, 100);
    int usb = find(0, 0, 48), trs = find(0, 1, 48);
    assert(usb >= 0 && trs >= 0 && usb != trs);
    msg(0, 0x80, 48, 0);
    assert(e.voice[usb].releasing && e.voice[trs].held);
    engine_panic(&e, 0);
    assert(e.voice[trs].active && e.voice[trs].held);
    setup();
    msg(0, 0x92, 48, 100);
    tick(10);
    assert(e.sn_div[0][0] == e.sn_div[1][0]);
    e.preset.studio.part[2].sound.detune = 50;
    tick(5);
    assert(e.sn_div[0][0] != e.sn_div[1][0]);
}
static void test_cv_and_panic(void) {
    setup();
    studio_cv(&e, 0, 0, false, true);
    studio_cv(&e, 0, 0, true, true);
    int cv = find(0, 2, 48);
    assert(cv >= 0);
    msg(0, 0x90, 52, 100);
    msg(0, 0x90, 55, 100);
    assert(e.voice[cv].active && e.voice[cv].source == 2 && e.voice[cv].held);
    msg(0, 0x80, 52, 0);
    assert(e.voice[cv].held);
    studio_cv(&e, 0, 1, true, true);
    engine_tick(&e, 1);
    assert(e.voice[cv].pitch == 60);
    studio_cv(&e, 0, 1, false, true);
    assert(e.voice[cv].releasing);
    setup();
    e.preset.sn_clock.mode = SN_CLOCK_DIVE;
    studio_cv(&e, 1, 2.5f, false, true);
    engine_tick(&e, 1);
    assert(e.studio.clock_depth == 24);
    studio_cv(&e, 1, 0, false, false);
    engine_tick(&e, 1);
    assert(e.studio.clock_depth == 0);
    studio_cv(&e, 0, 0, true, true);
    assert(engine_active(&e));
    engine_panic(&e, -1);
    studio_cv(&e, 0, 0, true, true);
    tick(1);
    assert(!engine_active(&e));
    studio_cv(&e, 0, 0, false, true);
    studio_cv(&e, 0, 0, true, true);
    assert(engine_active(&e));
    setup();
    e.preset.studio.cv[1].mode = 0;
    e.preset.studio.cv[1].gate = 4;
    unsigned trigger = e.sn_manual_trigger;
    studio_cv(&e, 1, 0, true, false);
    assert(e.sn_manual_trigger == trigger + 1);
}
static void test_motion(void) {
    setup();
    studio_test(&e, 0, 48, 1);
    tick(10);
    assert(engine_active(&e));
    tick(1000);
    assert(!engine_active(&e) && !e.studio.test_on);
    studio_test(&e, 0, 48, 2);
    tick(10);
    assert(engine_active(&e));
    tick(1100);
    assert(engine_active(&e) && e.studio.test_on);
    engine_panic(&e, -1);
    assert(!e.studio.test_on);
    studio_run(&e, true);
    tick(1);
    assert(find(0, 5, 48) >= 0);
    tick(126);
    assert(e.studio.position[0] == 2);
    studio_run(&e, false);
    tick(1000);
    assert(!engine_active(&e));
    setup();
    e.preset.studio.part[0].arp = 1;
    msg(0, 0x90, 48, 100);
    msg(0, 0x90, 55, 100);
    msg(1, 0x90, 60, 100);
    tick(1);
    assert(find(0, 0, 48) >= 0);
    tick(126);
    assert(find(0, 0, 55) >= 0);
    msg(0, 0x80, 48, 0);
    msg(0, 0x80, 55, 0);
    msg(1, 0x80, 60, 0);
    tick(1000);
    assert(!engine_active(&e));
    setup();
    e.preset.studio.mod[0] = (StMod){.source = 5, .part = 0, .parameter = ST_PITCH, .amount = 50};
    msg(0, 0x90, 48, 100);
    tick(50);
    assert(isfinite(saa_hz(1)));
    assert(e.preset.studio.part[0].sound.transpose == 0);
    setup();
    e.preset.clock_policy = 3;
    e.preset.studio.gate_clock = 1;
    studio_run(&e, true);
    tick(10);
    uint32_t clocks = engine_transport_clocks(&e);
    studio_cv(&e, 1, 0, true, false);
    assert(engine_transport_clocks(&e) == clocks + 6);
}
static bool save(void *ctx) {
    (void)ctx;
    assert(settings_valid(&settings));
    store_record(&record, &settings, ++save_calls);
    assert(store_valid(&record));
    return true;
}
static bool cal(void *ctx, unsigned input, bool one) {
    (void)ctx;
    (void)one;
    return input < 2;
}
static void click(unsigned b) {
    ui_click(&ui, b, false, e.now);
}
static void back(void) {
    ui_click(&ui, 1, true, e.now);
}
static void open_page(unsigned page, unsigned field) {
    ui.depth = 0;
    ui_open(&ui, page);
    ui.field = (uint8_t)field;
    ui.edit = false;
    ui.confirm = false;
}
static void test_ui(void) {
    setup();
    ui_init(&ui, &settings, &e, save, cal, NULL);
    assert(!ui.depth);
    click(2);
    assert(ui.group == 1);
    click(1);
    assert(ui.page == UI_SOUND && ui.depth == 1);
    ui.field = 2;
    click(1);
    assert(ui.page == UI_OSC);
    ui.field = 1;
    click(1);
    assert(ui.edit);
    click(0);
    assert(e.preset.studio.part[0].sound.steps == 8);
    back();
    assert(!ui.edit && e.preset.studio.part[0].sound.steps == 16);
    back();
    assert(ui.page == UI_SOUND);
    back();
    assert(!ui.depth);
    ui.part = 2;
    open_page(UI_VOICE, 0);
    click(1);
    click(2);
    click(1);
    assert(ui.edit && e.preset.studio.part[2].poly == 2);
    back();
    ui.part = 0;
    open_page(UI_CV, 1);
    ui.cv_input = 0;
    click(1);
    click(2);
    click(1);
    assert(e.preset.studio.cv[0].part == 1);
    open_page(UI_SCENE, 1);
    click(1);
    assert(ui.confirm && !ui.confirm_yes);
    click(1);
    assert(!ui.confirm && !save_calls);
    click(1);
    click(2);
    click(1);
    assert(save_calls == 1 && !ui.dirty && settings.preset[0].studio.enabled);
    open_page(UI_SYSTEM, 5);
    click(1);
    assert(ui.confirm && !ui.boot_requested);
    click(2);
    click(1);
    assert(ui.boot_requested);
    ui.boot_requested = false;
    open_page(UI_PLAY, 5);
    click(1);
    click(2);
    click(1);
    tick(1);
    assert(e.studio.test_on);
    bool both[] = {true, false, true};
    ui_buttons(&ui, both, 1000);
    ui_buttons(&ui, both, 1950);
    assert(!engine_active(&e) && !e.studio.test_on);
    bool none[] = {false, false, false};
    ui_buttons(&ui, none, 1980);
    assert(ui.field == 5 && !ui.edit);
    UiStatus status = {.power = true, .usb = true, .adc_ok = true};
    for (unsigned page = 0; page < UI_PAGES; page++) {
        open_page(page, 0);
        unsigned count = ui_count(&ui);
        for (unsigned f = 0; f < (count ? count : 1); f++) {
            ui.field = (uint8_t)f;
            ui_render(&ui, &status, 3000);
        }
    }
}
static void test_store(void) {
    setup();
    settings.preset[0] = e.preset;
    settings.cv_zero[0] = 3.5f;
    settings.cv_codes_per_volt[1] = 1400;
    store_record(&record, &settings, 100);
    uint32_t seq;
    assert(sizeof(record) < STORE_BANK_BYTES);
    assert(store_load(&record, &loaded, &seq) && seq == 100 &&
           !memcmp(&settings, &loaded, sizeof(settings)));
    record.settings.preset[0].studio.part[0].sound.steps = 9;
    assert(!store_valid(&record));
    static uint8_t legacy[2140];
    memset(legacy, 0, sizeof(legacy));
    uint32_t n = STORE_COMMIT;
    memcpy(legacy, &n, 4);
    n = 4;
    memcpy(legacy + 4, &n, 4);
    memcpy(legacy + 20, &n, 4);
    n = 99;
    memcpy(legacy + 8, &n, 4);
    n = 2120;
    memcpy(legacy + 12, &n, 4);
    for (unsigned i = 0; i < PRESETS; i++)
        memcpy(legacy + 24 + i * 262, &settings.preset[i], 262);
    memcpy(legacy + 2120, settings.cv_zero, 8);
    memcpy(legacy + 2128, settings.cv_codes_per_volt, 8);
    legacy[2136] = 2;
    n = store_crc(legacy + 4, sizeof(legacy) - 4);
    memcpy(legacy + 16, &n, 4);
    assert(store_load(legacy, &loaded, &seq) && seq == 99 && loaded.selected == 2);
    assert(loaded.cv_zero[0] == 3.5f && loaded.cv_codes_per_volt[1] == 1400);
    printf("Settings %zu B, record %zu B, engine %zu B, UI %zu B\n", sizeof(Settings),
           sizeof(StoreRecord), sizeof(Engine), sizeof(Ui));
}
static void test_fuzz_and_mono(void) {
    setup();
    e.preset.studio.part[0].sound.mode = 2;
    msg(0, 0x90, 48, 100);
    tick(10);
    msg(0, 0x90, 52, 110);
    tick(1);
    assert(find(0, 0, 52) >= 0);
    msg(0, 0x80, 52, 0);
    tick(1);
    assert(find(0, 0, 48) >= 0);
    msg(0, 0x80, 48, 0);
    tick(1000);
    assert(!engine_active(&e));
    setup();
    e.preset.studio.part[0].arp = 1;
    msg(0, 0xb0, 64, 127);
    msg(0, 0x90, 48, 100);
    tick(1);
    msg(0, 0x80, 48, 0);
    tick(500);
    assert(engine_active(&e));
    msg(0, 0xb0, 64, 0);
    tick(1000);
    assert(!engine_active(&e));
    setup();
    ui_init(&ui, &settings, &e, save, cal, NULL);
    UiStatus status = {.power = true, .usb = true, .adc_ok = true};
    for (unsigned lane = 0; lane < 4; lane++) {
        open_page(UI_GRID, 0);
        ui.grid_lane = (uint8_t)lane;
        for (unsigned st = 0; st < 16; st++) {
            ui.grid_step = (uint8_t)st;
            ui_render(&ui, &status, e.now);
        }
    }
    uint32_t random = 0x23502040;
    for (unsigned n = 0; n < 12000; n++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        ui_click(&ui, random % 3, (random % 11) == 0, e.now);
        engine_tick(&e, 1);
        if (n % 17 == 0)
            ui_render(&ui, &status, e.now);
        assert(studio_valid(&e.preset.studio));
    }
}
static void test_colour_packing_and_buttons(void) {
    setup();
    only(ST_SAA_NOISE, 2);
    e.preset.studio.part[0].sound.noise_rate = 0;
    e.preset.studio.part[1].enabled = 1;
    e.preset.studio.part[1].poly = 1;
    studio_sound_default(&e.preset.studio.part[1].sound, ST_SAA_NOISE);
    e.preset.studio.part[1].sound.noise_rate = 1;
    e.preset.studio.part[2].enabled = 1;
    e.preset.studio.part[2].poly = 1;
    studio_sound_default(&e.preset.studio.part[2].sound, ST_SAA_ENV);
    assert(studio_valid(&e.preset.studio));
    setup();
    ui_init(&ui, &settings, &e, save, cal, NULL);
    bool down[3] = {false, true, false};
    ui_buttons(&ui, down, 100);
    ui_buttons(&ui, down, 110);
    assert(!ui.depth);
    ui_buttons(&ui, down, 121);
    assert(!ui.depth);
    down[1] = false;
    ui_buttons(&ui, down, 150);
    ui_buttons(&ui, down, 171);
    assert(ui.depth == 1);
    down[1] = true;
    ui_buttons(&ui, down, 200);
    ui_buttons(&ui, down, 221);
    ui_buttons(&ui, down, 950);
    assert(!ui.depth);
    down[1] = false;
    ui_buttons(&ui, down, 980);
    ui_buttons(&ui, down, 1001);
    assert(!ui.depth);
    open_page(UI_TEMPO, 0);
    click(1);
    unsigned tempo = e.preset.tempo;
    down[2] = true;
    ui_buttons(&ui, down, 1100);
    ui_buttons(&ui, down, 1121);
    ui_buttons(&ui, down, 2700);
    assert(e.preset.tempo > tempo);
    down[2] = false;
    ui_buttons(&ui, down, 2720);
    ui_buttons(&ui, down, 2741);
    back();
    assert(e.preset.tempo == tempo);
}
static void test_route_change_while_high(void) {
    setup();
    studio_cv(&e, 0, 0, false, true);
    studio_cv(&e, 0, 0, true, true);
    msg(0, 0x91, 55, 100);
    int midi = find(1, 0, 55);
    assert(midi >= 0);
    static StScene next;
    next = e.preset.studio;
    next.cv[0].part = 1;
    next.cv[0].gate_part = 1;
    assert(studio_apply(&e, &next));
    studio_cv(&e, 0, 0, true, true);
    tick(500);
    assert(find(0, 2, 48) < 0 && find(1, 2, 48) < 0);
    assert(e.voice[midi].held);
    studio_cv(&e, 0, 0, false, true);
    studio_cv(&e, 0, 0, true, true);
    assert(find(1, 2, 48) >= 0);
    setup();
    ui_init(&ui, &settings, &e, save, cal, NULL);
    e.preset.studio.part[0].sound.shape = 3;
    open_page(UI_SOUND, 1);
    click(1);
    click(1);
    assert(e.preset.studio.part[0].sound.shape == 3);
}
static void test_independent_gate_and_continue(void) {
    setup();
    e.preset.studio.cv[0].gate_part = 1;
    e.preset.studio.cv[0].gate_note = 36;
    studio_cv(&e, 0, 0, false, true);
    studio_cv(&e, 0, 0, true, true);
    assert(e.preset.studio.cv[0].base_note == 48 && find(1, 2, 36) >= 0);
    setup();
    e.preset.clock_policy = 3;
    e.preset.studio.gate_clock = 1;
    e.preset.studio.cv[0].gate = 3;
    e.preset.studio.cv[0].gate_part = 1;
    studio_cv(&e, 0, 0, true, false);
    assert(find(1, 5, 48) >= 0);
    setup();
    e.preset.clock_policy = 1;
    uint8_t start = 0xfa, clock = 0xf8, stop = 0xfc, cont = 0xfb;
    engine_message(&e, 0, &start, 1);
    tick(1);
    for (unsigned i = 0; i < 6; i++)
        engine_message(&e, 0, &clock, 1);
    tick(1);
    assert(e.studio.position[0] == 2);
    engine_message(&e, 0, &stop, 1);
    engine_message(&e, 0, &cont, 1);
    assert(e.studio.position[0] == 2);
    for (unsigned i = 0; i < 6; i++)
        engine_message(&e, 0, &clock, 1);
    tick(1);
    assert(e.studio.position[0] == 3);
}
static void test_audition_follows_part(void) {
    setup();
    ui_init(&ui, &settings, &e, save, cal, NULL);
    ui.test_mode = 2;
    studio_test(&e, 0, 48, 2);
    open_page(UI_PLAY, 0);
    click(1);
    click(2);
    click(1);
    assert(e.studio.test_part == 1 && find(1, 4, 48) >= 0);
    tick(300);
    assert(find(0, 4, 48) < 0);
}
int main(void) {
    test_plan();
    test_env();
    test_polyphony_and_owners();
    test_cv_and_panic();
    test_motion();
    test_ui();
    test_store();
    test_fuzz_and_mono();
    test_colour_packing_and_buttons();
    test_route_change_while_high();
    test_independent_gate_and_continue();
    test_audition_follows_part();
    puts("PASS: static reservations, all models, pitched ENV, stereo voices/ownership, CV, "
         "audition, sequencer/arp, UI commit/cancel/save/recovery and v4 migration");
}
