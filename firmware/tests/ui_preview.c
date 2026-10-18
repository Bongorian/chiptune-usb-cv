#include "ui.h"
#include <stdio.h>
static Engine engine;
static Settings settings;
static Ui ui;
static int output(const char *directory, const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.pgm", directory, name);
    FILE *file = fopen(path, "wb");
    if (!file)
        return 3;
    fprintf(file, "P5\n128 64\n255\n");
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 128; x++)
            fputc(ui.frame[(y / 8) * 128 + x] & (1u << (y % 8)) ? 255 : 0, file);
    fclose(file);
    return 0;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    settings_default(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, NULL, NULL, NULL);
    UiStatus status = {.power = true,
                       .usb = true,
                       .adc_ok = true,
                       .cv_raw = {1455, 2909},
                       .gates = {true, false},
                       .clock_hz = 2000000};
    const uint8_t notes[][3] = {{0x90, 48, 100}, {0x91, 55, 90}, {0x92, 36, 100}, {0x93, 38, 80}};
    for (unsigned i = 0; i < 4; i++)
        engine_message(&engine, 0, notes[i], 3);
    engine_tick(&engine, 10);
    ui_render(&ui, &status, 1000);
    if (output(argv[1], "home"))
        return 3;
    for (unsigned page = 0; page < UI_PAGES; page++) {
        ui.depth = 0;
        ui_open(&ui, page);
        ui.field = 0;
        ui.edit = false;
        if (page == UI_NAME) {
            ui.name_cursor = 3;
            snprintf(ui.name, sizeof(ui.name), "ENV LAB ");
        }
        if (page == UI_OSC) {
            ui.edit = true;
            ui.draft = 5;
        }
        if (page == UI_AMP) {
            ui.edit = true;
            ui.draft = 3;
        }
        if (page == UI_SN_CLOCK) {
            engine.preset.sn_clock.mode = SN_CLOCK_DIVE;
            ui.field = 1;
            ui.edit = true;
            ui.draft = SN_CLOCK_DIVE;
        }
        ui_render(&ui, &status, 1000);
        char name[32];
        snprintf(name, sizeof(name), "screen-%u", page);
        if (output(argv[1], name))
            return 3;
    }
    return 0;
}
