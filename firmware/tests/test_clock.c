#include "test_common.h"
#include <float.h>
static void near(double a, double b, double tolerance) {
    if (fabs(a - b) > tolerance)
        fprintf(stderr, "near %.17g %.17g tolerance %.17g\n", a, b, tolerance);
    assert(fabs(a - b) <= tolerance);
}
static void plans(void) {
    SnClockPlan p = sn_clock_plan(2000000);
    assert(p.available && !p.experimental);
    near(p.hz, 2000000, 1e-8);
    near(sn_clock_plan(sn_clock_min_hz()).hz, sn_clock_min_hz(), 1e-20);
    near(sn_clock_plan(sn_clock_max_hz()).hz, sn_clock_max_hz(), 1e-8);
    assert(!sn_clock_plan(sn_clock_max_hz() * 1.00001).available);
    assert(!sn_clock_plan(sn_clock_min_hz() * .99999).available);
    assert(!sn_clock_plan(0).available && !sn_clock_plan(NAN).available);
    assert(!sn_clock_plan(INFINITY).available && !sn_clock_plan(-1).available);
    assert(!sn_clock_direct(0, 255).available &&
           !sn_clock_direct(0, SN_CLOCK_DIV_MAX + 1).available);
    for (double hz = sn_clock_min_hz(); hz <= sn_clock_max_hz(); hz *= 1.013) {
        p = sn_clock_plan(hz);
        assert(p.available);
        assert(fabs(p.hz / hz - 1) < .0021);
        near(p.hz, sn_clock_direct(p.period, p.div256).hz, 0);
    }
    assert(sn_clock_plan(8000000).experimental);
    assert(sn_clock_plan(1).available && sn_clock_plan(100).available);
}
static void modulation(void) {
    SnClockConfig c;
    sn_clock_default(&c);
    SnClock s;
    assert(sn_clock_valid(&c));
    sn_clock_init(&s, &c);
    near(sn_clock_sample(&s).hz, 2000000, 0);
    c.direct = 1;
    c.period = UINT32_MAX;
    c.div256 = SN_CLOCK_DIV_MAX;
    sn_clock_configure(&s, &c);
    SnClockPlan p = sn_clock_sample(&s);
    assert(p.period == UINT32_MAX && p.div256 == SN_CLOCK_DIV_MAX);
    sn_clock_default(&c);
    c.mod[0].mode = SN_CLOCK_MANUAL;
    c.mod[0].offset = 1200;
    sn_clock_configure(&s, &c);
    near(sn_clock_sample(&s).hz, 4000000, 1e-6);
    c.mod[1] = c.mod[0];
    c.mod[1].offset = -1200;
    sn_clock_configure(&s, &c);
    near(sn_clock_sample(&s).hz, 2000000, 1e-6);
    c.mod[1].mode = SN_CLOCK_OFF;
    c.mod[0].domain = SN_DOMAIN_HZ;
    c.mod[0].offset = -1999900;
    sn_clock_configure(&s, &c);
    near(sn_clock_sample(&s).hz, 100, .001);
    c.mod[0].offset = 500000;
    c.mod[0].domain = SN_DOMAIN_RATIO;
    sn_clock_configure(&s, &c);
    near(sn_clock_sample(&s).hz, 3000000, 3000);
    c.mod[0].domain = SN_DOMAIN_PERIOD;
    c.mod[0].offset = 32;
    c.direct = 1;
    c.period = 27;
    c.div256 = 256;
    sn_clock_configure(&s, &c);
    near(sn_clock_sample(&s).hz, 1000000, 1);
    c.mod[0].domain = SN_DOMAIN_HZ;
    c.mod[0].offset = -2000001;
    sn_clock_configure(&s, &c);
    assert(!sn_clock_sample(&s).available);
    for (unsigned mode = 2; mode < SN_CLOCK_MODES; mode++) {
        sn_clock_default(&c);
        c.mod[0].mode = (uint8_t)mode;
        c.mod[0].depth = 300;
        c.mod[0].rate_millihz = 10000;
        sn_clock_init(&s, &c);
        sn_clock_trigger(&s, true, 0);
        double min = DBL_MAX, max = 0;
        for (unsigned t = 0; t < 200000; t += 250) {
            p = sn_clock_tick(&s, 250, t);
            assert(p.available);
            if (p.hz < min)
                min = p.hz;
            if (p.hz > max)
                max = p.hz;
        }
        assert(max > min + 1000);
    }
    sn_clock_default(&c);
    c.mod[0].mode = SN_CLOCK_TRIANGLE;
    c.mod[0].sync = 8;
    c.mod[0].depth = 1200;
    sn_clock_init(&s, &c);
    sn_clock_midi(&s, 1, 20000);
    sn_clock_midi(&s, 2, 40000);
    p = sn_clock_tick(&s, 250, 41000);
    double a = p.hz;
    p = sn_clock_tick(&s, 250, 41250);
    assert(p.hz != a); /* interpolates without a new MIDI clock */
    c.mod[0].rate_millihz = UINT32_MAX;
    c.mod[0].sync = 0;
    sn_clock_configure(&s, &c);
    assert(sn_clock_tick(&s, UINT32_MAX, UINT32_MAX).available); /* no integer overflow/UB */
}
static void counting(void) {
    SnClockCounter c;
    sn_counter_init(&c, sn_clock_plan(1), 0);
    sn_counter_advance(&c, 39999999);
    assert(c.cycles == 38);
    sn_counter_advance(&c, 41000000);
    assert(c.cycles == 40);
    sn_counter_change(&c, sn_clock_plan(.1), 41000000, false);
    sn_counter_advance(&c, 61000000);
    assert(c.cycles == 41);
    sn_counter_change(&c, sn_clock_plan(8000000), 61000000, false);
    sn_counter_advance(&c, 61000010);
    assert(c.cycles >= 120);
    uint64_t n = c.cycles;
    sn_counter_advance(&c, 61000001);
    assert(c.cycles == n);
    sn_counter_change(&c, c.plan, 61000010, true);
    sn_counter_advance(&c, UINT64_C(9999999999999));
    assert(c.cycles == n);
    sn_counter_change(&c, sn_clock_plan(sn_clock_min_hz()), UINT64_C(9999999999999), false);
    sn_counter_advance(&c, UINT64_C(9999999999999) + UINT64_C(1000000000000000));
    assert(c.cycles > n);
}
static void follow(void) {
    setup();
    message(0, 0x96, 69, 100);
    message(0, 0x97, 72, 100);
    message(0, 0x98, 76, 100);
    engine.project.sn_clock.mod[0].offset = INT32_MAX;
    engine.clock_base = 100;
    assert(engine_effective_clock(&engine).mod[0].offset == INT32_MAX);
    engine.project.sn_clock.mod[0].offset = 0;
    engine.clock_base = 0;
    unsigned active = engine_active(&engine);
    assert(active == 3);
    Project p = engine.project;
    for (unsigned mode = 0; mode < 3; mode++) {
        p.sn_clock.follow = (uint8_t)mode;
        p.sn_clock.base_hz = 1000000;
        assert(engine_apply(&engine, &p));
        assert(engine_active(&engine) == active);
        engine.sn_clock_hz = 500000;
        engine_tick(&engine, 1);
        float ref = mode == 0 ? 2000000 : mode == 1 ? 1000000 : 500000;
        for (unsigned c = 0; c < 2; c++)
            for (unsigned v = 0; v < 3; v++) {
                float target = engine.sn_target_hz[c][v];
                assert(target > 0);
                bool clip;
                assert(engine.bus.tone[c][v] == sn_divider_clock(target, ref, &clip));
            }
    }
    engine.sn_clock_hz = 1;
    engine_tick(&engine, 1);
    assert(engine.sn_clip_mask == 63 && engine_active(&engine) == active);
    engine.sn_clock_hz = 8000000;
    engine_tick(&engine, 1);
    assert(engine_active(&engine) == active);
    setup();
    Patch pulse = engine.project.track[9].patch;
    assert(patch_set_wave(&pulse, WAVE_PULSE));
    /* Pulse requires Tone3; disable the conflicting direct tone track explicitly. */
    p = engine.project;
    p.track[8].enabled = 0;
    p.track[9].patch = pulse;
    p.sn_clock.follow = SN_FOLLOW_FULL;
    assert(engine_apply(&engine, &p));
    message(0, 0x99, 69, 100);
    engine.sn_clock_hz = 1000000;
    engine_tick(&engine, 1);
    for (unsigned c = 0; c < 2; c++) {
        assert(engine.sn_target_hz[c][2] > 6500);
        bool clip;
        assert(engine.bus.tone[c][2] ==
               sn_divider_clock(engine.sn_target_hz[c][2], 1000000, &clip));
    }
    engine_panic(&engine, -1);
    engine_tick(&engine, 1);
    assert(!engine_active(&engine));
}
int main(void) {
    plans();
    modulation();
    counting();
    follow();
    puts("PASS clock: full PIO range, two-slot FX/domains, interpolated MIDI, conservative "
         "cycle accounting, RAW/BASE/FULL including stereo Pulse, clamps and panic");
}
