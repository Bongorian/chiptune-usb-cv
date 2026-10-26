#include "ui.h"
#include "studio.h"
#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Settings settings, loaded;
static Engine engine;
static Ui ui;
static StoreRecord old_record, new_record;
static uint8_t saa[32], sn_latch[2], attenuation[2][4];
static void write_saa(void *ctx, uint8_t reg, uint8_t value) {
    (void)ctx;
    saa[reg] = value;
}
static void write_sn(void *ctx, uint8_t chip, uint8_t value) {
    (void)ctx;
    if (value & 0x80)
        sn_latch[chip] = (value >> 4) & 7;
    if (sn_latch[chip] & 1)
        attenuation[chip][sn_latch[chip] / 2] = value & 15;
}
static void setup(void) {
    settings_default(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){write_saa, write_sn, NULL});
    ui_init(&ui, &settings, &engine, NULL, NULL, NULL);
    assert(ui.layout_reset && settings.layout_revision == LAYOUT_REVISION);
}
static void message(unsigned src, unsigned kind, unsigned channel, unsigned note) {
    uint8_t bytes[] = {(uint8_t)(kind | channel), (uint8_t)note, 100};
    engine_message(&engine, (uint8_t)src, bytes, kind == 0xc0 ? 2 : 3);
    engine_tick(&engine, 1);
}
static void test_channels_and_resources(void) {
    setup();
    studio_profile(&engine.preset.studio, 0);
    studio_reset(&engine);
    const uint16_t outputs[] = {0x0009, 0x0024, 0x06c0, 0x3000};
    const uint16_t resources[] = {0x0009, 0x0036, 0x06c0, 0x3000};
    for (unsigned p = 0; p < 4; p++) {
        assert(engine.studio.outputs[p] == outputs[p]);
        assert(engine.studio.resources[p] == resources[p]);
    }
    for (unsigned src = 0; src < 2; src++)
        for (unsigned c = 0; c < 16; c++)
            assert(engine.preset.studio.route[src][c] == (c < 3 ? (1u << c) : c == 9 ? 8u : 0u));
    message(0, 0x90, 0, 48);
    message(0, 0x90, 0, 52);
    message(0, 0x90, 1, 48);
    message(0, 0x90, 1, 52);
    message(0, 0x90, 2, 48);
    message(0, 0x90, 2, 55);
    message(0, 0x90, 9, 38);
    for (unsigned p = 0; p < 4; p++)
        assert(studio_note_count(&engine, p) == (p == 3 ? 1 : 2));
    assert(engine_active(&engine) == 10 && engine_active_notes(&engine) == 7);
    assert(saa[0] && saa[3] && saa[2] && saa[5]);
    assert(!saa[1] && !saa[4]);
    for (unsigned c = 0; c < 2; c++)
        assert(attenuation[c][0] < 15 && attenuation[c][1] < 15 && attenuation[c][3] < 15);
    assert(!engine.voice[1].active && !engine.voice[4].active);  /* ENV clock only. */
    assert(!engine.voice[8].active && !engine.voice[11].active); /* SN tone3 spare. */
    message(0, 0xc0, 9, 11); /* METAL takes tone3, never the other channels' voices. */
    for (unsigned p = 0; p < 3; p++)
        assert(studio_note_count(&engine, p) == 2);
    assert(saa[0] && saa[3] && saa[2] && saa[5]);
    message(0, 0x80, 0, 48);
    for (unsigned t = 0; t < 120; t++)
        engine_tick(&engine, 1);
    assert(studio_note_count(&engine, 0) == 1);
    assert(studio_note_count(&engine, 1) == 2 && studio_note_count(&engine, 2) == 2);
}
static void test_focus_and_sound_roles(void) {
    setup();
    studio_profile(&engine.preset.studio, 0);
    studio_reset(&engine);
    message(0, 0x90, 0, 48);
    message(0, 0x90, 1, 52);
    ui_open(&ui, UI_MENU);
    ui_click(&ui, 1, false, engine.now);
    ui_click(&ui, 2, false, engine.now);
    ui_click(&ui, 1, false, engine.now);
    assert(ui.part == 1 && ui.library_slot == 1);
    assert(studio_note_count(&engine, 0) == 1 && studio_note_count(&engine, 1) == 1);
    assert(engine.preset.studio.route[0][0] == 1 && engine.preset.studio.route[0][1] == 2);
    for (unsigned p = 0; p < 4; p++) {
        ui.part = (uint8_t)p;
        ui.active_sound = UINT8_MAX;
        ui.depth = 0;
        ui.confirm = ui.edit = false;
        for (unsigned i = 0; i < ST_SOUNDS * 2; i++) {
            ui_render(&ui, &(UiStatus){.power = true, .usb = true}, engine.now);
            ui_click(&ui, 2, false, engine.now);
            assert(!ui.confirm);
            assert(studio_role_accepts(p + 1, engine.preset.studio.part[p].sound.model));
            assert(engine.preset.studio.part[p].poly == (p == 3 ? 1 : 2));
            assert(studio_valid(&engine.preset.studio));
        }
    }
    setup();
    studio_profile(&engine.preset.studio, 0);
    studio_reset(&engine);
    StSound original = engine.preset.studio.part[0].sound;
    unsigned refused = engine.refused;
    message(0, 0xc0, 0, 7); /* SN sound on fixed SAA channel must be ignored atomically. */
    assert(engine.refused == refused + 1);
    assert(!memcmp(&original, &engine.preset.studio.part[0].sound, sizeof(original)));
    message(0, 0xc0, 2, 8); /* Mono bass uses one of the two reserved stereo voices. */
    assert(engine.preset.studio.part[2].sound.mode == 1 && engine.preset.studio.part[2].poly == 2);
    message(0, 0xc0, 2, 7);
    assert(engine.preset.studio.part[2].sound.mode == 0 && engine.preset.studio.part[2].poly == 2);
}
static void test_destructive_reset_and_commit_epoch(void) {
    settings_default(&settings);
    settings.selected = 4;
    settings.cv_zero[0] = 12.5f;
    settings.cv_zero[1] = -4;
    settings.cv_codes_per_volt[0] = 1440;
    settings.cv_codes_per_volt[1] = 1470;
    settings.brightness = 55;
    settings.sounds[0].level = 1;
    memcpy(settings.sound_name[0], "OLD USER", 9);
    store_record(&old_record, &settings, 123);
    uint32_t sequence;
    assert(store_load(&old_record, &loaded, &sequence) && sequence == 123);
    assert(!loaded.layout_revision);
    engine_init(&engine, &loaded.preset[loaded.selected], (SoundSink){0});
    ui_init(&ui, &loaded, &engine, NULL, NULL, NULL);
    assert(ui.layout_reset && loaded.selected == 0 && loaded.layout_revision == LAYOUT_REVISION);
    assert(loaded.cv_zero[0] == 12.5f && loaded.cv_zero[1] == -4);
    assert(loaded.cv_codes_per_volt[0] == 1440 && loaded.cv_codes_per_volt[1] == 1470);
    assert(loaded.brightness == 55 && !memcmp(loaded.sound_name[0], "ENV BASS", 8));
    for (unsigned s = 0; s < PRESETS; s++)
        assert(loaded.preset[s].studio.part[0].role == ST_ROLE_SAA_TONE);
    store_record(&new_record, &loaded, sequence + 1);
    assert(store_select(&old_record, &new_record) == 1);
    assert(store_load(&new_record, &settings, &sequence));
    settings.sounds[0].level = 27;
    settings.preset[0].studio.part[0].sound.level = 42;
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, NULL, NULL, NULL);
    assert(!ui.layout_reset && settings.sounds[0].level == 27);
    assert(engine.preset.studio.part[0].sound.level == 42); /* No reset on later boots. */
    new_record.crc ^= 1;
    assert(store_select(&old_record, &new_record) == 0); /* Interrupted commit can retry safely. */
    assert(store_load(&old_record, &loaded, &sequence) && !loaded.layout_revision);
}
static void test_v1_to_v4_calibration(void) {
    const unsigned preset_sizes[] = {24, 30, 102, 262};
    const unsigned lengths[] = {216, 264, 840, 2120};
    static uint8_t raw[2140];
    for (unsigned version = 1; version <= 4; version++) {
        settings_default(&settings);
        settings.cv_zero[0] = 7.25f;
        settings.cv_codes_per_volt[1] = 1390;
        unsigned preset_size = preset_sizes[version - 1], length = lengths[version - 1];
        memset(raw, 0, sizeof(raw));
        uint32_t header[] = {STORE_COMMIT, version, 42, length, 0};
        memcpy(raw, header, sizeof(header));
        memcpy(raw + 20, &version, 4);
        for (unsigned p = 0; p < PRESETS; p++)
            memcpy(raw + 24 + p * preset_size, &settings.preset[p], preset_size);
        unsigned tail = 24 + PRESETS * preset_size;
        memcpy(raw + tail, settings.cv_zero, 8);
        memcpy(raw + tail + 8, settings.cv_codes_per_volt, 8);
        raw[tail + 16] = 3;
        uint32_t crc = store_crc(raw + 4, length + 16);
        memcpy(raw + 16, &crc, 4);
        uint32_t sequence;
        assert(store_load(raw, &loaded, &sequence) && sequence == 42);
        engine_init(&engine, &loaded.preset[loaded.selected], (SoundSink){0});
        ui_init(&ui, &loaded, &engine, NULL, NULL, NULL);
        assert(ui.layout_reset && loaded.layout_revision == LAYOUT_REVISION);
        assert(loaded.selected == 0 && loaded.cv_zero[0] == 7.25f);
        assert(loaded.cv_codes_per_volt[1] == 1390);
        assert(loaded.preset[0].studio.route[0][1] == 2);
    }
}
int main(void) {
    test_channels_and_resources();
    test_focus_and_sound_roles();
    test_destructive_reset_and_commit_epoch();
    test_v1_to_v4_calibration();
    puts("PASS: ch1/2/3/10 simultaneous voices, independent Note Off, editor focus, chip-role "
         "filtering, mono/poly reservation, destructive reset with calibration retained and boot "
         "epoch");
}
