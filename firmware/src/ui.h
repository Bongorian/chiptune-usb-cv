#pragma once
#include "engine.h"
typedef struct {
    bool power, usb, suspended, adc_ok;
    uint32_t uart_overflow, usb_dropped, tick_us, tick_overruns;
    int16_t cv_raw[2];
    bool gates[2];
} UiStatus;
typedef struct {
    Settings *settings;
    Engine *engine;
    bool (*save)(void *);
    bool (*calibrate)(void *, unsigned, bool);
    void *ctx;
    uint8_t frame[1024];
    uint8_t group, field;
    bool menu, edit;
    bool raw[3], stable[3], long_sent[3];
    uint32_t changed[3], pressed[3], repeat[3], toast_until;
    char toast[22];
} Ui;
void ui_init(Ui *u, Settings *s, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx);
void ui_buttons(Ui *u, const bool down[3], uint32_t now);
void ui_render(Ui *u, const UiStatus *status, uint32_t now);
