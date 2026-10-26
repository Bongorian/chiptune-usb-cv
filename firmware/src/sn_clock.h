#pragma once
#include <stdbool.h>
#include <stdint.h>
/* RP2040 PIO: nine instructions, half-wave X+5 SM cycles, 16.8 divider.
   Limits describe the generator, not a measured operating range of the PSG. */
#define SN_CLOCK_SYS_HZ 128000000u
#define SN_CLOCK_TICK_US 250u
#define SN_CLOCK_DIV_MIN 256u
#define SN_CLOCK_DIV_MAX 16777216u
#define SN_CLOCK_MODES 19u
#define SN_CLOCK_SLOTS 2u
#define SN_CLOCK_TRANSFER_CYCLES 40u /* datasheet ~32; 8-cycle phase/timing margin */
typedef enum {
    SN_CLOCK_OFF,
    SN_CLOCK_MANUAL,
    SN_CLOCK_SWEEP,
    SN_CLOCK_TRIANGLE,
    SN_CLOCK_SQUARE,
    SN_CLOCK_RANDOM,
    SN_CLOCK_SEQUENCE,
    SN_CLOCK_DIVE,
    SN_CLOCK_SINE,
    SN_CLOCK_SAW,
    SN_CLOCK_WALK,
    SN_CLOCK_SLEW,
    SN_CLOCK_WOW,
    SN_CLOCK_FLUTTER,
    SN_CLOCK_CURVE,
    SN_CLOCK_RATCHET,
    SN_CLOCK_BURST,
    SN_CLOCK_AUDIO,
    SN_CLOCK_SAW_DOWN
} SnClockMode;
typedef enum { SN_FOLLOW_RAW, SN_FOLLOW_BASE, SN_FOLLOW_FULL } SnFollow;
typedef enum { SN_DOMAIN_MUSIC, SN_DOMAIN_HZ, SN_DOMAIN_RATIO, SN_DOMAIN_PERIOD } SnDomain;
typedef enum { SN_VALID, SN_DEGRADED, SN_UNAVAILABLE, SN_STOPPED } SnClockState;
typedef struct {
    int32_t offset, depth; /* cents / Hz / ratio ppm / PIO half-period cycles */
    uint32_t rate_millihz;
    uint8_t mode, domain, sync, retrigger, curve, reserved[3];
} SnModConfig;
typedef struct {
    double base_hz;
    uint32_t period, div256; /* direct: X, and SM clock divider *256 */
    uint8_t direct, stopped, follow, reserved;
    SnModConfig mod[SN_CLOCK_SLOTS];
} SnClockConfig;
typedef struct {
    uint32_t period, div256;
    double hz;
    bool available, experimental;
} SnClockPlan;
typedef struct {
    uint32_t phase, random, sweep, sync_bias;
    int32_t held, smooth, walk;
} SnMod;
typedef struct {
    SnClockConfig config;
    SnClockPlan base_plan;
    SnMod mod[SN_CLOCK_SLOTS];
    uint32_t clocks, last_midi_us, midi_interval_us;
    uint16_t sequence[8];
    bool midi_seen;
} SnClock;
extern const uint8_t sn_clock_sync_clocks[9];
double sn_clock_min_hz(void);
double sn_clock_max_hz(void);
SnClockPlan sn_clock_plan(double hz);
SnClockPlan sn_clock_direct(uint32_t period, uint32_t div256);
SnClockPlan sn_clock_base(const SnClockConfig *c);
void sn_clock_default(SnClockConfig *c);
bool sn_clock_valid(const SnClockConfig *c);
void sn_clock_init(SnClock *s, const SnClockConfig *c);
void sn_clock_configure(SnClock *s, const SnClockConfig *c);
void sn_clock_trigger(SnClock *s, bool manual, uint32_t now_us);
void sn_clock_midi(SnClock *s, uint32_t clocks, uint32_t now_us);
SnClockPlan sn_clock_tick(SnClock *s, uint32_t elapsed_us, uint32_t now_us);
SnClockPlan sn_clock_sample(const SnClock *s);
void sn_clock_sequence(SnClock *s, const uint8_t percent[8]);
/* Conservative completed-clock budget between hardware reconfigurations.
   One whole period is withheld for unknown edge phase. No timeouts or clamping. */
typedef struct {
    SnClockPlan plan;
    uint64_t epoch_us, cycles, epoch_cycles;
    uint32_t restarts;
    bool stopped;
} SnClockCounter;
void sn_counter_init(SnClockCounter *c, SnClockPlan p, uint64_t now);
void sn_counter_advance(SnClockCounter *c, uint64_t now);
void sn_counter_change(SnClockCounter *c, SnClockPlan p, uint64_t now, bool stopped);
