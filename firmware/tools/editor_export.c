#include "editor.h"
#include <stdio.h>
static const char *names[] = {
#define FIELD(n, t, lo, hi) #n,
#include "sound_fields.inc"
#undef FIELD
};
int main(void) {
    printf("{\"schema\":%u,\"protocol\":%u,\"factories\":[\n", PATCH_SCHEMA, PROTOCOL_VERSION);
    for (unsigned f = 0; f < FACTORY_PATCHES; f++) {
        Patch p;
        char name[9];
        patch_factory(f, &p, name);
        printf("{\"name\":\"%.8s\",\"patch\":{", name);
        for (unsigned i = 0; i < editor_field_count(); i++)
            printf("%s\"%s\":%d", i ? "," : "", names[i], editor_field_get(&p, i));
        printf("}}%s\n", f + 1 < FACTORY_PATCHES ? "," : "");
    }
    puts("]}");
}
