#include "engine.h"
#include <limits.h>
static uint32_t random_next(Engine *e) {
    uint32_t x = e->random;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    e->random = x;
    return x;
}
void sequence_run(Engine *e, bool run, bool restart) {
    e->running = run;
    for (unsigned t = 0; t < TRACKS; t++) {
        if (!run) {
            engine_release_source(e, t, 5);
            if (e->project.sequence[t].arp)
                for (unsigned s = 0; s < SOURCES; s++)
                    engine_release_source(e, t, s);
        }
        if (restart) {
            e->motion[t].position = e->motion[t].arp_position = 0;
            e->motion[t].pulse = UINT32_MAX;
        }
    }
}
void sequence_gate(Engine *e, unsigned t) {
    if (t >= TRACKS || !e->project.track[t].enabled)
        return;
    const Sequence *q = &e->project.sequence[t];
    SequenceRuntime *r = &e->motion[t];
    unsigned pos = r->position % q->length;
    if (q->direction == 1)
        pos = q->length - 1 - pos;
    if (q->direction == 2)
        pos = random_next(e) % q->length;
    engine_release_source(e, t, 5);
    r->lane = q->motion[pos];
    if (q->gate[pos] && q->velocity[pos]) {
        engine_note(e, t, 5, t, q->note[pos], q->velocity[pos], ++e->age, q->note[pos]);
        unsigned duration = 60000u * q->division / (24u * (e->project.tempo + 30u));
        duration = duration * q->gate[pos] / 100;
        r->due = e->now + (duration ? duration : 1);
    }
    r->position = (uint8_t)((r->position + 1) % q->length);
}
static void arp(Engine *e, unsigned t) {
    const Sequence *q = &e->project.sequence[t];
    unsigned order[HELD_NOTES], count = 0;
    for (unsigned h = 0; h < HELD_NOTES; h++)
        if (e->held[t][h].velocity)
            order[count++] = h;
    if (!count)
        return;
    for (unsigned i = 1; i < count; i++)
        for (unsigned j = i; j && e->held[t][order[j]].note < e->held[t][order[j - 1]].note; j--) {
            unsigned v = order[j];
            order[j] = order[j - 1];
            order[j - 1] = v;
        }
    for (unsigned s = 0; s < SOURCES; s++)
        engine_release_source(e, t, s);
    unsigned pos = e->motion[t].arp_position++, span = count * q->arp_octaves, index = pos % span;
    if (q->arp == 2)
        index = span - 1 - index;
    if (q->arp == 3 && span > 1) {
        index = pos % (2 * (span - 1));
        if (index >= span)
            index = 2 * (span - 1) - index;
    }
    if (q->arp == 4)
        index = random_next(e) % span;
    for (unsigned i = 0; i < (q->arp == 5 ? count : 1); i++) {
        HeldNote *h = &e->held[t][order[q->arp == 5 ? i : index % count]];
        engine_note(e, t, h->source, h->channel, h->note, h->velocity, h->token,
                    (float)h->note + (float)(index / count) * 12);
        if (h->sustained)
            for (unsigned v = 0; v < 14; v++)
                if (e->voice[v].active && e->voice[v].track == t && e->voice[v].token == h->token &&
                    e->voice[v].source == h->source) {
                    e->voice[v].held = false;
                    e->voice[v].sustained = true;
                }
    }
}
void sequence_tick(Engine *e, uint32_t dt) {
    static const uint32_t step[] = {2147484,  4294967,  8589935,   17179869,
                                    34359738, 68719477, 137438953, 274877907};
    uint32_t clock = engine_transport_clocks(e);
    for (unsigned t = 0; t < TRACKS; t++) {
        const Track *a = &e->project.track[t];
        SequenceRuntime *r = &e->motion[t];
        const Sequence *q = &e->project.sequence[t];
        if (!a->enabled)
            continue;
        uint32_t old = r->phase;
        r->phase += step[a->patch.lfo_rate] * dt;
        if (r->phase < old)
            r->random_lfo = (float)(random_next(e) & 65535) * (1.0f / 32767.5f) - 1;
        unsigned shape = a->patch.lfo_shape;
        float phase = (float)(r->phase >> 8) * (1.0f / 16777216.0f);
        e->lfo[t] = shape == 0   ? fast_sine(r->phase)
                    : shape == 1 ? 1 - 4 * (phase > .5f ? phase - .5f : .5f - phase)
                    : shape == 2 ? (phase < .5f ? 1 : -1)
                                 : r->random_lfo;
        if (r->due && (int32_t)(e->now - r->due) >= 0) {
            engine_release_source(e, t, 5);
            r->due = 0;
        }
        uint32_t pulse = clock / q->division;
        if (pulse != r->pulse && e->clock_running) {
            bool gated = false;
            for (unsigned i = 0; i < 2; i++)
                if (e->project.cv[i].gate == 3 && e->project.cv[i].gate_track == t &&
                    !(e->project.clock_policy == 3 && e->project.gate_clock == i))
                    gated = true;
            if (e->running && q->enabled && !gated)
                sequence_gate(e, t);
            if (q->arp)
                arp(e, t);
            r->pulse = pulse;
        }
    }
    if (e->test_on) {
        if (e->test_mode != 3 && (int32_t)(e->now - e->test_off) >= 0) {
            engine_release_source(e, e->test_track, 4);
            if (e->test_mode == 1)
                e->test_on = false;
            e->test_off = e->now + 1000;
        }
        if (e->test_mode == 2 && (int32_t)(e->now - e->test_due) >= 0) {
            engine_note(e, e->test_track, 4, 0, e->test_note, 100, ++e->age, e->test_note);
            e->test_due = e->now + 1000;
            e->test_off = e->now + 400;
        }
    }
}
