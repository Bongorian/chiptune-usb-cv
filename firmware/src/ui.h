#pragma once
#include "engine.h"
typedef struct {
    bool power, usb, suspended, adc_ok;
    uint32_t uart_overflow, usb_dropped, tick_us, tick_overruns, clock_hz;
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
    uint8_t group, field, route_channel[SOURCES], tone_chip, sequence_step;
    bool menu, edit;
    bool raw[3], stable[3], long_sent[3];
    uint32_t changed[3], pressed[3], repeat[3], toast_until;
    char toast[22];
    uint8_t page, part, depth, path[6], cursor[6], memory[40];
    uint8_t cv_input, mod_slot, macro, slot, library_slot, target_part, channel;
    uint8_t grid_lane, grid_step, test_note, test_mode, name_target, name_cursor;
    uint8_t confirm_action, name_return;
    uint8_t active_sound, pending_sound, factory_slot;
    bool dirty, confirm, confirm_yes, boot_requested, chord, chord_sent;
    bool was_dirty, previewed, system_dirty, was_system_dirty;
    bool layout_reset;
    uint8_t profile;
    uint32_t chord_since;
    int draft, original;
    uint8_t acceleration;
    char name[9];
    Preset undo;
    StScene candidate;
} Ui;
typedef enum {
    UI_PLAY,
    UI_SOUND,
    UI_ROUTE,
    UI_MOTION,
    UI_SCENE,
    UI_SYSTEM,
    UI_OSC,
    UI_AMP,
    UI_VOICE,
    UI_MACROS,
    UI_USB,
    UI_TRS,
    UI_CV,
    UI_GATE,
    UI_MOD,
    UI_SN_CLOCK,
    UI_SEQ,
    UI_GRID,
    UI_ARP,
    UI_TEMPO,
    UI_PARTS,
    UI_LIBRARY,
    UI_NAME,
    UI_RESOURCES,
    UI_CAL,
    UI_MONITOR,
    UI_CLOCK_STEPS,
    UI_MIX,
    UI_MENU,
    UI_MIDI,
    UI_SETUP,
    UI_SAVE_SOUND,
    UI_FACTORY,
    UI_PAGES
} UiPage;
void ui_open(Ui *u, unsigned page);
void ui_click(Ui *u, unsigned button, bool long_press, uint32_t now);
unsigned ui_count(Ui *u);
void ui_init(Ui *u, Settings *s, Engine *e, bool (*save)(void *),
             bool (*calibrate)(void *, unsigned, bool), void *ctx);
void ui_buttons(Ui *u, const bool down[3], uint32_t now);
void ui_render(Ui *u, const UiStatus *status, uint32_t now);

void ui_external_change(Ui *u);
