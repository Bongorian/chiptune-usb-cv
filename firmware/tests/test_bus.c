#include "engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static RegisterBus bus;
static uint32_t now, started, minimum;
static bool held;
static uint8_t chip, byte, latch[2], saa[32];
static uint16_t sn[2][8];
static unsigned env_resets;
static void saa_write(void *ctx, uint8_t r, uint8_t value) {
    (void)ctx;
    assert(!held);
    assert(r < 6 || (r >= 8 && r <= 13) || (r >= 0x10 && r <= 0x16) || r == 0x18 || r == 0x19 ||
           r == 0x1c);
    saa[r] = value;
    if (r == 0x18 && !value)
        env_resets++;
}
static void begin(void *ctx, uint8_t c, uint8_t value) {
    (void)ctx;
    assert(!held);
    held = true;
    chip = c;
    byte = value;
    started = now;
}
static void end(void *ctx) {
    (void)ctx;
    assert(held && now - started >= minimum);
    held = false;
    if (byte & 128) {
        latch[chip] = (byte >> 4) & 7;
        sn[chip][latch[chip]] = (uint16_t)((sn[chip][latch[chip]] & 0x3f0) | (byte & 15));
    } else
        sn[chip][latch[chip]] = (uint16_t)((sn[chip][latch[chip]] & 15) | ((byte & 63) << 4));
}
static void service(unsigned wait) {
    minimum = wait;
    bus_service(&bus, now, wait);
    now += 4;
}
static void drain(unsigned wait) {
    for (unsigned i = 0; bus_pending(&bus) && i < 5000; i++)
        service(wait);
    assert(!bus_pending(&bus) && !held);
}
int main(void) {
    bus_init(&bus, (SoundSink){saa_write, begin, end, NULL});
    drain(16);
    bus_tone(&bus, 0, 0, 568);
    bus_volume(&bus, 0, 0, 3);
    drain(136);
    assert(sn[0][0] == 568 && (sn[0][1] & 15) == 3);
    for (unsigned n = 1; n < 1000; n++)
        bus_tone(&bus, 0, 0, n);
    assert(bus_pending(&bus) <= 1 && bus.coalesced > 900);
    drain(136);
    assert(sn[0][0] == 999);
    bus_tone(&bus, 1, 2, 425);
    while (!bus.busy)
        service(16);
    bus_tone(&bus, 1, 2, 777);
    drain(16);
    assert(sn[1][4] == 777);
    unsigned initial = env_resets;
    bus_saa(&bus, 0x18, 0x8a);
    drain(16);
    bus_env_retrigger(&bus, 0);
    bus_saa(&bus, 0x18, 0x8a);
    drain(16);
    assert(env_resets == initial + 1 && saa[0x18] == 0x8a);
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 4; n++)
            bus_volume(&bus, c, n, 2);
    drain(16);
    bus_mute(&bus);
    drain(136);
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 4; n++)
            assert((sn[c][n * 2 + 1] & 15) == 15);
    /* Slowing the clock while /WE is low extends the in-flight transaction. */
    bus_tone(&bus, 0, 1, 222);
    while (!bus.busy)
        service(16);
    uint32_t start = bus.started;
    for (unsigned i = 0; i < 34; i++)
        service(136);
    assert(now - start >= 136);
    drain(136);
    assert(bus.max_pending <= 50);
    puts("PASS bus: coalescing, byte-pair atomicity, reset events, panic priority, asynchronous "
         "16–136us hold and clock transitions");
}
