#pragma once
#include <stdbool.h>
#include <stdint.h>
/* The PCB shares this external clock between both SN76489ANs. */
#define SN_CLOCK_TICK_US 250u
#define SN_CLOCK_MODES 8u
#define SN_CLOCK_RATES 16u
#define SN_CLOCK_MAX_DROP 48u
#define SN_CLOCK_SYS_HZ 128000000u
#define SN_CLOCK_MIN_HALF 16u
#define SN_CLOCK_MAX_HALF 256u
typedef enum {
    SN_CLOCK_OFF,
    SN_CLOCK_MANUAL,
    SN_CLOCK_SWEEP,
    SN_CLOCK_TRIANGLE,
    SN_CLOCK_SQUARE,
    SN_CLOCK_RANDOM,
    SN_CLOCK_SEQUENCE,
    SN_CLOCK_DIVE
} SnClockMode;
typedef struct {
    uint8_t mode, base, depth, rate, sync, retrigger;
} SnClockConfig; /* base/depth: downward semitones; sync 0=free, 1..8=MIDI division */
typedef struct {
    SnClockConfig config;
    uint32_t phase, random, clocks, sweep;
    uint16_t held;
    uint8_t sync_step;
} SnClock;
extern const uint32_t sn_clock_rate_millihz[SN_CLOCK_RATES];
extern const uint8_t sn_clock_sync_clocks[9];
bool sn_clock_valid(const SnClockConfig *c);
void sn_clock_init(SnClock *s, const SnClockConfig *c);
void sn_clock_configure(SnClock *s, const SnClockConfig *c);
void sn_clock_trigger(SnClock *s);
void sn_clock_midi(SnClock *s, uint32_t clocks);
uint16_t sn_clock_tick(SnClock *s); /* integer-only, fixed 250us step */
uint16_t sn_clock_sample(const SnClock *s);
uint16_t sn_clock_half(uint16_t drop_q8);
uint32_t sn_clock_hz(uint16_t half_cycles);
unsigned sn_clock_write_us(const SnClockConfig *c);
