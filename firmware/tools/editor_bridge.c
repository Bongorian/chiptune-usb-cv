#include "editor.h"
#include "store.h"
#include <stdio.h>
#include <string.h>
static Settings settings;
static Engine engine;
static Editor editor;
static StoreRecord stored;
static bool save(void *ctx) {
    (void)ctx;
    store_record(&stored, &settings, 1);
    return store_valid(&stored);
}
int main(void) {
    settings_default(&settings);
    engine_init(&engine, &settings.project[0], (SoundSink){0});
    engine.library = &settings;
    editor_init(&editor);
    char line[2048];
    while (fgets(line, sizeof(line), stdin)) {
        const char *p = line;
        unsigned b;
        int used;
        while (sscanf(p, "%2x%n", &b, &used) == 1) {
            editor_byte(&editor, (uint8_t)b, 0);
            p += used;
        }
        editor_task(&editor, &engine, &settings, false, save, NULL, 1);
        for (unsigned i = 0; i < editor.length; i++)
            printf("%02x", editor.tx[i]);
        putchar('\n');
        uint8_t packet[4];
        while (editor_packet(&editor, packet)) {
        }
        engine_tick(&engine, 1);
    }
    return 0;
}
