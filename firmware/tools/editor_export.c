#include "editor.h"
#include <stdio.h>
static const char *names[] = {
#define FIELD(n, t, lo, hi) #n,
#include "sound_fields.inc"
#undef FIELD
};
static void patch(const Patch *p) {
    printf("\"wave\":%u,\"features\":%u,\"patch\":{", (unsigned)patch_wave(p),
           (unsigned)patch_features(p));
    for (unsigned i = 0; i < editor_field_count(); i++)
        printf("%s\"%s\":%d", i ? "," : "", names[i], editor_field_get(p, i));
    putchar('}');
}
int main(void) {
    printf("{\"schema\":%u,\"protocol\":%u,\"factories\":[\n", PATCH_SCHEMA, PROTOCOL_VERSION);
    for (unsigned f = 0; f < FACTORY_PATCHES; f++) {
        Patch p;
        char name[9];
        patch_factory(f, &p, name);
        printf("{\"name\":\"%.8s\",", name);
        patch(&p);
        printf("}%s\n", f + 1 < FACTORY_PATCHES ? "," : "");
    }
    printf("],\"waveforms\":[\n");
    unsigned n = 0;
    for (unsigned chip = 0; chip < 2; chip++)
        for (unsigned wave = 0; wave < WAVE_COUNT; wave++) {
            if (!patch_wave_available(chip, wave))
                continue;
            Patch p;
            patch_default(&p, chip);
            if (!patch_set_wave(&p, wave))
                return 1;
            printf("%s{\"chip\":%u,", n++ ? ",\n" : "", chip);
            patch(&p);
            putchar('}');
        }
    puts("\n]}");
}
