#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sn_clock.h"
#include "studio_types.h"
#define VOICES 14
#define PATCHES 16
#define PRESETS 8
#define SOURCES 2
#define SETTINGS_VERSION 5
#define LAYOUT_REVISION 1
#define CV_SCALES 8
#define CV_ROLES 7
#define ARP_MODES 8
#define UI_GROUPS 12 /* legacy regression fixture; sound-first UI uses its own pages */

typedef enum {
    TARGET_AUTO,
    TARGET_SAA,
    TARGET_SN1,
    TARGET_SN2,
    TARGET_LAYER,
    TARGET_SN_PAIR,
    TARGET_FULL,
    TARGET_OFF,
    TARGET_COUNT
} Target;
typedef struct {
    uint8_t channel, target, program, forward;
} Route; /* channel 0=omni, 1..16 */
typedef struct {
    uint8_t enabled, voice, program, base_note;
} CvConfig;
typedef struct {
    uint8_t target, program;
} MidiPart;
typedef struct {
    uint8_t transpose, low, high, velocity; /* transpose 0..48 => -24..+24 */
} PartPerformance;
typedef struct {
    uint8_t attack, decay, release, sustain, tremolo, rate;
} ToneControl; /* times: 64=original; sustain 127=patch; rate index 0..7 */
typedef enum {
    CV_PITCH,
    CV_SN_BASE,
    CV_SN_DEPTH,
    CV_SN_WIDTH,
    CV_MASTER,
    CV_SAA_PAN,
    CV_TREMOLO
} CvRole;
typedef struct {
    uint8_t mix[4]; /* master, SAA, SN1, SN2: 0..100 */
    Route route[SOURCES];
    CvConfig cv[2];
    uint8_t reserve_cv, arp_mode, arp_rate,
        clock_source; /* arp_mode 0=off/1=major/2=minor/3=octave */
    SnClockConfig sn_clock;
    uint8_t channel_map[SOURCES];
    MidiPart part[SOURCES][16];
    uint8_t cv_pair[2], sn_reference, sn_width, sn_trigger_source, arp_target;
    PartPerformance performance[SOURCES][16];
    ToneControl tone[2]; /* SAA, SN; affects the software envelope only */
    uint8_t cv_scale[2], cv_root[2], cv_glide[2], cv_role[2], cv_range[2];
    uint8_t tempo, clock_policy, clock_sequence[8]; /* tempo 0..210 => 30..240 BPM */
    StScene studio;
} Preset;
typedef struct {
    uint32_t version;
    Preset preset[PRESETS];
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t selected, layout_revision, padding[2];
    StSound sounds[ST_SOUNDS];
    char sound_name[ST_SOUNDS][9];
    uint16_t sound_used;
    uint8_t brightness;
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
    uint8_t source, channel, note, velocity, program, pair, hybrid, pressure;
    uint16_t token;
    uint32_t age, elapsed, release_elapsed;
    float pitch, level, release_level;
    uint8_t part;
} Voice;
typedef struct {
    uint8_t volume, expression, pan, modulation, sustain, program, bend_range;
    uint8_t bend_cents, rpn_msb, rpn_lsb, pressure, attack, decay, release, envelope_sustain;
    int16_t bend;
} Channel;
typedef struct {
    void (*saa)(void *, uint8_t, uint8_t);
    void (*sn)(void *, uint8_t, uint8_t);
    void *ctx;
} SoundSink;
typedef struct Engine {
    Preset preset;
    Voice voice[VOICES];
    Channel channel[SOURCES][16];
    uint16_t pressed[SOURCES][16][128], released[SOURCES][16][128];
    uint8_t saa_cache[32];
    uint16_t sn_div[2][3];
    uint8_t sn_atten[2][4], sn_noise[2];
    uint32_t now, age, steals, refused, pitch_clamps, clocks, last_clock;
    float midi_hz;
    uint32_t sn_note_trigger, sn_manual_trigger, sn_frequency_updated;
    uint32_t internal_clocks, internal_phase;
    uint8_t sequence_step;
    float cv_target[2], cv_mod[2], cv_quantized[2];
    bool cv_target_valid[2], cv_mod_valid[2], cv_quantized_valid[2], cv_control_gate[2];
    bool clock_seen, clock_running;
    SoundSink sink;
    StRuntime studio;
    const Settings *library;
} Engine;
void settings_default(Settings *s);
void settings_reset_layout(Settings *s);
bool settings_valid(const Settings *s);
void engine_init(Engine *e, const Preset *p, SoundSink sink);
void engine_panic(Engine *e, int source); /* -1 all; 0 USB; 1 TRS */
void engine_preset(Engine *e, const Preset *p);
void engine_message(Engine *e, uint8_t source, const uint8_t *msg, uint8_t len);
void engine_tick(Engine *e, uint32_t elapsed_ms);
void engine_cv(Engine *e, unsigned input, float note, bool gate);
void engine_cv_pitch(Engine *e, unsigned input, float note);
uint16_t sn_divider(float hz, bool *clamped);
uint16_t sn_divider_clock(float hz, uint32_t clock, bool *clamped);
uint32_t engine_sn_reference(const Preset *p);
SnClockConfig engine_clock_config(const Preset *p);
uint16_t engine_cv_mask(const Preset *p, unsigned input, bool resources);
bool engine_cv_config_valid(const Preset *p);
bool engine_cv_patch_ok(int voice, int program);
bool engine_target_patch_ok(int target, int program);
void engine_routes_reset(Engine *e, unsigned source);
void saa_frequency(float hz, uint8_t *octave, uint8_t *frequency, bool *clamped);
uint8_t sn_attenuation(float gain);
float sn_noise_clock_hz(float note_hz, bool white);
unsigned engine_active(const Engine *e);
unsigned engine_active_notes(const Engine *e);

/* Runtime modulation never changes the saved preset. */
SnClockConfig engine_effective_clock(const Engine *e);
uint32_t engine_transport_clocks(const Engine *e);
void engine_cv_control(Engine *e, unsigned input, float volts, bool gate);
void engine_cv_clear(Engine *e, unsigned input);
float engine_quantize(float note, unsigned scale, unsigned root);
unsigned engine_time_scale(unsigned value); /* Q8 multiplier, 64=256 */
void preset_performance_default(Preset *p); /* appended v4 fields only */

void engine_transport_reset(Engine *e);
