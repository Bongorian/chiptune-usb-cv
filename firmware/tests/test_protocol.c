#include "test_common.h"
static Editor ed;
static uint8_t payload[192];
static unsigned request(unsigned command, const uint8_t *data, unsigned n, unsigned version,
                        bool corrupt, bool blocked) {
    assert(!ed.length);
    uint8_t body[192] = {125, 67, 84, (uint8_t)version, 3, (uint8_t)command};
    assert(n + 7 <= sizeof(body));
    if (n)
        memcpy(body + 6, data, n);
    unsigned sum = 0;
    for (unsigned i = 0; i < 6 + n; i++)
        sum += body[i];
    body[6 + n] = (uint8_t)((0u - sum) & 127);
    if (corrupt)
        body[6 + n] ^= 1;
    editor_byte(&ed, 240, 0);
    for (unsigned i = 0; i < 7 + n; i++)
        editor_byte(&ed, body[i], 1);
    editor_byte(&ed, 247, 1);
    editor_task(&ed, &engine, &settings, blocked, NULL, NULL, 1);
    assert(ed.length >= 10 && ed.tx[0] == 240 && ed.tx[ed.length - 1] == 247);
    return ed.tx[7];
}
static void drain(void) {
    uint8_t packet[4];
    unsigned count = 0;
    while (editor_packet(&ed, packet)) {
        unsigned n = packet[0] == 4 ? 3 : packet[0] - 4;
        assert(n > 0 && n <= 3);
        count += n;
    }
    assert(count);
}
int main(void) {
    setup();
    editor_init(&ed);
    assert(request(24, (uint8_t[]){0}, 1, 2, false, false) == 5);
    drain();
    assert(request(19, NULL, 0, 2, false, false) == 5);
    drain();
    assert(request(0, (uint8_t[]){10}, 1, 1, false, false) == 1);
    drain();
    assert(!ed.negotiated);
    assert(request(0, (uint8_t[]){9}, 1, 2, false, false) == 5);
    drain();
    assert(!request(0, (uint8_t[]){10}, 1, 2, false, false));
    assert(ed.tx[9] == 10 && ed.tx[12] == 10);
    drain();
    assert(!request(1, (uint8_t[]){0}, 1, 2, false, false));
    unsigned n = ed.length - 10 - 4;
    memcpy(payload, ed.tx + 8, n);
    drain();
    assert(n == 39 + 2 * editor_field_count());
    payload[12] = 'W';
    payload[4] = 55;
    assert(!request(2, payload, n, 2, false, false));
    drain();
    assert(engine.project.track[0].level == 55 && engine.project.track[0].name[0] == 'W');
    Project before = engine.project;
    assert(request(2, payload, n, 2, true, false) == 1);
    drain();
    assert(!memcmp(&before, &engine.project, sizeof(before)));
    assert(request(2, payload, n, 2, false, true) == 3);
    drain();
    payload[32] = 0;
    payload[33] = 0;
    payload[34] = 2; /* USB CH16, full 16-bit mask roundtrip. */
    assert(!request(2, payload, n, 2, false, false));
    assert(ed.tx[8 + 34] == 2);
    drain();
    assert(engine.project.route[0][15] & 1);
    payload[3] = 6;
    payload[2] = 1;
    assert(request(2, payload, n, 2, false, false) == 2);
    assert(ed.tx[12] > 0 && ed.tx[13] > 0);
    drain();
    assert(!request(8, (uint8_t[]){0}, 1, 2, false, false));
    unsigned seqn = ed.length - 10;
    assert(seqn == 87);
    memcpy(payload, ed.tx + 8, seqn);
    drain();
    payload[55] = 28;
    payload[56] = 63;
    assert(!request(9, payload, seqn, 2, false, false));
    drain();
    assert(engine.project.sequence[0].motion[0] == -100);
    for (unsigned f = 0; f < FACTORY_PATCHES; f++) {
        assert(!request(12, (uint8_t[]){(uint8_t)f}, 1, 2, false, false));
        drain();
    }
    assert(!request(4, NULL, 0, 2, false, false));
    assert(ed.length - 10 == 26);
    memcpy(payload, ed.tx + 8, 26);
    drain();
    payload[10] = 72;
    payload[11] = 1; /* 230 BPM: 200 + 30, full range. */
    payload[25] = 1;
    assert(!request(5, payload, 26, 2, false, false));
    assert(engine.project.tempo == 200 && engine.project.gate_clock == 1);
    drain();
    assert(!request(18, (uint8_t[]){46}, 1, 2, false, false));
    assert(engine.system.brightness == 46 && settings.system.brightness != 46);
    drain();
    assert(request(3, (uint8_t[]){SAVE_PATCH, 0, 0}, 3, 2, false, false) == 4);
    drain();
    editor_byte(&ed, 240, 10);
    for (unsigned i = 0; i < 300; i++)
        editor_byte(&ed, 1, 10);
    editor_byte(&ed, 247, 10);
    assert(!ed.ready);
    editor_byte(&ed, 240, 10);
    editor_byte(&ed, 125, 10);
    editor_byte(&ed, 247, 3000);
    editor_task(&ed, &engine, &settings, false, NULL, NULL, 3000);
    assert(!ed.length);
    puts("PASS protocol: exact schema handshake, old-version rejection, C roundtrip, USB ch16, "
         "signed sequence, packet framing, busy/corrupt/resource/overflow/save rejection");
}
