#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Prefer the audio tick's idle window, but never starve a background device.
   All timestamps are microseconds and deliberately use wrap-safe subtraction. */
static inline bool service_ready(uint32_t now, uint32_t tick, uint32_t last, uint32_t idle_window,
                                 uint32_t max_wait) {
    return now - tick < idle_window || now - last >= max_wait;
}
