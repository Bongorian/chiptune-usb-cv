#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
static Engine engine;
static Ui ui;
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    Settings settings;
    settings_default(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &engine, NULL, NULL, NULL);
    UiStatus status = {.power = true,
                       .usb = true,
                       .adc_ok = true,
                       .cv_raw = {1455, 2909},
                       .gates = {true, false},
                       .tick_us = 100,
                       .tick_overruns = 0};
    for (int group = 0; group < 9; group++) {
        ui.group = (uint8_t)group;
        ui.menu = true;
        ui.field = (uint8_t)(group == 5                                             ? 1
                             : group == 7                                           ? 1
                             : group == 8                                           ? 2
                             : group == 1 || group == 2 || group == 3 || group == 4 ? 2
                                                                                    : 0);
        ui.edit = false;
        ui_render(&ui, &status, 1000);
        char path[1024];
        snprintf(path, sizeof(path), "%s/screen-%d.pgm", argv[1], group);
        FILE *out = fopen(path, "wb");
        if (!out)
            return 3;
        fprintf(out, "P5\n128 64\n255\n");
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 128; x++)
                fputc(ui.frame[(y / 8) * 128 + x] & (1 << (y % 8)) ? 255 : 0, out);
        fclose(out);
    }
    return 0;
}
