#include "test_common.h"
#include "midi.h"
#include "power.h"
static MidiParser parser;
static unsigned messages, sysex, realtime;
static void callback(void *ctx, const uint8_t *d, uint8_t n, uint8_t cin) {
    (void)ctx;
    (void)cin;
    if (d[0] >= 0xf8)
        realtime++;
    else if (d[0] == 0xf0 || (cin >= 4 && cin <= 7))
        sysex++;
    else {
        messages++;
        engine_message(&engine, 0, d, n);
    }
}
int main(void) {
    setup();
    isolated(20, PLAY_POLY, 3);
    midi_init(&parser, callback, NULL);
    const uint8_t bytes[] = {0x90, 60, 100, 64, 100, 0xf8, 67, 100, 0x80, 60, 0, 64, 0, 67, 0};
    for (unsigned i = 0; i < sizeof(bytes); i++)
        midi_byte(&parser, bytes[i], i);
    advance(1);
    assert(messages == 6 && realtime == 1 && !engine_active(&engine));
    const uint8_t sx[] = {0xf0, 125, 67, 84, 0xf8, 2, 1, 0, 0, 0xf7};
    for (unsigned i = 0; i < sizeof(sx); i++)
        midi_byte(&parser, sx[i], i);
    assert(sysex == 3 && realtime == 2);
    midi_byte(&parser, 0xfe, 100);
    assert(!midi_sensing_expired(&parser, 399));
    assert(midi_sensing_expired(&parser, 401));
    engine.pressed[0][0][60] = engine.released[0][0][60] = UINT32_MAX - 1;
    message(0, 0x90, 60, 100);
    message(0, 0x90, 60, 100);
    assert(engine_active(&engine) == 2);
    message(0, 0x80, 60, 0);
    assert(engine_active(&engine) == 1);
    message(0, 0x80, 60, 0);
    assert(!engine_active(&engine));
    uint32_t random = 0x12345678;
    for (unsigned i = 0; i < 10000; i++) {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        unsigned n = random % 128, kind = (random >> 8) % 5;
        message((random >> 16) & 1,
                kind == 0   ? 0x80
                : kind == 1 ? 0xb0
                            : 0x90,
                n, kind == 1 ? 0 : 100);
        assert(engine_active(&engine) <= 3);
    }
    engine_panic(&engine, -1);
    assert(!engine_active(&engine));
    puts("PASS MIDI: running status, realtime/SysEx, sensing timeout, 32-bit token wrap and 10,000 "
         "adversarial events");
}
