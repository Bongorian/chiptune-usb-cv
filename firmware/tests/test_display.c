#include "oled.h"
#include "service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Oled oled;
    uint8_t frame[1024], ram[1024];
    unsigned writes, fail_at, renders, data_writes, page, col, contrast, on_count;
    bool on, pump;
} Panel;
static bool write_panel(void *ctx, const uint8_t *bytes, size_t length) {
    Panel *p = ctx;
    assert(length && length <= 26);
    if (++p->writes == p->fail_at)
        return false;
    if (bytes[0] == 0x40) {
        assert(length == 17 && p->page < 8 && p->col <= 112);
        memcpy(p->ram + p->page * 128 + p->col, bytes + 1, 16);
        p->col += 16;
        p->data_writes++;
        return true;
    }
    assert(bytes[0] == 0);
    for (size_t i = 1; i < length; i++) {
        unsigned b = bytes[i];
        if (b == 0xae)
            p->on = false;
        else if (b == 0xaf) {
            assert(p->pump && p->data_writes);
            p->on = true;
            p->on_count++;
        } else if (b == 0x8d) {
            assert(i + 1 < length);
            p->pump = bytes[++i] == 0x14;
        } else if (b == 0x81) {
            assert(i + 1 < length);
            p->contrast = bytes[++i];
        } else if (b == 0xd5 || b == 0xa8 || b == 0xd3 || b == 0x20 || b == 0xda || b == 0xd9 ||
                   b == 0xdb) {
            assert(++i < length);
        } else if (b >= 0xb0 && b <= 0xb7)
            p->page = b - 0xb0;
        else if (b < 0x10)
            p->col = (p->col & 0xf0) | b;
        else if (b >= 0x10 && b < 0x20)
            p->col = (p->col & 15) | ((b & 15) << 4);
    }
    return true;
}
static void render(void *ctx, uint32_t now) {
    Panel *p = ctx;
    (void)now;
    p->renders++;
    for (unsigned i = 0; i < sizeof(p->frame); i++)
        p->frame[i] = (uint8_t)(i + p->renders);
}
static void setup(Panel *p) {
    memset(p, 0, sizeof(*p));
    oled_init(&p->oled, p->frame, write_panel, render, p);
}
static void test_busy_main_loop(void) {
    Panel p;
    setup(&p);
    uint32_t serviced = 0;
    unsigned old_admissions = 0, forced = 0;
    /* No loop ever reaches the old <250us window. The real driver must still
       initialize, settle its charge pump, turn on and transfer every pixel. */
    for (unsigned ms = 0; ms < 1000; ms++) {
        uint32_t now = ms * 1000 + 700, tick = ms * 1000;
        if (now - tick < 250)
            old_admissions++;
        if (!service_ready(now, tick, serviced, 250, 4000))
            continue;
        assert(now - serviced < 5000);
        serviced = now;
        forced++;
        oled_task(&p.oled, ms, true, 70);
        if (p.data_writes == 64)
            break;
    }
    assert(!old_admissions && forced && p.on && p.pump);
    assert(p.contrast == 178 && p.renders == 1 && p.data_writes == 64);
    assert(memcmp(p.ram, p.frame, sizeof(p.ram)) == 0);
}
static void test_failures_and_resume(void) {
    /* Fail each startup transfer, and a later data transfer. Every failure must
       back off and then recover at the SAME stored brightness. */
    for (unsigned fail = 1; fail <= 12; fail++) {
        Panel p;
        setup(&p);
        p.fail_at = fail;
        for (uint32_t ms = 0; ms < 1700; ms++)
            oled_task(&p.oled, ms, true, 70);
        assert(p.on && p.contrast == 178 && p.data_writes >= 64);
        oled_suspend(&p.oled);
        assert(!p.on && !p.pump);
        for (uint32_t ms = 2000; ms < 2300; ms++)
            oled_task(&p.oled, ms, true, 70);
        assert(p.on && p.pump && p.contrast == 178);
        for (uint32_t ms = 2300; ms < 2350; ms++)
            oled_task(&p.oled, ms, false, 70);
        assert(!p.on && !p.pump);
        for (uint32_t ms = 2400; ms < 2700; ms++)
            oled_task(&p.oled, ms, true, 100);
        assert(p.on && p.contrast == 255);
    }
}
static void test_idle_window_and_wrap(void) {
    assert(service_ready(100, 0, 0, 250, 4000));
    assert(!service_ready(700, 0, 0, 250, 4000));
    assert(service_ready(4700, 4000, 700, 250, 4000));
    assert(service_ready(3000, 2000, UINT32_MAX - 1000, 250, 4000));
    Panel p;
    setup(&p);
    const uint32_t start = UINT32_MAX - 80;
    for (uint32_t delta = 0; delta < 300; delta++)
        oled_task(&p.oled, start + delta, true, 70);
    assert(p.on && p.contrast == 178 && p.data_writes >= 64);
    oled_suspend(&p.oled);
    oled_task(&p.oled, UINT32_MAX - 40, true, 70);
    assert(p.oled.ready);
}
int main(void) {
    test_busy_main_loop();
    test_failures_and_resume();
    test_idle_window_and_wrap();
    puts("PASS: busy-loop display service, complete frame, I2C fault recovery, brightness, "
         "power/suspend/resume and clock wrap");
}
