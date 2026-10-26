#pragma once
#include <stdbool.h>
#include <stdint.h>
#define TRACKS 14
#define PHYSICAL_RESOURCES 14
#define MAX_POLYPHONY 6
#define USER_PATCHES 16
#define USER_TRACKS 16
#define PROJECTS 4
#define FACTORY_PATCHES 23
#define STEPS 16
#define MOD_SLOTS 8
#define HELD_NOTES 16
#define SOURCES 2
#define CV_SCALES 8
#define SETTINGS_VERSION 10u
#define PROTOCOL_VERSION 2u
#define PATCH_SCHEMA 10u

typedef enum { CHIP_SAA, CHIP_SN } Chip;
typedef enum { SOURCE_TONE, SOURCE_NOISE, SOURCE_MIX } Source;
typedef enum { ENV_OFF, ENV_OSC, ENV_AM } HardwareEnvelope;
typedef enum { OUTPUT_STEREO, OUTPUT_LEFT, OUTPUT_RIGHT } Output;
/* Musical selectors derive from Patch; no duplicate waveform field is stored. */
typedef enum {
    WAVE_SQUARE,
    WAVE_PULSE,
    WAVE_ENV,
    WAVE_TONE_NOISE,
    WAVE_NOISE,
    WAVE_METAL,
    WAVE_AM,
    WAVE_COUNT
} PatchWave;
typedef enum {
    PATCH_F_PITCH = 1u << 0,
    PATCH_F_PAN = 1u << 1,
    PATCH_F_ENV = 1u << 2,
    PATCH_F_ENV_RATIO = 1u << 3,
    PATCH_F_NOISE = 1u << 4,
    PATCH_F_NOISE_TYPE = 1u << 5,
    PATCH_F_NOISE_TRACK = 1u << 6,
    PATCH_F_NOISE_MIX = 1u << 7,
    PATCH_F_LAYERS = 1u << 8,
    PATCH_F_LAYER_PITCH = 1u << 9,
    PATCH_F_STACK = 1u << 10,
    PATCH_F_SAA_LAYER = 1u << 11,
    PATCH_F_DETUNE = 1u << 12,
    PATCH_F_PULSE = 1u << 13
} PatchFeature;
typedef enum { PLAY_DIRECT, PLAY_POLY, PLAY_LEGATO } PlayMode;
typedef enum {
    MOD_NONE,
    MOD_LEVEL,
    MOD_PAN,
    MOD_PITCH,
    MOD_VIBRATO,
    MOD_TREMOLO,
    MOD_LAYER,
    MOD_ENV_RATIO,
    MOD_NOISE_PITCH,
    MOD_CLOCK_BASE,
    MOD_CLOCK_DEPTH,
    MOD_MORPH,
    MOD_PARAMS
} ModParameter;
typedef struct {
    uint16_t attack, decay, release, pitch_decay, glide, layer_decay;
    uint8_t chip, source, env, output, layers, layer_source;
    uint8_t shape, env_steps, env_invert, retrigger, noise_rate, noise_type;
    uint8_t sustain, level, noise_mix, vibrato, tremolo, detune, lfo_rate, lfo_shape, layer_level;
    int8_t transpose, fine, pitch_amount, env_ratio, layer_interval, stack_interval;
    int8_t noise_pitch, noise_sweep;
} Patch;
typedef struct {
    Patch patch;
    char name[9];
    uint8_t enabled, mode, voices, level, pan, mute, low, high, velocity_curve;
    uint8_t morph_target, morph; /* target 0=off, 1..16=user patch; continuous fields only */
    uint8_t macro_dest[4];
    int8_t macro[4];
} Track;
typedef struct {
    uint8_t enabled, length, division, direction, arp, arp_octaves;
    uint8_t note[STEPS], gate[STEPS], velocity[STEPS];
    int8_t motion[STEPS];
} Sequence;
typedef struct {
    uint8_t mode, track, base_note, scale, root, glide, range;
    uint8_t gate, gate_track, parameter, gate_note;
    int8_t amount;
} CvRoute;
typedef struct {
    uint8_t source, track, parameter;
    int8_t amount;
} ModRoute;
#include "sn_clock.h"
typedef struct {
    char name[9];
    Track track[TRACKS];
    Sequence sequence[TRACKS];
    uint16_t route[SOURCES][16];
    CvRoute cv[2];
    ModRoute mod[MOD_SLOTS];
    SnClockConfig sn_clock;
    uint8_t mix[4], sn_reference, sn_trigger_source;
    uint8_t clock_source, clock_policy, gate_clock, tempo, midi_thru;
    uint8_t clock_sequence[8];
} Project;
typedef struct {
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t brightness;
} System;
typedef struct {
    uint32_t version;
    System system;
    Patch patches[USER_PATCHES];
    char patch_name[USER_PATCHES][9];
    Track tracks[USER_TRACKS];
    Project project[PROJECTS];
    uint16_t patch_used, track_used;
    uint8_t selected;
} Settings;
void patch_default(Patch *p, unsigned chip);
void patch_factory(unsigned index, Patch *p, char name[9]);
bool patch_valid(const Patch *p);
PatchWave patch_wave(const Patch *p);
bool patch_wave_available(unsigned chip, unsigned wave);
bool patch_set_wave(Patch *p, unsigned wave);
uint32_t patch_features(const Patch *p);
bool patch_same_structure(const Patch *a, const Patch *b);
void patch_morph(Patch *out, const Patch *a, const Patch *b, unsigned amount);
void project_default(Project *p);
void settings_default(Settings *s);
bool project_valid(const Project *p);
bool settings_valid(const Settings *s);
bool name_valid(const char name[9]);
