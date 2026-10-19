#include "editor.h"
#include "studio.h"
#include <stdio.h>
int main(void) {
    puts("[");
    for (unsigned f = 0; f < ST_FACTORIES; f++) {
        StSound s;
        char name[9];
        studio_factory(f, &s, name);
        printf("{\"name\":\"%.8s\",\"values\":[", name);
        for (unsigned i = 0; i < editor_field_count(); i++)
            printf("%s%d", i ? "," : "", editor_field_get(&s, i));
        printf("]}%s\n", f + 1 < ST_FACTORIES ? "," : "");
    }
    puts("]");
    return 0;
}
