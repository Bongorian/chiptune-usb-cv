#include "test_common.h"
#include "ui.h"
static Ui ui;
static unsigned saves;
static bool save(void *ctx) {
    (void)ctx;
    saves++;
    return true;
}
static void click(unsigned button) {
    ui_click(&ui, button, false, engine.now);
}
int main(void) {
    setup();
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    assert(!saves && ui.page == UI_HOME);
    message(0, 0x90, 45, 100);
    click(2);
    assert(ui.track == 1 && engine_active(&engine) == 1);
    click(0);
    click(1);
    assert(ui.page == UI_QUICK);
    ui.field = 2;
    click(1);
    assert(ui.edit);
    click(0);
    click(1);
    assert(!ui.edit && engine.project.track[0].level == 99);
    ui.field = 3;
    click(1);
    click(0);
    ui_click(&ui, 1, true, engine.now);
    assert(engine.project.track[0].pan == 50 && !ui.edit);
    ui_open(&ui, UI_SAVE);
    ui.save_kind = SAVE_PATCH;
    ui.slot = 4;
    ui.field = 2;
    click(1);
    assert(ui.confirm && !ui.confirm_yes);
    click(1);
    assert(!saves);
    click(1);
    click(2);
    click(1);
    assert(saves == 1 && settings.patches[4].chip == 0);
    assert(settings.project[0].track[0].level == 100);
    UiStatus status = {.power = true, .usb = true, .adc_ok = true};
    for (unsigned page = 0; page < UI_PAGES; page++) {
        ui.page = (uint8_t)page;
        ui.edit = ui.confirm = false;
        ui.field = 0;
        ui_render(&ui, &status, 100);
        bool any = false;
        for (unsigned i = 0; i < 1024; i++)
            any |= ui.frame[i] != 0;
        assert(any);
    }
    ui.page = UI_HOME;
    uint32_t random = 0x1945;
    for (unsigned i = 0; i < 5000; i++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        ui_click(&ui, random % 3, (random >> 8) % 7 == 0, i * 10);
        assert(project_valid(&engine.project));
        ui_render(&ui, &status, i * 10);
    }
    setup();
    isolated(20, PLAY_DIRECT, 1);
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    ui_open(&ui, UI_QUICK);
    ui.field = 10; /* WAVE follows CHIP in the direct sound menu. */
    click(1);
    click(2);
    click(1);
    assert(!ui.edit && patch_wave(&engine.project.track[0].patch) == WAVE_ENV);
    assert(ui_count(&ui) > 25);
    setup();
    isolated(22, PLAY_DIRECT, 1);
    ui_init(&ui, &settings, &engine, save, NULL, NULL);
    ui_open(&ui, UI_QUICK);
    unsigned noise_count = ui_count(&ui);
    ui.field = 10;
    click(1);
    click(0);
    click(1);
    assert(patch_wave(&engine.project.track[0].patch) == WAVE_PULSE);
    assert(ui_count(&ui) > noise_count); /* Pitched pulse enables actual pitch controls. */
    assert(engine.plan.resources[0][0] == 0x3900);
    bool down[3] = {true, false, true};
    ui_buttons(&ui, down, 100000);
    ui_buttons(&ui, down, 100901);
    assert(!engine_active(&engine));
    puts("PASS UI: track focus, direct controls, commit/cancel, patch-only save, all pages, 5,000 "
         "random actions and panic chord");
}
