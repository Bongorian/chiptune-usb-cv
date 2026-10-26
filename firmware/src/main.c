#include "cv.h"
#include "editor.h"
#include "engine.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "midi.h"
#include "oled.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pinmap.h"
#include "power.h"
#include "service.h"
#include "sn_clock.pio.h"
#include "sound_bus.h"
#include "store.h"
#include "tusb.h"
#include "ui.h"
#include <math.h>
#include <string.h>
#define UART_RING 512u
#define GATE_RING 32u
#define USB_RING 128u
#define I2C_TIMEOUT_US 900
_Static_assert(PICO_FLASH_SIZE_BYTES == STORE_PHYSICAL_BYTES,
               "Keep SDK physical flash capacity separate from the application linker limit");
static Engine engine;
static Settings settings;
static Ui ui;
static UiStatus status;
static PowerPolicy power_policy;
static volatile bool rail_on;
static bool usb_suspended, was_mounted;
static volatile uint16_t uart_head, uart_tail;
static volatile uint8_t uart_bytes[UART_RING];
static volatile uint32_t uart_overflows;
static volatile uint8_t gate_head, gate_tail;
static volatile uint8_t gate_events[GATE_RING];
static volatile uint32_t gate_overflows;
static uint8_t usb_tx[USB_RING][4];
static unsigned usb_head, usb_tail;
static bool forward_panic;
static MidiParser parsers[2];
static Editor editor;
static uint32_t source_id[2] = {0, 1};
static int store_bank = -1;
static uint32_t store_sequence;
static uint32_t cv_time[2], cv_sequence[2], cv_last_cal[2];
static int16_t cv_samples[2][64];
static unsigned cv_count[2], cv_pos[2];
static bool cal_zero_set[2];
static int adc_input = -1;
static uint32_t adc_started, adc_poll_due;
static Oled oled;
static SnClock clock_fx;
static int clock_sm = -1;
static unsigned clock_offset;
static uint32_t seen_note_trigger, seen_manual_trigger, seen_midi_clocks;
static uint32_t clock_last_us;
static SnClockCounter clock_counter;
static SnClockPlan clock_requested;
static uint8_t seen_clock_sequence[8];
static PIO const clock_pio = pio0;
static void sn_hardware_clock(SnClockPlan p, bool stop, uint64_t now) {
    unsigned sm = (unsigned)clock_sm;
    now = time_us_64();
    bool divider_only =
        !stop && !clock_counter.stopped && clock_counter.plan.period == p.period;
    sn_counter_change(&clock_counter, p, now, stop);
    if (divider_only) {
        pio_sm_set_clkdiv_int_frac(clock_pio, sm, (uint16_t)(p.div256 >> 8), (uint8_t)p.div256);
        pio_sm_clkdiv_restart(clock_pio, sm);
    } else {
        pio_sm_set_enabled(clock_pio, sm, false);
        if (!stop) {
            pio_sm_config cfg = sn_clock_program_get_default_config(clock_offset);
            sm_config_set_set_pins(&cfg, PIN_SN_CLK_RAW, 1);
            sm_config_set_clkdiv_int_frac(&cfg, (uint16_t)(p.div256 >> 8), (uint8_t)p.div256);
            pio_sm_init(clock_pio, sm, clock_offset, &cfg);
            pio_sm_set_pins_with_mask(clock_pio, sm, 0, 1u << PIN_SN_CLK_RAW);
            pio_sm_put(clock_pio, sm, p.period);
            pio_sm_clkdiv_restart(clock_pio, sm);
            pio_sm_set_enabled(clock_pio, sm, true);
        } else
            pio_sm_set_pins_with_mask(clock_pio, sm, 0, 1u << PIN_SN_CLK_RAW);
    }
    clock_counter.epoch_us = time_us_64(); /* exclude disabled/configuration time */
    engine.sn_clock_period = p.period;
    engine.sn_clock_div256 = p.div256;
    engine.sn_clock_hz = stop ? 0 : p.hz;
    engine.sn_experimental = !stop && p.experimental;
    engine.clock_restarts = clock_counter.restarts;
}
/* Runs in the cooperative loop: no floating-point work in interrupt context.
   Every hardware restart invalidates partial period credit. This deliberately
   under-counts transfers during rapid/very slow modulation, never releases early. */
static void sn_clock_control(void) {
    uint64_t now = time_us_64();
    SnClockConfig config = engine_effective_clock(&engine);
    bool changed = memcmp(&clock_fx.config, &config, sizeof(config)) != 0;
    if (changed)
        sn_clock_configure(&clock_fx, &config);
    if (memcmp(seen_clock_sequence, engine.project.clock_sequence, 8)) {
        sn_clock_sequence(&clock_fx, engine.project.clock_sequence);
        memcpy(seen_clock_sequence, engine.project.clock_sequence, 8);
    }
    uint32_t clocks = engine_transport_clocks(&engine);
    if (clocks != seen_midi_clocks)
        sn_clock_midi(&clock_fx, clocks, (uint32_t)now);
    bool manual = engine.sn_manual_trigger != seen_manual_trigger;
    bool note = engine.sn_note_trigger != seen_note_trigger;
    if (manual || note)
        sn_clock_trigger(&clock_fx, manual, (uint32_t)now);
    seen_midi_clocks = clocks;
    seen_note_trigger = engine.sn_note_trigger;
    seen_manual_trigger = engine.sn_manual_trigger;
    uint32_t elapsed = (uint32_t)now - clock_last_us;
    if (elapsed >= SN_CLOCK_TICK_US || changed || manual || note) {
        uint32_t started = time_us_32();
        engine.sn_fx_flags = elapsed > SN_CLOCK_TICK_US * 2 ? 1 : 0;
        if (engine.sn_fx_flags)
            engine.clock_late++;
        clock_requested = sn_clock_tick(&clock_fx, elapsed, (uint32_t)now);
        clock_last_us = (uint32_t)now;
        engine.clock_updates++;
        uint32_t duration = time_us_32() - started;
        if (duration > engine.clock_max_us)
            engine.clock_max_us = duration;
    }
    bool stopping = config.stopped || !clock_requested.available;
    engine.sn_stop_pending = stopping && engine.bus.busy;
    /* Finish an in-flight divider's two-byte snapshot before stopping. No new
       SN transaction starts while stop is pending; SAA traffic can continue. */
    sn_counter_advance(&clock_counter, now);
    bus_service(&engine.bus, now, clock_counter.cycles, !stopping && !clock_counter.stopped);
    if (!engine.sn_stop_pending) {
        if (stopping) {
            if (!clock_counter.stopped)
                sn_hardware_clock(clock_counter.plan, true, now);
        } else if (clock_counter.stopped ||
                   clock_requested.period != clock_counter.plan.period ||
                   clock_requested.div256 != clock_counter.plan.div256)
            sn_hardware_clock(clock_requested, false, now);
    }
    for (unsigned i = 0; i < 2; i++) {
        const SnModConfig *m = &config.mod[i];
        bool aliased =
            m->sync ? (uint64_t)clock_fx.midi_interval_us * sn_clock_sync_clocks[m->sync] <
                          SN_CLOCK_TICK_US * 2u
                    : m->rate_millihz > 1000000000u / (SN_CLOCK_TICK_US * 2u);
        if (m->mode > SN_CLOCK_MANUAL && aliased)
            engine.sn_fx_flags |= 2;
    }
    bool delayed = engine.bus.busy && now - engine.bus.started > 1000;
    bool lag = false;
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 3; n++)
            lag |= engine.sn_target_hz[c][n] > 0 &&
                   engine.bus.tone[c][n] != engine.bus.tone_sent[c][n];
    engine.sn_clock_state =
        !clock_requested.available ? SN_UNAVAILABLE
        : clock_counter.stopped    ? SN_STOPPED
        : engine.sn_experimental || delayed || engine.sn_clip_mask || engine.sn_fx_flags ||
                (config.follow == SN_FOLLOW_FULL && lag) || engine.sn_stop_pending
            ? SN_DEGRADED
            : SN_VALID;
}
static void __attribute__((noinline)) sn_clock_start(void) {
    sn_clock_init(&clock_fx, &engine.project.sn_clock);
    memset(seen_clock_sequence, 255, 8);
    seen_note_trigger = engine.sn_note_trigger;
    seen_manual_trigger = engine.sn_manual_trigger;
    seen_midi_clocks = engine_transport_clocks(&engine);
    pio_gpio_init(clock_pio, PIN_SN_CLK_RAW);
    pio_sm_set_consecutive_pindirs(clock_pio, (unsigned)clock_sm, PIN_SN_CLK_RAW, 1, true);
    clock_requested = sn_clock_sample(&clock_fx);
    uint64_t now = time_us_64();
    clock_last_us = (uint32_t)now;
    sn_counter_init(&clock_counter, clock_requested, now);
    clock_counter.stopped = true; /* first start must initialize and enable the SM */
    sn_hardware_clock(clock_requested,
                      engine.project.sn_clock.stopped || !clock_requested.available, now);
}
static const unsigned cs[3] = {PIN_SAA_CS_N, PIN_SN1_CS_N, PIN_SN2_CS_N};
static void bus_cycle(unsigned select, bool address, uint8_t byte, unsigned duration) {
    if (!rail_on)
        return;
    gpio_put(PIN_SAA_A0, address);
    gpio_put_masked(0xffu, byte);
    gpio_put(select, 0);
    busy_wait_us_32(1);
    gpio_put(PIN_WR_N, 0);
    busy_wait_us_32(duration);
    gpio_put(PIN_WR_N, 1);
    gpio_put(select, 1);
    busy_wait_us_32(2);
}
static void saa_write(void *ctx, uint8_t reg, uint8_t value) {
    (void)ctx;
    bus_cycle(cs[0], true, chiptune_saa_gpio_word(reg), 2);
    bus_cycle(cs[0], false, chiptune_saa_gpio_word(value), 2);
}
/* SN /WE stays low while the main loop continues servicing USB, MIDI and UI.
   The register bus calls sn_end only after the conservative clock-dependent hold time. */
static void sn_begin(void *ctx, uint8_t chip, uint8_t value) {
    (void)ctx;
    if (!rail_on)
        return;
    gpio_put(PIN_SAA_A0, 0);
    gpio_put_masked(0xffu, chiptune_sn_gpio_word(value));
    gpio_put(cs[chip + 1], 0);
    busy_wait_us_32(1);
    gpio_put(PIN_WR_N, 0);
}
static void sn_end(void *ctx) {
    (void)ctx;
    gpio_put(PIN_WR_N, 1);
    gpio_put(cs[1], 1);
    gpio_put(cs[2], 1);
    busy_wait_us_32(2);
}
static uint64_t sn_completed_cycles(void *ctx) {
    (void)ctx;
    sn_counter_advance(&clock_counter, time_us_64());
    return clock_counter.cycles;
}
static void clock_start(unsigned pin, unsigned wrap) {
    pwm_config c = pwm_get_default_config();
    pwm_config_set_wrap(&c, (uint16_t)wrap);
    pwm_config_set_clkdiv(&c, 1);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    unsigned slice = pwm_gpio_to_slice_num(pin);
    pwm_init(slice, &c, false);
    pwm_set_gpio_level(pin, (uint16_t)((wrap + 1) / 2));
    pwm_set_enabled(slice, true);
}
static void high_z(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
}
static void sound_off(void) {
    if (rail_on)
        engine_panic(&engine, -1);
    pwm_set_enabled(pwm_gpio_to_slice_num(PIN_SAA_CLK_RAW), false);
    rail_on = false;
    if (clock_sm >= 0) {
        pio_sm_set_enabled(clock_pio, (unsigned)clock_sm, false);
        pio_sm_clear_fifos(clock_pio, (unsigned)clock_sm);
    }
    for (int p = 0; p < 8; p++)
        high_z((unsigned)p);
    high_z(PIN_WR_N);
    high_z(PIN_SAA_A0);
    for (int i = 0; i < 3; i++)
        high_z(cs[i]);
    high_z(PIN_SAA_CLK_RAW);
    high_z(PIN_SN_CLK_RAW);
    high_z(PIN_SAA_DTACK_N);
    gpio_put(PIN_SOUND_ENABLE, 0);
    rail_on = false;
    status.power = false;
}
static void sound_on(void) {
    gpio_put(PIN_SOUND_ENABLE, 1);
    sleep_ms(12);
    for (unsigned p = 0; p < 8; p++) {
        gpio_put(p, 0);
        gpio_set_dir(p, GPIO_OUT);
    }
    for (int i = 0; i < 3; i++) {
        gpio_put(cs[i], 1);
        gpio_set_dir(cs[i], GPIO_OUT);
    }
    gpio_put(PIN_WR_N, 1);
    gpio_set_dir(PIN_WR_N, GPIO_OUT);
    gpio_put(PIN_SAA_A0, 0);
    gpio_set_dir(PIN_SAA_A0, GPIO_OUT);
    clock_start(PIN_SAA_CLK_RAW, 15);
    engine_reset_audio(&engine,
                       (SoundSink){saa_write, sn_begin, sn_end, NULL, sn_completed_cycles});
    sn_clock_start();
    rail_on = true;
    status.power = true;
    /* Discard stale UART data at rail transition. */
    uint32_t irq = save_and_disable_interrupts();
    uart_tail = uart_head;
    gate_tail = gate_head;
    restore_interrupts(irq);
    for (int s = 0; s < 2; s++)
        midi_init(&parsers[s], parsers[s].callback, parsers[s].ctx);
}
static void uart_irq_handler(void) {
    while (uart_is_readable(uart0)) {
        uint32_t dr = uart_get_hw(uart0)->dr;
        if (dr & 0xf00u) {
            uart_overflows++;
            uart_get_hw(uart0)->rsr = 0;
            continue;
        }
        uint8_t b = (uint8_t)dr;
        uint16_t next = (uint16_t)((uart_head + 1) % UART_RING);
        if (next == uart_tail)
            uart_overflows++;
        else {
            uart_bytes[uart_head] = b;
            __dmb();
            uart_head = next;
        }
    }
}
static void gate_irq_handler(unsigned pin, uint32_t events) {
    (void)events;
    unsigned input = pin == PIN_GATE1_N ? 0 : 1;
    uint8_t next = (uint8_t)((gate_head + 1) % GATE_RING);
    if (next == gate_tail)
        gate_overflows++;
    else {
        gate_events[gate_head] = (uint8_t)((input << 1) | (!gpio_get(pin)));
        __dmb();
        gate_head = next;
    }
}
static void midi_received(void *ctx, const uint8_t *data, uint8_t len, uint8_t cin) {
    unsigned source = *(uint32_t *)ctx;
    if (source == 0)
        for (unsigned i = 0; i < len; i++)
            editor_byte(&editor, data[i], to_ms_since_boot(get_absolute_time()));
    if (rail_on)
        engine_message(&engine, (uint8_t)source, data, len);
    if (source == 1 && engine.project.midi_thru && tud_mounted() && !usb_suspended) {
        unsigned next = (usb_head + 1) % USB_RING;
        if (next == usb_tail || forward_panic) {
            forward_panic = true;
            status.usb_dropped++;
            return;
        }
        uint8_t *packet = usb_tx[usb_head];
        memset(packet, 0, 4);
        packet[0] = cin;
        memcpy(packet + 1, data, len);
        usb_head = next;
    }
}
void tud_mount_cb(void) { usb_suspended = false; }
void tud_umount_cb(void) { usb_suspended = false; }
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    usb_suspended = tud_mounted();
}
void tud_resume_cb(void) { usb_suspended = false; }
static void midi_tasks(uint32_t now) {
    static uint32_t previous_overflows;
    if (uart_overflows != previous_overflows) {
        previous_overflows = uart_overflows;
        if (engine.project.midi_thru)
            forward_panic = true;
        engine_panic(&engine, 1);
        midi_init(&parsers[1], midi_received, &source_id[1]);
        uint32_t irq = save_and_disable_interrupts();
        uart_tail = uart_head;
        restore_interrupts(irq);
    }
    for (unsigned budget = 0; budget < 64 && uart_tail != uart_head; budget++) {
        uint8_t b = uart_bytes[uart_tail];
        __dmb();
        uart_tail = (uint16_t)((uart_tail + 1) % UART_RING);
        midi_byte(&parsers[1], b, now);
    }
    uint8_t packet[4];
    for (unsigned budget = 0; budget < 16 && tud_midi_packet_read(packet); budget++) {
        if ((packet[0] >> 4) != 0)
            continue;
        uint8_t len = midi_packet_length(packet[0]);
        for (uint8_t i = 0; i < len; i++)
            midi_byte(&parsers[0], packet[i + 1], now);
    }
    for (int s = 0; s < 2; s++)
        if (midi_sensing_expired(&parsers[s], now)) {
            engine_panic(&engine, s);
            if (s == 1 && engine.project.midi_thru)
                forward_panic = true;
        }
    if (forward_panic && tud_mounted() && !usb_suspended) {
        usb_tail = usb_head = 0;
        for (unsigned c = 0; c < 16; c++) {
            uint8_t *p = usb_tx[usb_head++];
            p[0] = 0x0b;
            p[1] = (uint8_t)(0xb0 | c);
            p[2] = 120;
            p[3] = 0;
        }
        forward_panic = false;
    }
    for (unsigned budget = 0; budget < 16 && usb_tail != usb_head; budget++) {
        if (!tud_midi_packet_write(usb_tx[usb_tail]))
            break;
        usb_tail = (usb_tail + 1) % USB_RING;
    }
    status.uart_overflow = uart_overflows;
}
static int i2c_write(uint8_t address, const uint8_t *bytes, size_t len) {
    return i2c_write_timeout_us(i2c1, address, bytes, len, false, I2C_TIMEOUT_US);
}
static bool adc_read_reg(uint8_t reg, uint16_t *value) {
    uint8_t data[2];
    if (i2c_write_timeout_us(i2c1, 0x48, &reg, 1, true, I2C_TIMEOUT_US) != 1)
        return false;
    if (i2c_read_timeout_us(i2c1, 0x48, data, 2, false, I2C_TIMEOUT_US) != 2)
        return false;
    *value = (uint16_t)((data[0] << 8) | data[1]);
    return true;
}
static void cv_adc_task(uint32_t now_us) {
    static uint32_t retry_due;
    static unsigned next;
    if (adc_input < 0) {
        if ((int32_t)(now_us - retry_due) < 0)
            return;
        uint16_t config =
            (uint16_t)(0x8000 | (next ? 3u << 12 : 0) | (2u << 9) | (1u << 8) | (7u << 5) | 3u);
        uint8_t data[] = {1, (uint8_t)(config >> 8), (uint8_t)config};
        if (i2c_write(0x48, data, 3) == 3) {
            adc_input = (int)next;
            adc_started = now_us;
            adc_poll_due = now_us + 1400;
            next ^= 1;
        } else {
            status.adc_ok = false;
            retry_due = now_us + 250000;
        }
        return;
    }
    if ((int32_t)(now_us - adc_poll_due) < 0)
        return;
    if (now_us - adc_started > 12000) {
        status.adc_ok = false;
        adc_input = -1;
        retry_due = now_us + 250000;
        return;
    }
    uint16_t conf, raw;
    adc_poll_due = now_us + 200;
    if (!adc_read_reg(1, &conf)) {
        status.adc_ok = false;
        adc_input = -1;
        retry_due = now_us + 250000;
        return;
    }
    if (!(conf & 0x8000))
        return;
    if (adc_read_reg(0, &raw)) {
        unsigned input = (unsigned)adc_input;
        status.cv_raw[input] = (int16_t)raw;
        cv_time[input] = to_ms_since_boot(get_absolute_time());
        cv_sequence[input]++;
        cv_samples[input][cv_pos[input]++ % 64] = (int16_t)raw;
        if (cv_count[input] < 64)
            cv_count[input]++;
        status.adc_ok = true;
    } else
        status.adc_ok = false;
    adc_input = -1;
}
static void cv_event(unsigned input, bool gate) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    engine_cv(&engine, input, cv_volts(&engine.system, input, status.cv_raw[input]), gate,
              status.adc_ok && cv_sequence[input] && now - cv_time[input] < 100);
}
static void gate_tasks(uint32_t now) {
    static uint32_t previous_overflow, poll_due;
    if (previous_overflow != gate_overflows) {
        previous_overflow = gate_overflows;
        engine_panic(&engine, -1);
        uint32_t irq = save_and_disable_interrupts();
        gate_tail = gate_head;
        restore_interrupts(irq);
    }
    for (unsigned budget = 0; budget < 16 && gate_tail != gate_head; budget++) {
        uint8_t event = gate_events[gate_tail];
        __dmb();
        gate_tail = (uint8_t)((gate_tail + 1) % GATE_RING);
        if (rail_on)
            cv_event(event >> 1, (event & 1) != 0);
    }
    if ((int32_t)(now - poll_due) < 0)
        return;
    poll_due = now + 1;
    for (unsigned i = 0; i < 2; i++) {
        bool gate = !gpio_get(i ? PIN_GATE2_N : PIN_GATE1_N);
        status.gates[i] = gate;
        if (rail_on)
            cv_event(i, gate);
    }
}

static bool calibrate(void *ctx, unsigned input, bool one) {
    (void)ctx;
    if (input >= 2 || cv_count[input] < 64 || !status.adc_ok ||
        cv_sequence[input] == cv_last_cal[input])
        return false;
    if (!cv_calibration(&engine.system, input, cv_samples[input], one, cal_zero_set))
        return false;
    cv_last_cal[input] = cv_sequence[input];
    engine_panic(&engine, -1);
    return true;
}
/* Explicit user save only. Boot and unknown settings versions never erase/program flash. */
static bool commit_settings(void) {
    if (!settings_valid(&settings))
        return false;
    static StoreRecord record;
    store_record(&record, &settings, store_sequence + 1);
    static uint8_t pages[STORE_BANK_BYTES];
    memset(pages, 0xff, sizeof(pages));
    memcpy(pages, &record, sizeof(record));
    /* Commit marker is programmed last; the other bank survives any interrupted write. */
    memset(pages, 0xff, 4);
    unsigned dest = store_bank == 0 ? 1 : 0;
    uint32_t offset = STORE_FLASH_OFFSET + dest * STORE_BANK_BYTES;
    if (offset < STORE_FLASH_OFFSET || offset + sizeof(pages) > STORE_PHYSICAL_BYTES)
        return false;
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, STORE_BANK_BYTES);
    flash_range_program(offset, pages, STORE_BANK_BYTES);
    memcpy(pages, &record, 4);
    flash_range_program(offset, pages, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    const StoreRecord *written = (const StoreRecord *)(XIP_BASE + offset);
    bool ok = store_valid(written);
    if (ok) {
        store_bank = (int)dest;
        store_sequence = record.sequence;
    }
    return ok;
}
static bool save_settings(void *ctx) {
    (void)ctx;
    if (!rail_on || usb_suspended || cal_zero_set[0] || cal_zero_set[1] ||
        !settings_valid(&settings))
        return false;
    engine_panic(&engine, -1);
    uint32_t mute_deadline = time_us_32() + 10000;
    while (bus_pending(&engine.bus) && (int32_t)(time_us_32() - mute_deadline) < 0) {
        sn_clock_control();
        tud_task();
    }
    if (bus_pending(&engine.bus))
        return false;
    bool ok = commit_settings();
    /* Flash erase stalls MIDI: flush partial messages and notes. No autosave during a
     * performance.
     */
    uint32_t irq = save_and_disable_interrupts();
    uart_tail = uart_head;
    gate_tail = gate_head;
    restore_interrupts(irq);
    for (int s = 0; s < 2; s++)
        midi_init(&parsers[s], midi_received, &source_id[s]);
    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
    }
    usb_tail = usb_head;
    return ok;
}
static bool oled_write(void *ctx, const uint8_t *bytes, size_t length) {
    (void)ctx;
    return i2c_write(0x3c, bytes, length) == (int)length;
}
static void oled_render(void *ctx, uint32_t now) {
    (void)ctx;
    ui_render(&ui, &status, now);
}
int main(void) {
    /* No stdio: GP12 remains SAA A0, GP13 is UART0 RX only. */
    gpio_init(PIN_SOUND_ENABLE);
    gpio_put(PIN_SOUND_ENABLE, 0);
    gpio_set_dir(PIN_SOUND_ENABLE, GPIO_OUT);
    set_sys_clock_khz(128000, true);
    for (unsigned p = 0; p < 8; p++) {
        gpio_init(p);
        high_z(p);
    }
    gpio_init(PIN_WR_N);
    gpio_init(PIN_SAA_A0);
    for (int i = 0; i < 3; i++)
        gpio_init(cs[i]);
    sound_off();
    gpio_init(PIN_STATUS_LED);
    gpio_set_dir(PIN_STATUS_LED, GPIO_OUT);
    gpio_put(PIN_STATUS_LED, 0);
    const unsigned buttons[] = {PIN_UI_MINUS, PIN_UI_OK, PIN_UI_PLUS};
    for (int i = 0; i < 3; i++) {
        gpio_init(buttons[i]);
        gpio_pull_up(buttons[i]);
    }
    /* Recovery stays available even with a corrupt settings bank or absent sound ICs. */
    sleep_ms(25);
    if (!gpio_get(PIN_UI_MINUS) && !gpio_get(PIN_UI_PLUS)) {
        sleep_ms(25);
        if (!gpio_get(PIN_UI_MINUS) && !gpio_get(PIN_UI_PLUS))
            reset_usb_boot(0, 0);
    }
    bool boot_defaults = !gpio_get(PIN_UI_OK);
    /* Bring up the USB stack before storage/peripheral/engine setup. No flash
       write, migration failure or missing I2C device may gate USB initialization. */
    tusb_init();
    tud_task();
    gpio_init(PIN_GATE1_N);
    gpio_pull_up(PIN_GATE1_N);
    gpio_init(PIN_GATE2_N);
    gpio_pull_up(PIN_GATE2_N);
    gpio_set_irq_enabled_with_callback(PIN_GATE1_N, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                       true, gate_irq_handler);
    gpio_set_irq_enabled(PIN_GATE2_N, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    i2c_init(i2c1, 400000);
    gpio_set_function(PIN_UI_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_UI_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_UI_SDA);
    gpio_pull_up(PIN_UI_SCL);
    adc_init();
    adc_gpio_init(PIN_CC1_ADC);
    adc_gpio_init(PIN_CC2_ADC);
    uart_init(uart0, 31250);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    gpio_set_function(PIN_MIDI_RX, GPIO_FUNC_UART);
    uart_set_fifo_enabled(uart0, true);
    irq_set_exclusive_handler(UART0_IRQ, uart_irq_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
    store_bank = store_restore((const uint8_t *)(XIP_BASE + STORE_FLASH_OFFSET), boot_defaults,
                               &settings, &store_sequence);
    tud_task();
    engine_init(&engine, &settings.project[settings.selected],
                (SoundSink){saa_write, sn_begin, sn_end, NULL, sn_completed_cycles});
    for (int s = 0; s < 2; s++)
        midi_init(&parsers[s], midi_received, &source_id[s]);
    ui_init(&ui, &settings, &engine, save_settings, calibrate, NULL);
    if (boot_defaults) {
        memcpy(ui.toast, "DEFAULTS (RAM)", 15);
        ui.toast_until = to_ms_since_boot(get_absolute_time()) + 5000;
    }
    oled_init(&oled, ui.frame, oled_write, oled_render, NULL);
    clock_sm = pio_claim_unused_sm(clock_pio, true);
    clock_offset = pio_add_program(clock_pio, &sn_clock_program);
    tud_task();
    uint32_t tick = time_us_32(), power_due = 0;
    bool suspend_handled = false;
    uint32_t oled_serviced = tick, adc_serviced = tick;
    for (;;) {
        tud_task();
        uint32_t now_us = time_us_32(), now = to_ms_since_boot(get_absolute_time());
        if (usb_suspended) {
            if (!suspend_handled) {
                sound_off();
                oled_suspend(&oled);
                gpio_put(PIN_STATUS_LED, 0);
                suspend_handled = true;
            }
            /* Mask across the queue check: a pending IRQ wakes WFI, then runs after restore. */
            uint32_t irq = save_and_disable_interrupts();
            if (!tud_task_event_ready()) {
                __dsb();
                __wfi();
            }
            restore_interrupts(irq);
            continue;
        }
        if (suspend_handled) {
            suspend_handled = false;
            tick = now_us;
            power_due = now;
        }
        bool mounted = tud_mounted();
        status.clock_hz = rail_on ? engine.sn_clock_hz : 0;
        status.usb = mounted;
        status.suspended = usb_suspended;
        if (was_mounted && !mounted) {
            engine_panic(&engine, 0);
            midi_init(&parsers[0], midi_received, &source_id[0]);
            usb_tail = usb_head;
        }
        was_mounted = mounted;
        if ((int32_t)(now - power_due) >= 0) {
            adc_select_input(0);
            uint16_t cc1 = adc_read();
            adc_select_input(1);
            uint16_t cc2 = adc_read();
            float volts = (cc1 > cc2 ? cc1 : cc2) * 6.4f / 4095;
            bool allowed = power_allowed(&power_policy, mounted, usb_suspended, volts, now);
            if (allowed && !rail_on)
                sound_on();
            if (!allowed && rail_on)
                sound_off();
            power_due = now + 10;
        }
        midi_tasks(now);
        if (!tud_mounted())
            editor_init(&editor);
        editor_task(&editor, &engine, &settings,
                    !rail_on || usb_suspended || ui.edit || ui.confirm || ui.page == UI_NAME,
                    save_settings, NULL, now);
        if (editor.changed) {
            ui_external_change(&ui);
            editor.changed = false;
        }
        unsigned free_packets = (usb_tail + USB_RING - usb_head - 1) % USB_RING;
        /* Do not nest an editor response inside a forwarded TRS SysEx message. */
        if (editor.length && (!engine.project.midi_thru || !parsers[1].sysex) &&
            free_packets >= (editor.length + 2) / 3) {
            while (editor_packet(&editor, usb_tx[usb_head]))
                usb_head = (usb_head + 1) % USB_RING;
        }
        gate_tasks(now);
        if (rail_on) {
            sn_clock_control();
        }
        if (now_us - tick >= 1000) {
            uint32_t elapsed = (now_us - tick) / 1000;
            tick += elapsed * 1000;
            uint32_t start = time_us_32();
            if (rail_on)
                engine_tick(&engine, elapsed);
            uint32_t duration = time_us_32() - start;
            engine.tick_max_us = duration > engine.tick_max_us ? duration : engine.tick_max_us;
            if (duration > 1000 || elapsed > 1)
                engine.tick_overruns++;
            if (duration > status.tick_us)
                status.tick_us = duration;
            if (duration > 1000 || elapsed > 1)
                status.tick_overruns++;
            bool down[3];
            for (int b = 0; b < 3; b++)
                down[b] = !gpio_get(buttons[b]);
            ui_buttons(&ui, down, now);
            if (ui.boot_requested) {
                sound_off();
                reset_usb_boot(0, 0);
            }
        }
        /* Busy audio/CV work may miss every idle window. Force each bounded
           background service after 4ms instead of leaving the screen dark. */
        uint32_t service_us = time_us_32();
        if (service_ready(service_us, tick, adc_serviced, 500, 4000)) {
            adc_serviced = service_us;
            cv_adc_task(service_us);
        }
        service_us = time_us_32();
        if (service_ready(service_us, tick, oled_serviced, 250, 4000)) {
            oled_serviced = service_us;
            oled_task(&oled, to_ms_since_boot(get_absolute_time()), rail_on,
                      engine.system.brightness);
        }
        gpio_put(PIN_STATUS_LED, rail_on && (engine_active(&engine) > 0 || (now % 2000) < 50));
        tight_loop_contents();
    }
}
