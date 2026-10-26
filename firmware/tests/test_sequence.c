#include "test_common.h"
#include "cv.h"
static void sequencing(void) {
    setup();
    isolated(20, PLAY_DIRECT, 1);
    engine.project.sequence[0].enabled = 1;
    sequence_run(&engine, true, true);
    advance(1);
    assert(engine.motion[0].position == 1 && engine_active(&engine) == 1);
    advance(90);
    assert(!engine_active(&engine));
    advance(40);
    assert(engine.motion[0].position == 2);
    engine.project.clock_policy = 1;
    uint8_t stop = 0xfc, start = 0xfa, cont = 0xfb, clock = 0xf8;
    engine_message(&engine, 0, &stop, 1);
    assert(!engine.running);
    unsigned pos = engine.motion[0].position;
    engine_message(&engine, 0, &cont, 1);
    assert(engine.motion[0].position == pos);
    engine_message(&engine, 0, &start, 1);
    advance(1);
    assert(engine.motion[0].position == 1);
    for (unsigned i = 0; i < 6; i++)
        engine_message(&engine, 0, &clock, 1);
    advance(1);
    assert(engine.motion[0].position == 2);
    setup();
    isolated(20, PLAY_DIRECT, 1);
    engine.project.sequence[0].arp = 1;
    message(0, 0x90, 48, 100);
    message(0, 0x90, 55, 100);
    advance(150);
    assert(engine_active(&engine) == 1);
    message(0, 0x80, 48, 0);
    message(0, 0x80, 55, 0);
    advance(10);
    assert(!engine_active(&engine));
    message(0, 0x90, 48, 100);
    message(0, 0xb0, 64, 127);
    message(0, 0x80, 48, 0);
    advance(150);
    assert(engine_active(&engine) == 1);
    message(0, 0xb0, 64, 0);
    advance(1);
    assert(!engine_active(&engine));
}
static void cv_gate(void) {
    setup();
    isolated(20, PLAY_DIRECT, 1);
    engine.project.cv[0] = (CvRoute){.mode = 1,
                                     .track = 0,
                                     .base_note = 45,
                                     .gate = 1,
                                     .gate_track = 0,
                                     .gate_note = 60,
                                     .amount = 100};
    engine_cv(&engine, 0, 0, true, true);
    advance(1);
    assert(engine_active(&engine) == 1);
    for (unsigned i = 0; i < 14; i++)
        if (engine.voice[i].active)
            assert(engine.voice[i].target == 45);
    engine_cv(&engine, 0, 1, true, true);
    advance(1);
    for (unsigned i = 0; i < 14; i++)
        if (engine.voice[i].active)
            assert(engine.voice[i].target == 57);
    engine_cv(&engine, 0, NAN, true, false);
    advance(1);
    assert(!engine_active(&engine));
    engine_cv(&engine, 0, 0, false, true);
    engine_cv(&engine, 0, 0, true, true);
    advance(1);
    assert(engine_active(&engine) == 1);
    Project p = engine.project;
    p.cv[0].gate_track = 1;
    p.track[1].enabled = 1;
    assert(engine_apply(&engine, &p));
    advance(1);
    assert(!engine_active(&engine));
    engine_cv(&engine, 0, 0, true, true);
    assert(!engine_active(&engine));
    bool zero[2] = {false, false};
    int16_t samples[64];
    for (unsigned i = 0; i < 64; i++)
        samples[i] = 10;
    assert(cv_calibration(&engine.system, 0, samples, false, zero));
    for (unsigned i = 0; i < 64; i++)
        samples[i] = 1460;
    assert(cv_calibration(&engine.system, 0, samples, true, zero));
    assert(fabsf(cv_volts(&engine.system, 0, 1460) - 1) < .001f);
    assert(fabsf(engine_quantize(60.4f, 1, 0) - 60) < .001f);
    assert(engine_quantize(61, 2, 0) == 60);
}
int main(void) {
    sequencing();
    cv_gate();
    puts("PASS sequence/CV: independent event generator, MIDI Start/Continue/Stop, arp ownership, "
         "0V/freshness, reroute gating, calibration and quantization");
}
