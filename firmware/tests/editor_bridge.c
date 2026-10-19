/* Stream real protocol frames through the portable firmware for browser contract tests. */
#include "editor.h"
#include "studio.h"
#include <stdio.h>
#include <string.h>
static Settings settings;
static Engine engine;
static Editor editor;
int main(void) {
    settings_default(&settings);
    settings_reset_layout(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){0});
    studio_activate(&engine);
    char line[2048];
    while (fgets(line, sizeof(line), stdin)) {
        const char *p = line;
        unsigned byte;
        int used;
        while (sscanf(p, "%2x%n", &byte, &used) == 1) {
            editor_byte(&editor, (uint8_t)byte, 0);
            p += used;
        }
        editor_task(&editor, &engine, &settings, false, NULL, NULL, 1);
        for (unsigned i = 0; i < editor.length; i++)
            printf("%02x", editor.tx[i]);
        putchar('\n');
        uint8_t packet[4];
        while (editor_packet(&editor, packet)) {
        }
    }
    return 0;
}
