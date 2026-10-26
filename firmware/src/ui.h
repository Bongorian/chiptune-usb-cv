#pragma once
#include "engine.h"
#include "persistence.h"
typedef struct {
    bool power, usb, suspended, adc_ok;
    uint32_t uart_overflow, usb_dropped, tick_us, tick_overruns;
    double clock_hz;
    int16_t cv_raw[2];
    bool gates[2];
} UiStatus;
typedef enum {
    UI_HOME,
    UI_MENU,
    UI_QUICK,
    UI_ADVANCED,
    UI_CLOCK,
    UI_SEQUENCE,
    UI_MOD,
    UI_CV,
    UI_SAVE,
    UI_SYSTEM,
    UI_NAME,
    UI_CONFLICT,
    UI_CLOCK_ADVANCED,
    UI_PAGES
} UiPage;
typedef struct {
    Settings *settings;
    Engine *engine;
    bool (*save)(void *);
    bool (*calibrate)(void *, unsigned, bool);
    void *ctx;
    uint8_t frame[1024];
    uint8_t page, track, field, previous_page, slot, save_kind, factory, step, mod, input,
        macro, name_cursor, wave, clock_slot;
    uint8_t path[4], cursor[4], depth;
    bool edit, confirm, confirm_yes, boot_requested, dirty;
    int64_t value;
    unsigned action;
    Project draft;
    System system_draft;
    uint8_t receive, acceleration;
    char name[9], toast[22];
    uint32_t toast_until;
    bool raw[3], stable[3], long_sent[3], chord, chord_sent;
    uint32_t changed[3], pressed[3], repeat[3], chord_since;
} Ui;
void ui_init(Ui *u, Settings *s, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx);
void ui_open(Ui *u, unsigned page);
void ui_click(Ui *u, unsigned button, bool long_press, uint32_t now);
void ui_buttons(Ui *u, const bool down[3], uint32_t now);
void ui_render(Ui *u, const UiStatus *status, uint32_t now);
void ui_external_change(Ui *u);
unsigned ui_count(Ui *u);
