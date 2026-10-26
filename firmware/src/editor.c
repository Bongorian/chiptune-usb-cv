#include "editor.h"
#include "persistence.h"
#include <string.h>
enum { U8, I8, U16 };
typedef struct {
    size_t offset;
    int min, max;
    unsigned type;
} Field;
#define FIELD(n, t, lo, hi) {offsetof(Patch, n), lo, hi, t},
static const Field fields[] = {
#include "sound_fields.inc"
};
#undef FIELD
unsigned editor_field_count(void) {
    return sizeof(fields) / sizeof(fields[0]);
}
int editor_field_get(const Patch *p, unsigned i) {
    if (i >= editor_field_count())
        return 0;
    const uint8_t *b = (const uint8_t *)p + fields[i].offset;
    if (fields[i].type == U16) {
        uint16_t n;
        memcpy(&n, b, 2);
        return n;
    }
    return fields[i].type == I8 ? *(const int8_t *)b : *b;
}
bool editor_field_set(Patch *p, unsigned i, int value) {
    if (i >= editor_field_count() || value < fields[i].min || value > fields[i].max)
        return false;
    uint8_t *b = (uint8_t *)p + fields[i].offset;
    if (fields[i].type == U16) {
        uint16_t n = (uint16_t)value;
        memcpy(b, &n, 2);
    } else if (fields[i].type == I8)
        *(int8_t *)b = (int8_t)value;
    else
        *b = (uint8_t)value;
    return true;
}
void editor_init(Editor *e) {
    memset(e, 0, sizeof(*e));
}
void editor_byte(Editor *e, uint8_t b, uint32_t now) {
    if (b >= 0xf8)
        return;
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
        e->receiving = false;
        e->ready = true;
        return;
    }
    if (b >= 0x80 || e->count >= EDITOR_BYTES) {
        e->receiving = false;
        return;
    }
    e->rx[e->count++] = b;
}
static void put(Editor *e, unsigned v) {
    if (e->length < EDITOR_BYTES)
        e->tx[e->length++] = (uint8_t)(v & 127);
}
static void word(Editor *e, unsigned n) {
    put(e, n);
    put(e, n >> 7);
}
static void wide(Editor *e, uint32_t n) {
    for (unsigned i = 0; i < 5; i++) {
        put(e, n);
        n >>= 7;
    }
}
static unsigned readword(const uint8_t *b) {
    return b[0] | (unsigned)b[1] << 7;
}
static void patch_write(Editor *ed, const Patch *p) {
    put(ed, editor_field_count());
    for (unsigned i = 0; i < editor_field_count(); i++)
        word(ed, (unsigned)(editor_field_get(p, i) + 8192));
}
static bool patch_read(Patch *p, const uint8_t *b, unsigned n) {
    if (n != 1 + 2 * editor_field_count() || b[0] != editor_field_count())
        return false;
    patch_default(p, CHIP_SAA);
    for (unsigned i = 0; i < editor_field_count(); i++)
        if (!editor_field_set(p, i, (int)readword(b + 1 + i * 2) - 8192))
            return false;
    return patch_valid(p);
}
static uint16_t routes(const Project *p, unsigned src, unsigned t) {
    uint16_t mask = 0;
    for (unsigned c = 0; c < 16; c++)
        if (p->route[src][c] & (1u << t))
            mask |= (uint16_t)(1u << c);
    return mask;
}
static void track_write(Editor *ed, const Engine *e, unsigned t) {
    const Track *a = &e->project.track[t];
    put(ed, t);
    const uint8_t values[] = {a->enabled, a->mode, a->voices, a->level,          a->pan,
                              a->mute,    a->low,  a->high,   a->velocity_curve, a->morph_target,
                              a->morph};
    for (unsigned i = 0; i < sizeof(values); i++)
        put(ed, values[i]);
    for (unsigned i = 0; i < 8; i++)
        put(ed, (unsigned)a->name[i]);
    for (unsigned i = 0; i < 4; i++)
        put(ed, a->macro_dest[i]);
    for (unsigned i = 0; i < 4; i++)
        word(ed, (unsigned)(a->macro[i] + 8192));
    /* 16 MIDI channels require three 7-bit bytes (unlike the 14-bit physical resource mask). */
    for (unsigned src = 0; src < 2; src++) {
        unsigned mask = routes(&e->project, src, t);
        word(ed, mask);
        put(ed, mask >> 14);
    }
    patch_write(ed, &a->patch);
    word(ed, resource_track_mask(&e->plan, t, true));
    word(ed, resource_track_mask(&e->plan, t, false));
}
static bool track_read(Project *p, const uint8_t *d, unsigned n) {
    if (n != 38 + 1 + 2 * editor_field_count() || d[0] >= TRACKS)
        return false;
    unsigned t = d[0];
    Track *a = &p->track[t];
    a->enabled = d[1];
    a->mode = d[2];
    a->voices = d[3];
    a->level = d[4];
    a->pan = d[5];
    a->mute = d[6];
    a->low = d[7];
    a->high = d[8];
    a->velocity_curve = d[9];
    a->morph_target = d[10];
    a->morph = d[11];
    memcpy(a->name, d + 12, 8);
    a->name[8] = 0;
    memcpy(a->macro_dest, d + 20, 4);
    for (unsigned i = 0; i < 4; i++) {
        int v = (int)readword(d + 24 + 2 * i) - 8192;
        if (v < -100 || v > 100)
            return false;
        a->macro[i] = (int8_t)v;
    }
    for (unsigned src = 0; src < 2; src++) {
        const uint8_t *b = d + 32 + src * 3;
        if (b[2] > 3)
            return false;
        unsigned mask = readword(b) | (unsigned)b[2] << 14;
        for (unsigned c = 0; c < 16; c++) {
            p->route[src][c] &= (uint16_t)~(1u << t);
            if (mask & (1u << c))
                p->route[src][c] |= (uint16_t)(1u << t);
        }
    }
    /* Prefix is 38 bytes: track+11 controls+name8+dest4+macro8+routes6. */
    return patch_read(&a->patch, d + 38, n - 38);
}
static void global_write(Editor *ed, const Project *p) {
    const uint8_t v[] = {p->sn_reference,
                         p->sn_clock.mode,
                         p->sn_clock.base,
                         p->sn_clock.depth,
                         p->sn_clock.rate,
                         p->sn_clock.sync,
                         p->sn_clock.retrigger,
                         p->sn_trigger_source,
                         p->clock_source,
                         p->clock_policy,
                         p->tempo,
                         p->midi_thru};
    for (unsigned i = 0; i < 12; i++)
        if (i == 10)
            word(ed, v[i]);
        else
            put(ed, v[i]);
    for (unsigned i = 0; i < 4; i++)
        put(ed, p->mix[i]);
    for (unsigned i = 0; i < 8; i++)
        put(ed, p->clock_sequence[i]);
    put(ed, p->gate_clock);
}
static void sequence_write(Editor *ed, const Project *p, unsigned t) {
    const Sequence *q = &p->sequence[t];
    put(ed, t);
    put(ed, q->enabled);
    put(ed, q->length);
    put(ed, q->division);
    put(ed, q->direction);
    put(ed, q->arp);
    put(ed, q->arp_octaves);
    for (unsigned i = 0; i < 16; i++)
        put(ed, q->note[i]);
    for (unsigned i = 0; i < 16; i++)
        put(ed, q->gate[i]);
    for (unsigned i = 0; i < 16; i++)
        put(ed, q->velocity[i]);
    for (unsigned i = 0; i < 16; i++)
        word(ed, (unsigned)(q->motion[i] + 8192));
}
static void cv_write(Editor *ed, const Project *p, unsigned i) {
    const CvRoute *c = &p->cv[i];
    put(ed, i);
    const uint8_t v[] = {c->mode,  c->track, c->base_note,  c->scale,     c->root,     c->glide,
                         c->range, c->gate,  c->gate_track, c->parameter, c->gate_note};
    for (unsigned n = 0; n < 11; n++)
        put(ed, v[n]);
    word(ed, (unsigned)(c->amount + 8192));
}
static bool cv_read(Project *p, const uint8_t *d) {
    if (d[0] > 1)
        return false;
    CvRoute *c = &p->cv[d[0]];
    *c = (CvRoute){d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], 0};
    int amount = (int)readword(d + 12) - 8192;
    if (amount < -100 || amount > 100)
        return false;
    c->amount = (int8_t)amount;
    return true;
}
static void conflict_write(Editor *ed, const Engine *e) {
    put(ed, e->conflict.track);
    put(ed, e->conflict.other == 255 ? 127 : e->conflict.other);
    put(ed, e->conflict.reason);
    put(ed, e->conflict.available);
    const char *why = e->conflict.message[0] ? e->conflict.message : "Invalid parameter value";
    const char *alternative =
        e->conflict.suggestion[0] ? e->conflict.suggestion : "Check supported fields and ranges";
    unsigned len = (unsigned)strlen(why);
    put(ed, len);
    for (unsigned i = 0; i < len; i++)
        put(ed, (unsigned)why[i]);
    len = (unsigned)strlen(alternative);
    put(ed, len);
    for (unsigned i = 0; i < len; i++)
        put(ed, (unsigned)alternative[i]);
}
void editor_task(Editor *ed, Engine *e, Settings *s, bool blocked, bool (*save)(void *), void *ctx,
                 uint32_t now) {
    if (ed->receiving && now - ed->last_byte > 2000)
        ed->receiving = false;
    if (!ed->ready || ed->length)
        return;
    ed->ready = false;
    const uint8_t *b = ed->rx;
    unsigned size = ed->count;
    if (size < 7 || b[0] != 0x7d || b[1] != 0x43 || b[2] != 0x54)
        return;
    unsigned id = b[4], cmd = b[5], status = 0, n = size - 7;
    const uint8_t *d = b + 6;
    unsigned sum = 0;
    for (unsigned i = 0; i < size; i++)
        sum += b[i];
    if ((sum & 127) || b[3] != PROTOCOL_VERSION)
        status = 1;
    bool write = cmd == 2 || cmd == 3 || cmd == 5 || cmd == 6 || cmd == 7 || cmd == 9 ||
                 cmd == 11 || cmd == 14 || cmd == 16 || cmd == 18 || cmd == 19 || cmd == 20 ||
                 cmd == 22 || cmd == 24;
    if (!status && write && !ed->negotiated)
        status = 5;
    if (!status && write && blocked)
        status = 3;
    if (!status && write)
        memset(&e->conflict, 0, sizeof(e->conflict));
    ed->length = 0;
    ed->tx[ed->length++] = 0xf0;
    put(ed, 0x7d);
    put(ed, 0x43);
    put(ed, 0x54);
    put(ed, PROTOCOL_VERSION);
    put(ed, id);
    put(ed, cmd | 64);
    put(ed, status);
    static Project candidate;
    if (!status)
        switch (cmd) {
        case 0:
            ed->negotiated = n == 1 && d[0] == PATCH_SCHEMA;
            if (!ed->negotiated) {
                status = 5;
                break;
            }
            put(ed, 0);
            put(ed, 10);
            put(ed, 0);
            put(ed, PROTOCOL_VERSION);
            put(ed, PATCH_SCHEMA);
            put(ed, TRACKS);
            put(ed, USER_PATCHES);
            put(ed, USER_TRACKS);
            put(ed, PROJECTS);
            put(ed, FACTORY_PATCHES);
            put(ed, editor_field_count());
            break;
        case 1:
            if (n != 1 || d[0] >= TRACKS) {
                status = 2;
                break;
            }
            track_write(ed, e, d[0]);
            break;
        case 2:
            candidate = e->project;
            if (!track_read(&candidate, d, n) || !engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            track_write(ed, e, d[0]);
            break;
        case 3:
            if (n != 3 || d[0] > SAVE_SYSTEM ||
                !settings_save(s, e, (SaveKind)d[0], d[1], d[2], save, ctx)) {
                status = 4;
                break;
            }
            break;
        case 4:
            if (n) {
                status = 2;
                break;
            }
            global_write(ed, &e->project);
            break;
        case 5:
            if (n != 26) {
                status = 2;
                break;
            }
            candidate = e->project;
            candidate.sn_reference = d[0];
            candidate.sn_clock = (SnClockConfig){d[1], d[2], d[3], d[4], d[5], d[6]};
            candidate.sn_trigger_source = d[7];
            candidate.clock_source = d[8];
            candidate.clock_policy = d[9];
            candidate.tempo = (uint8_t)readword(d + 10);
            if (readword(d + 10) > 210) {
                status = 2;
                break;
            }
            candidate.midi_thru = d[12];
            memcpy(candidate.mix, d + 13, 4);
            memcpy(candidate.clock_sequence, d + 17, 8);
            candidate.gate_clock = d[25];
            if (!engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            global_write(ed, &e->project);
            break;
        case 6:
            if (n != 1 || d[0] >= PROJECTS || !engine_load_project(e, &s->project[d[0]])) {
                status = 2;
                break;
            }
            ed->changed = true;
            break;
        case 7:
            if (n != 3 || d[0] >= TRACKS || d[1] > 127 || d[2] > 3) {
                status = 2;
                break;
            }
            engine_audition(e, d[0], d[1], d[2]);
            break;
        case 8:
            if (n != 1 || d[0] >= TRACKS) {
                status = 2;
                break;
            }
            sequence_write(ed, &e->project, d[0]);
            break;
        case 9: {
            if (n != 87 || d[0] >= TRACKS) {
                status = 2;
                break;
            }
            candidate = e->project;
            Sequence *q = &candidate.sequence[d[0]];
            q->enabled = d[1];
            q->length = d[2];
            q->division = d[3];
            q->direction = d[4];
            q->arp = d[5];
            q->arp_octaves = d[6];
            memcpy(q->note, d + 7, 16);
            memcpy(q->gate, d + 23, 16);
            memcpy(q->velocity, d + 39, 16);
            for (unsigned i = 0; i < 16; i++) {
                int v = (int)readword(d + 55 + i * 2) - 8192;
                if (v < -100 || v > 100) {
                    status = 2;
                    break;
                }
                q->motion[i] = (int8_t)v;
            }
            if (status || !engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            sequence_write(ed, &e->project, d[0]);
            break;
        }
        case 10:
            if (n != 1 || (d[0] > 1 && d[0] != 127)) {
                status = 2;
                break;
            }
            if (d[0] == 127) {
                cv_write(ed, &e->project, 0);
                cv_write(ed, &e->project, 1);
            } else
                cv_write(ed, &e->project, d[0]);
            break;
        case 11:
            candidate = e->project;
            if ((n != 14 && n != 28) || !cv_read(&candidate, d) ||
                (n == 28 && (d[0] == d[14] || !cv_read(&candidate, d + 14))) ||
                !engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            if (n == 28) {
                cv_write(ed, &e->project, 0);
                cv_write(ed, &e->project, 1);
            } else
                cv_write(ed, &e->project, d[0]);
            break;
        case 12: {
            if (n != 1 || d[0] >= FACTORY_PATCHES) {
                status = 2;
                break;
            }
            Patch p;
            char name[9];
            patch_factory(d[0], &p, name);
            put(ed, d[0]);
            for (unsigned i = 0; i < 8; i++)
                put(ed, (unsigned)name[i]);
            patch_write(ed, &p);
            break;
        }
        case 13:
            if (n != 1 || d[0] >= USER_PATCHES || !(s->patch_used & (1u << d[0]))) {
                status = 2;
                break;
            }
            put(ed, d[0]);
            for (unsigned i = 0; i < 8; i++)
                put(ed, (unsigned)s->patch_name[d[0]][i]);
            patch_write(ed, &s->patches[d[0]]);
            break;
        case 14:
            if (n != 2 || d[0] >= TRACKS || d[1] >= USER_PATCHES ||
                !(s->patch_used & (1u << d[1])) ||
                !engine_load_patch(e, d[0], &s->patches[d[1]], s->patch_name[d[1]])) {
                status = 2;
                break;
            }
            ed->changed = true;
            track_write(ed, e, d[0]);
            break;
        case 15:
            if (n) {
                status = 2;
                break;
            }
            for (unsigned i = 0; i < MOD_SLOTS; i++) {
                put(ed, e->project.mod[i].source);
                put(ed, e->project.mod[i].track);
                put(ed, e->project.mod[i].parameter);
                word(ed, (unsigned)(e->project.mod[i].amount + 8192));
            }
            break;
        case 16:
            if (n != MOD_SLOTS * 5) {
                status = 2;
                break;
            }
            candidate = e->project;
            for (unsigned i = 0; i < MOD_SLOTS; i++) {
                const uint8_t *m = d + i * 5;
                int amount = (int)readword(m + 3) - 8192;
                if (amount < -100 || amount > 100) {
                    status = 2;
                    break;
                }
                candidate.mod[i] = (ModRoute){m[0], m[1], m[2], (int8_t)amount};
            }
            if (status || !engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            break;
        case 17:
            if (n) {
                status = 2;
                break;
            }
            put(ed, e->system.brightness);
            break;
        case 18:
            if (n != 1 || d[0] < 10 || d[0] > 100) {
                status = 2;
                break;
            }
            e->system.brightness = d[0];
            ed->changed = true;
            break;
        case 19:
            if (n) {
                status = 2;
                break;
            }
            engine_panic(e, -1);
            break;
        case 20:
            if (n != 2 || d[0] >= TRACKS || d[1] >= USER_TRACKS ||
                !(s->track_used & (1u << d[1]))) {
                status = 2;
                break;
            }
            candidate = e->project;
            candidate.track[d[0]] = s->tracks[d[1]];
            if (!engine_apply(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            track_write(ed, e, d[0]);
            break;
        case 21:
            if (n) {
                status = 2;
                break;
            }
            put(ed, engine_active(e));
            wide(ed, e->steals);
            wide(ed, e->refused);
            wide(ed, e->bus.writes);
            wide(ed, e->bus.coalesced);
            put(ed, bus_pending(&e->bus));
            put(ed, e->bus.max_pending);
            wide(ed, e->tick_max_us);
            wide(ed, e->tick_overruns);
            break;
        case 22:
            if (n != 1 || d[0] > 2) {
                status = 2;
                break;
            }
            project_default(&candidate);
            if (d[0] == 1) {
                for (unsigned t = 1; t < 6; t++)
                    candidate.track[t].enabled = 0;
                candidate.track[0].mode = PLAY_POLY;
                candidate.track[0].voices = 6;
            }
            if (d[0] == 2) {
                for (unsigned t = 2; t < 6; t++)
                    candidate.track[t].enabled = 0;
                patch_factory(1, &candidate.track[1].patch, candidate.track[1].name);
                candidate.track[1].mode = PLAY_POLY;
                candidate.track[1].voices = 2;
                candidate.track[0].mode = PLAY_POLY;
                candidate.track[0].voices = 2;
            }
            if (!engine_load_project(e, &candidate)) {
                status = 2;
                break;
            }
            ed->changed = true;
            break;
        case 24:
            if (n != 1 || d[0] >= TRACKS || !engine_poly_focus(e, d[0])) {
                status = 2;
                break;
            }
            ed->changed = true;
            track_write(ed, e, d[0]);
            break;
        default:
            status = 1;
            break;
        }
    if (status) {
        ed->length = 8;
        ed->tx[7] = (uint8_t)status;
        if (status == 2)
            conflict_write(ed, e);
    }
    unsigned checksum = 0;
    for (unsigned i = 1; i < ed->length; i++)
        checksum += ed->tx[i];
    put(ed, (0u - checksum) & 127u);
    if (ed->length < EDITOR_BYTES)
        ed->tx[ed->length++] = 0xf7;
    ed->sent = 0;
}
bool editor_packet(Editor *e, uint8_t out[4]) {
    if (e->sent >= e->length)
        return false;
    unsigned remain = e->length - e->sent, n = remain > 3 ? 3 : remain;
    memset(out, 0, 4);
    out[0] = (uint8_t)(remain > 3 ? 4 : 4 + n);
    memcpy(out + 1, e->tx + e->sent, n);
    e->sent += n;
    if (e->sent == e->length)
        e->sent = e->length = 0;
    return true;
}
