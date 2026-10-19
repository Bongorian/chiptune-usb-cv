#include "ui.h"
#include "studio.h"
#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Settings settings, before;
static Engine engine;
static Ui ui;
static StoreRecord record;
static unsigned saves;
static bool fail_save;
static bool save(void *ctx) {
    (void)ctx;
    saves++;
    if (fail_save)
        return false;
    store_record(&record, &settings, saves);
    assert(store_valid(&record));
    return true;
}
static void setup(void) {
    saves = 0;
    fail_save = false;
    settings_default(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    StSound sound;
    char name[9];
    studio_factory(0, &sound, name);
    assert(studio_single(&engine, 0, 0));
    assert(studio_sound_load(&engine, 0, &sound, name));
    settings.preset[0] = engine.preset;
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
}
static void click(unsigned button) {
    ui_click(&ui, button, false, engine.now);
}
static void back(void) {
    ui_click(&ui, 1, true, engine.now);
}
static void open(unsigned page, unsigned field) {
    ui.depth = 0;
    ui.confirm = ui.edit = false;
    ui_open(&ui, page);
    ui.field = (uint8_t)field;
}
static void yes(void) {
    assert(ui.confirm && !ui.confirm_yes);
    click(2);
    click(1);
    assert(!ui.confirm);
}
static void note(unsigned source, unsigned channel, bool on) {
    uint8_t message[] = {(uint8_t)((on ? 0x90 : 0x80) | channel), 48, 100};
    engine_message(&engine, (uint8_t)source, message, 3);
    engine_tick(&engine, 10);
}
static void test_boot_and_browser(void) {
    setup();
    assert(studio_is_single(&engine, 0));
    assert(!memcmp(engine.preset.studio.part[0].name, "ENV BASS", 8));
    assert(!engine.preset.studio.cv[0].mode && !engine.preset.studio.cv[1].gate);
    assert(!engine.preset.studio.part[0].sequence && !engine.preset.studio.part[0].arp);
    for (unsigned src = 0; src < 2; src++) {
        note(src, 1, true);
        assert(!studio_note_count(&engine, 0));
        note(src, 0, true);
        assert(studio_note_count(&engine, 0));
        note(src, 0, false);
        engine_tick(&engine, 100);
    }
    engine_panic(&engine, -1);
    UiStatus status = {.power = true, .usb = true};
    for (unsigned index = 0; index < ST_SOUNDS; index++) {
        assert(ui.active_sound == index && !ui.confirm);
        assert(
            !memcmp(&engine.preset.studio.part[0].sound, &settings.sounds[index], sizeof(StSound)));
        assert(studio_valid(&engine.preset.studio));
        note(0, 0, true);
        assert(studio_note_count(&engine, 0));
        ui_render(&ui, &status, engine.now);
        bool pixels = false;
        for (unsigned i = 0; i < 1024; i++)
            pixels |= ui.frame[i] != 0;
        assert(pixels);
        note(0, 0, false);
        click(2);
    }
    assert(ui.active_sound == 0 && !saves);
    click(1);
    assert(ui.page == UI_SOUND && ui.depth == 1);
    back();
    assert(!ui.depth);
    back();
    assert(ui.page == UI_MENU && ui.depth == 1);
    back();
    assert(!ui.depth);
}
static void test_dirty_and_save(void) {
    setup();
    open(UI_AMP, 4);
    click(1);
    click(2);
    click(1);
    unsigned level = engine.preset.studio.part[0].sound.level;
    ui.depth = 0;
    click(2);
    assert(ui.confirm && !ui.confirm_yes);
    click(1); /* NO must retain the edit and the playing patch. */
    assert(ui.active_sound == 0 && engine.preset.studio.part[0].sound.level == level);
    open(UI_SAVE_SOUND, 2);
    ui.library_slot = 7;
    click(1);
    yes();
    assert(saves == 1 && ui.active_sound == 7);
    assert(settings.sounds[7].level == level);
    assert(settings.preset[0].studio.part[0].sound.level == level);
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    assert(studio_is_single(&engine, 0) && ui.active_sound == 7);
    ui.depth = 0;
    click(2);
    assert(ui.active_sound == 8 && !ui.confirm);
    open(UI_SAVE_SOUND, 2);
    ui.library_slot = 7;
    before = settings;
    fail_save = true;
    click(1);
    yes();
    assert(!memcmp(&settings, &before, sizeof(settings)));
    assert(ui.active_sound == 8);
}
static void test_receive_and_multi(void) {
    setup();
    open(UI_MIDI, 0);
    click(1);
    click(2);
    assert(engine.preset.studio.route[0][0] == 1 && !engine.preset.studio.route[0][1]);
    back();
    assert(engine.preset.studio.route[0][0] == 1);
    click(1);
    click(2);
    click(1);
    for (unsigned src = 0; src < 2; src++)
        assert(!engine.preset.studio.route[src][0] && engine.preset.studio.route[src][1] == 1);
    note(0, 0, true);
    assert(!studio_note_count(&engine, 0));
    note(0, 1, true);
    assert(studio_note_count(&engine, 0));
    assert(engine.studio.midi_seen[0] && engine.studio.midi_channel[0] == 1);
    assert(!saves);
    /* Explicitly saved v0.5 multi setups keep all their routes and sounds. */
    studio_default(&settings.preset[0].studio);
    settings.preset[0].studio.enabled = 1;
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    assert(!studio_is_single(&engine, 0));
    assert(!memcmp(&engine.preset.studio, &settings.preset[0].studio, sizeof(StScene)));
    open(UI_SETUP, 1);
    click(1);
    click(1); /* NO */
    assert(!studio_is_single(&engine, 0));
    click(1);
    yes();
    assert(studio_is_single(&engine, 0));
}
static void test_factory_and_programs(void) {
    setup();
    for (unsigned slot = 0; slot < ST_SOUNDS; slot++) {
        uint8_t message[] = {0xc0, (uint8_t)slot};
        engine_message(&engine, 0, message, 2);
        assert(
            !memcmp(&engine.preset.studio.part[0].sound, &settings.sounds[slot], sizeof(StSound)));
        assert(studio_valid(&engine.preset.studio));
    }
    before = settings;
    open(UI_FACTORY, 1);
    ui.factory_slot = 7;
    click(1);
    yes();
    assert(engine.preset.studio.part[0].sound.model == ST_SN_TWIN);
    assert(!memcmp(engine.preset.studio.part[0].name, "TWINLEAD", 8));
    assert(!memcmp(&settings, &before, sizeof(settings)) && !saves);
}
int main(void) {
    test_boot_and_browser();
    test_dirty_and_save();
    test_receive_and_multi();
    test_factory_and_programs();
    puts("PASS: MIDI-first boot, all sounds, browser/edit/menu, dirty guard, save rollback/reboot, "
         "receive channel, multi preservation and factory/Program Change");
}
