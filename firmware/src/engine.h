#pragma once
#include <stdbool.h>
#include <stdint.h>
#define VOICES 14
#define PATCHES 16
#define PRESETS 8
#define SOURCES 2
#define SETTINGS_VERSION 1

typedef enum { TARGET_AUTO, TARGET_SAA, TARGET_SN1, TARGET_SN2, TARGET_LAYER, TARGET_COUNT } Target;
typedef struct {
    uint8_t channel, target, program, forward;
} Route; /* channel 0=omni, 1..16 */
typedef struct {
    uint8_t enabled, voice, program, base_note;
} CvConfig;
typedef struct {
    uint8_t mix[4]; /* master, SAA, SN1, SN2: 0..100 */
    Route route[SOURCES];
    CvConfig cv[2];
    uint8_t reserve_cv, arp_mode, arp_rate,
        clock_source; /* arp_mode 0=off/1=major/2=minor/3=octave */
} Preset;
typedef struct {
    uint32_t version;
    Preset preset[PRESETS];
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t selected, padding[3];
} Settings;
typedef struct {
    const char *name;
    uint16_t attack, decay, release;
    uint8_t sustain, noise, noise_rate,
        hw_env;   /* noise 0=tone,1=noise,2=tone+noise; hw_env shape or 0 */
    int8_t sweep; /* initial pitch offset, decays over decay time */
    uint8_t vibrato, pan_motion;
} Patch;
extern const Patch patches[PATCHES];
typedef struct {
    bool active, held, sustained, releasing;
    uint8_t source, channel, note, velocity, program;
    uint16_t token;
    uint32_t age, elapsed, release_elapsed;
    float pitch, level, release_level;
} Voice;
typedef struct {
    uint8_t volume, expression, pan, modulation, sustain, program, bend_range;
    int16_t bend;
} Channel;
typedef struct {
    void (*saa)(void *, uint8_t, uint8_t);
    void (*sn)(void *, uint8_t, uint8_t);
    void *ctx;
} SoundSink;
typedef struct {
    Preset preset;
    Voice voice[VOICES];
    Channel channel[SOURCES][16];
    uint16_t pressed[SOURCES][16][128], released[SOURCES][16][128];
    uint8_t saa_cache[32];
    uint16_t sn_div[2][3];
    uint8_t sn_atten[2][4], sn_noise[2];
    uint32_t now, age, steals, refused, pitch_clamps, clocks, last_clock;
    float midi_hz;
    bool clock_seen, clock_running;
    SoundSink sink;
} Engine;
void settings_default(Settings *s);
bool settings_valid(const Settings *s);
void engine_init(Engine *e, const Preset *p, SoundSink sink);
void engine_panic(Engine *e, int source); /* -1 all; 0 USB; 1 TRS */
void engine_preset(Engine *e, const Preset *p);
void engine_message(Engine *e, uint8_t source, const uint8_t *msg, uint8_t len);
void engine_tick(Engine *e, uint32_t elapsed_ms);
void engine_cv(Engine *e, unsigned input, float note, bool gate);
void engine_cv_pitch(Engine *e, unsigned input, float note);
uint16_t sn_divider(float hz, bool *clamped);
void saa_frequency(float hz, uint8_t *octave, uint8_t *frequency, bool *clamped);
uint8_t sn_attenuation(float gain);
unsigned engine_active(const Engine *e);
