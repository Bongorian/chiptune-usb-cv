#pragma once
#include "model.h"
#include "sound_math.h"
#include "register_bus.h"
#include "resources.h"
typedef struct {
    bool active, held, sustained, releasing;
    uint8_t source, channel, note, velocity, track, slot, pressure;
    uint32_t token, age, elapsed, release_elapsed;
    float pitch, target, level, release_level;
} Voice;
typedef struct {
    uint8_t volume, expression, pan, modulation, sustain, bend_range, bend_cents;
    uint8_t rpn_msb, rpn_lsb, pressure;
    int16_t bend;
} Channel;
typedef struct {
    uint32_t token, age;
    uint8_t source, channel, note, velocity;
    bool sustained;
} HeldNote;
typedef struct {
    uint32_t phase, pulse, due;
    uint8_t position, arp_position;
    int8_t lane;
    float random_lfo;
} SequenceRuntime;
typedef struct Engine {
    Project project;
    System system; /* live calibration/brightness; persisted only by SAVE_SYSTEM */
    ResourcePlan plan;
    ResourceConflict conflict;
    RegisterBus bus;
    Voice voice[PHYSICAL_RESOURCES];
    Channel channel[SOURCES][16];
    uint32_t pressed[SOURCES][16][128], released[SOURCES][16][128];
    HeldNote held[TRACKS][HELD_NOTES];
    SequenceRuntime motion[TRACKS];
    Patch effective[TRACKS];
    float pitch_mod[TRACKS], pan_mod[TRACKS], lfo[TRACKS];
    int8_t macro[TRACKS][4];
    float cv[2], cv_note[2];
    bool cv_valid[2], gate[2], gate_block[2];
    bool running, clock_seen, clock_running, test_on;
    uint8_t test_track, test_note, test_mode;
    uint32_t now, age, steals, refused, pitch_clamps, random;
    uint32_t tick_max_us, tick_overruns;
    uint32_t clocks, internal_clocks, internal_phase, last_clock;
    uint32_t sn_note_trigger, sn_manual_trigger;
    uint32_t test_due, test_off;
    float clock_base, clock_depth;
    uint8_t midi_channel[SOURCES];
    uint32_t midi_time[SOURCES];
    bool midi_seen[SOURCES];
    const Settings *library;
} Engine;
void engine_init(Engine *e, const Project *p, SoundSink sink);
void engine_reset_audio(Engine *e, SoundSink sink);
bool engine_apply(Engine *e, const Project *p);
bool engine_load_patch(Engine *e, unsigned track, const Patch *p, const char name[9]);
void engine_panic(Engine *e, int source);
void engine_stop_track(Engine *e, unsigned track);
void engine_message(Engine *e, uint8_t source, const uint8_t *msg, uint8_t len);
void engine_tick(Engine *e, uint32_t elapsed_ms);
void engine_cv(Engine *e, unsigned input, float volts, bool gate, bool fresh);
void engine_audition(Engine *e, unsigned track, unsigned note, unsigned mode);
unsigned engine_active(const Engine *e);
unsigned engine_track_notes(const Engine *e, unsigned track);
uint32_t engine_sn_reference(const Project *p);
SnClockConfig engine_effective_clock(const Engine *e);
uint32_t engine_transport_clocks(const Engine *e);
float engine_quantize(float note, unsigned scale, unsigned root);
void engine_note(Engine *e, unsigned track, unsigned source, unsigned channel, unsigned note,
                 unsigned velocity, uint32_t token, float pitch);
void engine_release_source(Engine *e, unsigned track, unsigned source);
void sequence_tick(Engine *e, uint32_t dt);
void sequence_run(Engine *e, bool run, bool restart);
void sequence_gate(Engine *e, unsigned track);
uint16_t sn_divider_clock(float hz, uint32_t clock, bool *clamped);
uint8_t sn_attenuation(float gain);

void saa_note_register(float note, uint8_t *oct, uint8_t *reg, bool *clamped);

void engine_bind_library(Engine *e, const Settings *s);
/* Explicit user action: silence/disable other tracks on the same chip, then
   reserve the selected patch's largest feasible polyphony. Other chip stays live. */
bool engine_poly_focus(Engine *e, unsigned track);
