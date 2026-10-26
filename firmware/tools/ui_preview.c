#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static Settings s;
static Engine e;
static Ui u;
static void output(const char *dir, const char *name, unsigned now) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
    FILE *f = fopen(path, "wb");
    assert(f);
    UiStatus status = {.usb = true, .power = true, .adc_ok = true};
    ui_render(&u, &status, now);
    fprintf(f, "P4\n128 64\n");
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 128; x += 8) {
            unsigned b = 0;
            for (unsigned i = 0; i < 8; i++)
                b |= ((u.frame[(y / 8) * 128 + x + i] >> (y % 8)) & 1u) << (7 - i);
            fputc((int)b, f);
        }
    fclose(f);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    settings_default(&s);
    engine_init(&e, &s.project[0], (SoundSink){0});
    engine_bind_library(&e, &s);
    ui_init(&u, &s, &e, NULL, NULL, NULL);
    output(argv[1], "home", 2000);
    for (unsigned p = UI_MENU; p < UI_PAGES; p++) {
        if (p == UI_NAME || p == UI_CONFLICT)
            continue;
        ui_open(&u, p);
        char name[40];
        snprintf(name, sizeof(name), "page-%02u", p);
        output(argv[1], name, 2000);
    }
    ui_open(&u, UI_QUICK);
    u.field = 2; /* Volume */
    ui_click(&u, 1, false, 2100);
    output(argv[1], "edit", 2200);
    ui_click(&u, 1, true, 2300);
    Patch patch;
    char name[9];
    patch_factory(18, &patch, name);
    assert(!engine_load_patch(&e, 0, &patch, name));
    u.page = UI_CONFLICT;
    output(argv[1], "conflict", 2000);
    return 0;
}
