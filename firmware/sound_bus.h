#pragma once
#include <stdint.h>

// Board nets D0..D7 are GPIO7..GPIO0, respectively (see pinmap.h).
// A bus byte below uses bit0=board D0; write its bits via PIN_D0..PIN_D7.
// For gpio_put_masked(0xff, word), use *_gpio_word(), which is the logical byte.
// SAA1099 physical D0(pin10)..D7(pin17) connect to board D7..D0.
// SN76489AN physical D0(pin3) is its most significant data bit.
static inline uint8_t chiptune_reverse8(uint8_t value) {
    value = (uint8_t)(((value & 0x55u) << 1) | ((value >> 1) & 0x55u));
    value = (uint8_t)(((value & 0x33u) << 2) | ((value >> 2) & 0x33u));
    return (uint8_t)((value << 4) | (value >> 4));
}
static inline uint8_t chiptune_saa_bus_byte(uint8_t value) {
    return chiptune_reverse8(value);
}
static inline uint8_t chiptune_sn_bus_byte(uint8_t value) {
    return chiptune_reverse8(value);
}
static inline uint8_t chiptune_saa_gpio_word(uint8_t value) { return value; }
static inline uint8_t chiptune_sn_gpio_word(uint8_t value) { return value; }
// Keep SN write intervals >=10 us with its 4 MHz clock.
// Keep the data/address bus high impedance until SOUND_ENABLE is asserted.
