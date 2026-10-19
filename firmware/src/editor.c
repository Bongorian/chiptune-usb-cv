#include "editor.h"
#include "studio.h"
#include <string.h>
enum { U8, I8, U16 };
typedef struct {
    size_t offset;
    int min, max;
    unsigned type;
} Field;
#define FIELD(name, type, min, max) {offsetof(StSound, name), min, max, type},
static const Field fields[] = {
#include "sound_fields.inc"
};
#undef FIELD
_Static_assert(EDITOR_FIELDS == sizeof(fields) / sizeof(fields[0]), "Editor field schema");
_Static_assert(28 + 2 * EDITOR_FIELDS <= EDITOR_BYTES, "Editor response buffer capacity");
unsigned editor_field_count(void) {
    return sizeof(fields) / sizeof(fields[0]);
}
int editor_field_get(const StSound *s, unsigned field) {
    if (field >= editor_field_count())
        return 0;
    const Field *f = &fields[field];
    const uint8_t *p = (const uint8_t *)s + f->offset;
    if (f->type == U16) {
        uint16_t n;
        memcpy(&n, p, 2);
        return n;
    }
    return f->type == I8 ? *(const int8_t *)p : *p;
}
bool editor_field_set(StSound *s, unsigned field, int value) {
    if (field >= editor_field_count())
        return false;
    const Field *f = &fields[field];
    if (value < f->min || value > f->max)
        return false;
    uint8_t *p = (uint8_t *)s + f->offset;
    if (f->type == U16) {
        uint16_t n = (uint16_t)value;
        memcpy(p, &n, 2);
    } else if (f->type == I8)
        *(int8_t *)p = (int8_t)value;
    else
        *p = (uint8_t)value;
    return true;
}
void editor_init(Editor *e) {
    memset(e, 0, sizeof(*e));
}
void editor_byte(Editor *e, uint8_t b, uint32_t now) {
    if (b >= 0xf8)
        return; /* Realtime may be interleaved within SysEx. */
    if (e->receiving && now - e->last_byte > 2000)
        e->receiving = false;
    e->last_byte = now;
    if (b == 0xf0) {
        e->receiving = !e->ready && !e->length;
        e->count = 0;
        return;
    }
    if (!e->receiving)
        return;
    if (b == 0xf7) {
        e->ready = true;
        e->receiving = false;
        return;
    }
    if (b >= 0x80 || e->count >= EDITOR_BYTES) {
        e->receiving = false;
        return;
    }
    e->rx[e->count++] = b;
}
static void put(Editor *e, unsigned v) {
    e->tx[e->length++] = (uint8_t)(v & 127);
}
static void word(Editor *e, unsigned v) {
    put(e, v);
    put(e, v >> 7);
}
static void sound_write(Editor *ed, const StSound *s) {
    put(ed, editor_field_count());
    for (unsigned i = 0; i < editor_field_count(); i++)
        word(ed, (unsigned)(editor_field_get(s, i) + 8192));
}
static bool sound_read(StSound *s, const uint8_t *b, unsigned n) {
    if (n != 1 + 2 * editor_field_count() || b[0] != editor_field_count())
        return false;
    studio_sound_default(s, ST_SAA_TONE);
    for (unsigned i = 0; i < editor_field_count(); i++)
        if (!editor_field_set(s, i, (int)(b[1 + 2 * i] | b[2 + 2 * i] << 7) - 8192))
            return false;
    return studio_sound_valid(s);
}
static void part_write(Editor *ed, const Engine *e, unsigned part) {
    const StPart *p = &e->preset.studio.part[part];
    put(ed, part);
    put(ed, p->enabled);
    put(ed, p->poly);
    put(ed, p->role);
    unsigned channel = 127;
    for (unsigned c = 0; c < 16; c++)
        if (e->preset.studio.route[0][c] & (1u << part)) {
            if (channel != 127) {
                channel = 127;
                break;
            }
            channel = c;
        }
    put(ed, channel);
    for (unsigned i = 0; i < 8; i++)
        put(ed, (unsigned)p->name[i]);
    sound_write(ed, &p->sound);
    word(ed, e->studio.outputs[part]);
    word(ed, e->studio.resources[part]);
}
static void clock_write(Editor *ed, const Preset *p) {
    put(ed, p->sn_reference);
    put(ed, p->sn_clock.mode);
    put(ed, p->sn_clock.base);
    put(ed, p->sn_clock.depth);
    put(ed, p->sn_clock.rate);
    put(ed, p->sn_clock.sync);
    put(ed, p->sn_clock.retrigger);
    put(ed, p->sn_width);
    put(ed, p->sn_trigger_source);
    for (unsigned i = 0; i < 8; i++)
        put(ed, p->clock_sequence[i]);
}
static void motion_write(Editor *ed, const Engine *e, unsigned p) {
    const StPart *a = &e->preset.studio.part[p];
    put(ed, p);
    put(ed, a->length);
    put(ed, a->division);
    put(ed, a->direction);
    put(ed, a->sequence);
    put(ed, a->arp);
    put(ed, a->arp_octaves);
    put(ed, e->studio.running);
    for (unsigned i = 0; i < 16; i++)
        put(ed, a->note[i]);
    for (unsigned i = 0; i < 16; i++)
        put(ed, a->gate[i]);
    for (unsigned i = 0; i < 16; i++)
        put(ed, a->velocity[i]);
    for (unsigned i = 0; i < 16; i++)
        word(ed, (unsigned)(a->motion[i] + 8192));
}
static void cv_write(Editor *ed, const StCv *c, unsigned input) {
    put(ed, input);
    put(ed, c->mode);
    put(ed, c->part);
    put(ed, c->base_note);
    put(ed, c->scale);
    put(ed, c->root);
    put(ed, c->glide);
    put(ed, c->range);
    put(ed, c->gate);
    put(ed, c->gate_part);
    put(ed, c->parameter);
    put(ed, c->gate_note);
    word(ed, (unsigned)(c->amount + 8192));
}
void editor_task(Editor *ed, Engine *e, Settings *s, bool blocked, bool (*save)(void *), void *ctx,
                 uint32_t now) {
    (void)now;
    if (!ed->ready || ed->length)
        return;
    ed->ready = false;
    const uint8_t *b = ed->rx;
    if (ed->count < 7 || b[0] != 0x7d || b[1] != 0x43 || b[2] != 0x54)
        return;
    unsigned sum = 0;
    for (unsigned i = 0; i < ed->count; i++)
        sum += b[i];
    ed->tx[0] = 0xf0;
    ed->length = 1;
    ed->sent = 0;
    put(ed, 0x7d);
    put(ed, 0x43);
    put(ed, 0x54);
    put(ed, EDITOR_PROTOCOL);
    put(ed, b[4]);
    put(ed, b[5] | 0x40);
    unsigned status = 0;
    put(ed, 0);
    unsigned cmd = b[5], n = ed->count - 7;
    const uint8_t *d = b + 6;
    static StScene candidate; /* Never allocate the scene on the RP2040's 4KiB stack. */
    if ((sum & 127) || b[3] != EDITOR_PROTOCOL || cmd >= 64) {
        status = 1;
        goto done;
    }
    if (blocked && cmd != 0 && cmd != 1 && cmd != 4 && cmd != 8 && cmd != 10 && cmd != 12) {
        status = 3;
        goto done;
    }
    switch (cmd) {
    case 0:
        if (n) {
            status = 1;
            break;
        }
        put(ed, 0);
        put(ed, 8);
        put(ed, 0);
        put(ed, SETTINGS_VERSION);
        put(ed, ST_PARTS);
        put(ed, ST_SOUNDS);
        put(ed, ST_FACTORIES);
        put(ed, editor_field_count());
        break;
    case 1:
        if (n != 1 || d[0] >= ST_PARTS) {
            status = 2;
            break;
        }
        part_write(ed, e, d[0]);
        break;
    case 2: {
        if (n != 14 + 2 * editor_field_count() || d[0] >= ST_PARTS || (d[4] > 15 && d[4] != 127)) {
            status = 2;
            break;
        }
        candidate = e->preset.studio;
        unsigned p = d[0];
        StPart *a = &candidate.part[p];
        a->enabled = d[1];
        a->poly = d[2];
        a->role = d[3];
        memcpy(a->name, d + 5, 8);
        a->name[8] = 0;
        if (!sound_read(&a->sound, d + 13, n - 13)) {
            status = 2;
            break;
        }
        if (d[4] != 127)
            for (unsigned src = 0; src < 2; src++)
                for (unsigned c = 0; c < 16; c++) {
                    candidate.route[src][c] &= (uint16_t) ~(1u << p);
                    if (c == d[4])
                        candidate.route[src][c] |= (uint16_t)(1u << p);
                }
        if (!studio_valid(&candidate)) {
            status = 2;
            break;
        }
        bool rerouted = false;
        for (unsigned src = 0; src < 2; src++)
            for (unsigned c = 0; c < 16; c++)
                if (candidate.route[src][c] != e->preset.studio.route[src][c])
                    rerouted = true;
        if (rerouted)
            studio_stop_part(e, p);
        if (!studio_apply(e, &candidate)) {
            status = 2;
            break;
        }
        ed->changed = true;
        part_write(ed, e, p);
        break;
    }
    case 3: {
        if (n != 2 || d[0] >= ST_PARTS || d[1] >= ST_SOUNDS) {
            status = 2;
            break;
        }
        static Preset previous;
        previous = s->preset[s->selected];
        StSound old = s->sounds[d[1]];
        char name[9];
        memcpy(name, s->sound_name[d[1]], 9);
        uint16_t used = s->sound_used;
        s->preset[s->selected] = e->preset;
        s->sounds[d[1]] = e->preset.studio.part[d[0]].sound;
        memcpy(s->sound_name[d[1]], e->preset.studio.part[d[0]].name, 9);
        s->sound_used |= (uint16_t)(1u << d[1]);
        if (!save || !save(ctx)) {
            s->preset[s->selected] = previous;
            s->sounds[d[1]] = old;
            memcpy(s->sound_name[d[1]], name, 9);
            s->sound_used = used;
            status = 4;
        } else
            ed->changed = true;
        break;
    }
    case 4:
        if (n)
            status = 1;
        else
            clock_write(ed, &e->preset);
        break;
    case 5: {
        if (n != 17 || d[0] > 2 || d[7] > 30 || d[8] > 7) {
            status = 2;
            break;
        }
        SnClockConfig c = {d[1], d[2], d[3], d[4], d[5], d[6]};
        if (!sn_clock_valid(&c)) {
            status = 2;
            break;
        }
        for (unsigned i = 9; i < 17; i++)
            if (d[i] > 100)
                status = 2;
        if (status)
            break;
        if (e->preset.sn_reference != d[0])
            engine_panic(e, -1);
        e->preset.sn_reference = d[0];
        e->preset.sn_clock = c;
        e->preset.sn_width = d[7];
        e->preset.sn_trigger_source = d[8];
        memcpy(e->preset.clock_sequence, d + 9, 8);
        ed->changed = true;
        clock_write(ed, &e->preset);
        break;
    }
    case 6:
        if (n != 1 || d[0] > 3) {
            status = 2;
            break;
        }
        studio_profile(&candidate, d[0]);
        if (!studio_valid(&candidate)) {
            status = 2;
            break;
        }
        engine_panic(e, -1);
        (void)studio_apply(e, &candidate);
        ed->changed = true;
        break;
    case 7:
        if (n != 3 || d[0] >= ST_PARTS || d[1] > 127 || d[2] > 3) {
            status = 2;
            break;
        }
        studio_test(e, d[0], d[1], d[2]);
        break;
    case 8:
        if (n != 1 || d[0] >= ST_PARTS) {
            status = 2;
            break;
        }
        motion_write(ed, e, d[0]);
        break;
    case 9: {
        if (n != 88 || d[0] >= ST_PARTS || d[7] > 1) {
            status = 2;
            break;
        }
        candidate = e->preset.studio;
        StPart *a = &candidate.part[d[0]];
        a->length = d[1];
        a->division = d[2];
        a->direction = d[3];
        a->sequence = d[4];
        a->arp = d[5];
        a->arp_octaves = d[6];
        memcpy(a->note, d + 8, 16);
        memcpy(a->gate, d + 24, 16);
        memcpy(a->velocity, d + 40, 16);
        for (unsigned i = 0; i < 16; i++) {
            int value = (int)(d[56 + 2 * i] | d[57 + 2 * i] << 7) - 8192;
            if (value < -100 || value > 100) {
                status = 2;
                break;
            }
            a->motion[i] = (int8_t)value;
        }
        if (status)
            break;
        if (!studio_apply(e, &candidate)) {
            status = 2;
            break;
        }
        if (!d[7])
            engine_panic(e, -1);
        e->studio.running = d[7] != 0;
        ed->changed = true;
        motion_write(ed, e, d[0]);
        break;
    }
    case 10:
        if (n != 1 || d[0] > 1) {
            status = 2;
            break;
        }
        cv_write(ed, &e->preset.studio.cv[d[0]], d[0]);
        break;
    case 11: {
        if (n != 14 && n != 28) {
            status = 2;
            break;
        }
        candidate = e->preset.studio;
        for (unsigned at = 0; at < n; at += 14) {
            const uint8_t *v = d + at;
            int amount = (int)(v[12] | v[13] << 7) - 8192;
            if (v[0] > 1 || amount < -100 || amount > 100 || (n == 28 && v[0] != at / 14)) {
                status = 2;
                break;
            }
            candidate.cv[v[0]] = (StCv){.mode = v[1],
                                        .part = v[2],
                                        .base_note = v[3],
                                        .scale = v[4],
                                        .root = v[5],
                                        .glide = v[6],
                                        .range = v[7],
                                        .gate = v[8],
                                        .gate_part = v[9],
                                        .parameter = v[10],
                                        .gate_note = v[11],
                                        .amount = (int8_t)amount};
        }
        if (status)
            break;
        if (!studio_apply(e, &candidate)) {
            status = 2;
            break;
        }
        ed->changed = true;
        if (n == 28) {
            cv_write(ed, &candidate.cv[0], 0);
            cv_write(ed, &candidate.cv[1], 1);
        } else
            cv_write(ed, &candidate.cv[d[0]], d[0]);
        break;
    }
    case 12: {
        if (n != 1 || d[0] >= ST_FACTORIES) {
            status = 2;
            break;
        }
        StSound sound;
        char name[9];
        studio_factory(d[0], &sound, name);
        put(ed, d[0]);
        for (unsigned i = 0; i < 8; i++)
            put(ed, (unsigned)name[i]);
        sound_write(ed, &sound);
        break;
    }
    default:
        status = 1;
        break;
    }
done:
    ed->tx[7] = (uint8_t)status;
    if (status)
        ed->length = 8;
    sum = 0;
    for (unsigned i = 1; i < ed->length; i++)
        sum += ed->tx[i];
    put(ed, (128 - (sum & 127)) & 127);
    ed->tx[ed->length++] = 0xf7;
}
bool editor_packet(Editor *e, uint8_t packet[4]) {
    if (!e->length)
        return false;
    unsigned left = e->length - e->sent, count = left < 3 ? left : 3;
    memset(packet, 0, 4);
    packet[0] = (uint8_t)(left <= 3 ? 4 + count : 4);
    memcpy(packet + 1, e->tx + e->sent, count);
    e->sent += count;
    if (e->sent == e->length)
        e->length = e->sent = 0;
    return true;
}
