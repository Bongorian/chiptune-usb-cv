#pragma once
#include <stdbool.h>
#include <stdint.h>
#define ST_PARTS 14
#define ST_STEPS 16
#define ST_MODS 8
#define ST_SOUNDS 16
#define ST_FACTORIES 23
#define ST_HELD 16
typedef enum {
    ST_SAA_TONE,
    ST_SAA_NOISE,
    ST_SAA_MIX,
    ST_SAA_ENV,
    ST_SAA_AM,
    ST_SN_TWIN,
    ST_SN_LEFT,
    ST_SN_RIGHT,
    ST_SN_NOISE,
    ST_SN_METAL,
    ST_SN_NOISE_L,
    ST_SN_NOISE_R,
    ST_SAA_ENV_NOISE,
    ST_SAA_ENV_MIX,
    ST_SAA_LAYER,
    ST_SN_STACK,
    ST_MODELS
} StModel;
typedef enum {
    ST_NONE,
    ST_WAVE,
    ST_STEPSIZE,
    ST_INVERT,
    ST_PAN,
    ST_VIBRATO,
    ST_DETUNE,
    ST_NOISE_MIX,
    ST_ATTACK,
    ST_DECAY,
    ST_RELEASE,
    ST_TREMOLO,
    ST_LEVEL,
    ST_PITCH,
    ST_CLOCK_BASE,
    ST_CLOCK_DEPTH,
    ST_LAYER_LEVEL,
    ST_LAYER_DECAY,
    ST_ENV_RATIO,
    ST_LAYER_INTERVAL,
    ST_PARAMS
} StParam;
typedef struct {
    uint16_t attack, decay, release;
    uint8_t model, shape, steps, invert, retrigger, noise_rate, noise_mix;
    uint8_t sustain, level, pan, detune, vibrato, tremolo, lfo_rate, glide, mode;
    int8_t transpose, fine, sweep;
    uint8_t macro_dest[4];
    int8_t macro[4];      /* bipolar additions; zero leaves the base sound unchanged */
    uint16_t layer_decay; /* Additional exponential decay of the layer, ms; 0 = sustained. */
    int8_t layer_interval, env_ratio, stack_interval; /* semitones */
    uint8_t layer_level, layer_source, stack_size;    /* source: tone/noise/mix; stack: 2 or 3 */
} StSound;
typedef enum {
    ST_ROLE_ANY,
    ST_ROLE_SAA_TONE,
    ST_ROLE_SAA_ENV,
    ST_ROLE_SN_TONE,
    ST_ROLE_SN_NOISE,
    ST_ROLES
} StRole;
typedef struct {
    StSound sound;
    char name[9];
    uint8_t enabled, poly, low, high;
    uint8_t note[ST_STEPS], gate[ST_STEPS], velocity[ST_STEPS];
    int8_t motion[ST_STEPS];
    uint8_t length, division, direction, arp, arp_octaves, lfo_shape, sequence, velocity_curve;
    uint8_t role; /* Uses the former trailing padding byte; v5 storage size stays unchanged. */
} StPart;
typedef struct {
    uint8_t mode, part, base_note, scale, root, glide, range;
    uint8_t gate, gate_part, parameter, gate_note; /* CV and gate have independent destinations */
    int8_t amount;
} StCv;
typedef struct {
    uint8_t source, part, parameter;
    int8_t amount;
} StMod;
typedef struct {
    char name[9];
    uint8_t enabled, gate_clock;
    StPart part[ST_PARTS];
    uint16_t route[2][16];
    StCv cv[2];
    StMod mod[ST_MODS];
} StScene;
typedef struct {
    uint16_t token;
    uint8_t source, channel, note, velocity;
    bool sustained;
    uint32_t age;
} StHeld;
typedef struct {
    uint16_t outputs[ST_PARTS], resources[ST_PARTS];
    StHeld held[ST_PARTS][ST_HELD];
    float cv[2], cv_note[2], pitch_target[14];
    float clock_base, clock_depth, phase[ST_PARTS];
    bool cv_valid[2], gate[2], gate_block[2], test_on, running;
    uint8_t test_part, test_note, test_mode, position[ST_PARTS], arp_position[ST_PARTS];
    uint8_t lane_value[ST_PARTS], mute[ST_PARTS];
    uint32_t pulse[ST_PARTS], due[ST_PARTS], test_due, test_off, last_transport;
    uint16_t serial;
    uint8_t env_value[2];
    uint32_t random;
    float lfo_sample[ST_PARTS], lfo_value[ST_PARTS];
    StSound effective[ST_PARTS];
    uint32_t midi_time[2];
    uint8_t midi_channel[2];
    bool midi_seen[2];
} StRuntime;
