#include "engine.h"
#include "midi.h"
#include "store.h"
#include "ui.h"
#include "studio.h"
#include "oled.h"
#include "service.h"
#include "power.h"
#include "cv.h"
#include "pinmap.h"
#include "sound_bus.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "sn_clock.pio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "tusb.h"
#include <math.h>
#include <string.h>
#define UART_RING 512u
#define GATE_RING 32u
#define USB_RING 128u
#define I2C_TIMEOUT_US 900
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
static struct repeating_timer clock_timer;
static bool clock_timer_active;
static int clock_sm = -1;
static unsigned clock_offset;
static uint32_t seen_note_trigger, seen_manual_trigger, seen_midi_clocks;
static volatile unsigned sn_wait_us = 136;
static unsigned next_sn_wait_us = 16;
static volatile uint16_t last_clock_half;
static uint32_t clock_last_irq_us;
static uint8_t seen_clock_sequence[8];
static PIO const clock_pio = pio0;
static void sn_clock_control(void) {
    SnClockConfig config = engine_effective_clock(&engine);
    uint32_t clocks = engine_transport_clocks(&engine);
    unsigned wait = sn_clock_write_us(&config);
    bool note = engine.sn_note_trigger != seen_note_trigger;
    bool manual = engine.sn_manual_trigger != seen_manual_trigger;
    uint32_t irq = save_and_disable_interrupts();
    if (memcmp(&clock_fx.config, &config, sizeof(config))) {
        if (wait > sn_wait_us)
            sn_wait_us = wait;
        next_sn_wait_us = wait;
        sn_clock_configure(&clock_fx, &config);
    }
    if (memcmp(seen_clock_sequence, engine.preset.clock_sequence, sizeof(seen_clock_sequence))) {
        sn_clock_sequence(&clock_fx, engine.preset.clock_sequence);
        memcpy(seen_clock_sequence, engine.preset.clock_sequence, sizeof(seen_clock_sequence));
    }
    if (clocks != seen_midi_clocks)
        sn_clock_midi(&clock_fx, clocks);
    if (manual || (note && config.retrigger))
        sn_clock_trigger(&clock_fx);
    restore_interrupts(irq);
    seen_note_trigger = engine.sn_note_trigger;
    seen_manual_trigger = engine.sn_manual_trigger;
    seen_midi_clocks = clocks;
}
static bool sn_clock_irq(struct repeating_timer *timer) {
    (void)timer;
    if (!rail_on || clock_sm < 0)
        return true;
    uint32_t now = time_us_32(), elapsed = now - clock_last_irq_us;
    /* A flash erase or delayed IRQ must not cause a burst of catch-up FIFO writes. */
    timer->delay_us =
        elapsed < 125 || elapsed > 375 ? (int64_t)SN_CLOCK_TICK_US : -(int64_t)SN_CLOCK_TICK_US;
    if (elapsed < 125)
        return true;
    clock_last_irq_us = now;
    uint16_t half = sn_clock_tick(&clock_fx);
    if (half != last_clock_half) {
        if (pio_sm_is_tx_fifo_full(clock_pio, (unsigned)clock_sm))
            return true;
        pio_sm_put(clock_pio, (unsigned)clock_sm, half - 5u);
        last_clock_half = half;
    }
    /* The 8us margin also covers an old period still finishing at the PIO boundary. */
    sn_wait_us = next_sn_wait_us;
    return true;
}
static void sn_clock_start(void) {
    const SnClockConfig boot_clock = {SN_CLOCK_OFF, 0, 24, 4, 0, 1};
    sn_clock_init(&clock_fx, &boot_clock);
    memset(seen_clock_sequence, 0xff, sizeof(seen_clock_sequence));
    seen_note_trigger = engine.sn_note_trigger;
    seen_manual_trigger = engine.sn_manual_trigger;
    seen_midi_clocks = engine_transport_clocks(&engine);
    next_sn_wait_us = sn_wait_us = sn_clock_write_us(&clock_fx.config);
    pio_gpio_init(clock_pio, PIN_SN_CLK_RAW);
    pio_sm_config cfg = sn_clock_program_get_default_config(clock_offset);
    sm_config_set_set_pins(&cfg, PIN_SN_CLK_RAW, 1);
    sm_config_set_clkdiv(&cfg, 1.0f);
    pio_sm_init(clock_pio, (unsigned)clock_sm, clock_offset, &cfg);
    pio_sm_set_consecutive_pindirs(clock_pio, (unsigned)clock_sm, PIN_SN_CLK_RAW, 1, true);
    last_clock_half = sn_clock_sample(&clock_fx);
    pio_sm_set_pins_with_mask(clock_pio, (unsigned)clock_sm, 0, 1u << PIN_SN_CLK_RAW);
    pio_sm_put(clock_pio, (unsigned)clock_sm, last_clock_half - 5u);
    pio_sm_set_enabled(clock_pio, (unsigned)clock_sm, true);
    clock_last_irq_us = time_us_32();
    hard_assert(
        add_repeating_timer_us(-(int64_t)SN_CLOCK_TICK_US, sn_clock_irq, NULL, &clock_timer));
    clock_timer_active = true;
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
static void sn_write(void *ctx, uint8_t chip, uint8_t value) {
    (void)ctx;
    bus_cycle(cs[chip + 1], false, chiptune_sn_gpio_word(value), sn_wait_us);
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
    if (clock_timer_active) {
        cancel_repeating_timer(&clock_timer);
        clock_timer_active = false;
    }
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
    sn_clock_start();
    rail_on = true;
    status.power = true;
    static Preset current;
    current = engine.preset;
    engine_init(&engine, &current, (SoundSink){saa_write, sn_write, NULL});
    engine.library = &settings;
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
    if (rail_on)
        engine_message(&engine, (uint8_t)source, data, len);
    if (source == 1 && engine.preset.route[1].forward && tud_mounted() && !usb_suspended) {
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
void tud_mount_cb(void) {
    usb_suspended = false;
}
void tud_umount_cb(void) {
    usb_suspended = false;
}
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    usb_suspended = tud_mounted();
}
void tud_resume_cb(void) {
    usb_suspended = false;
}
static void midi_tasks(uint32_t now) {
    static uint32_t previous_overflows;
    if (uart_overflows != previous_overflows) {
        previous_overflows = uart_overflows;
        if (engine.preset.route[1].forward)
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
            if (s == 1 && engine.preset.route[1].forward)
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
static float cv_note(unsigned input) {
    return cv_pitch(&settings, input, status.cv_raw[input], engine.preset.cv[input].base_note);
}
static void cv_event(unsigned input, bool gate) {
    if (engine.preset.studio.enabled) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        studio_cv(&engine, input, cv_volts(&settings, input, status.cv_raw[input]), gate,
                  status.adc_ok && cv_sequence[input] && now - cv_time[input] < 100);
        return;
    }
    if (engine.preset.cv_role[input] == CV_PITCH)
        engine_cv(&engine, input, cv_note(input), gate);
    else
        engine_cv_control(&engine, input, cv_volts(&settings, input, status.cv_raw[input]), gate);
}
static void gate_tasks(uint32_t now) {
    static uint32_t previous_overflow;
    static bool ready[2], previous_power;
    static uint32_t seen[2];
    static bool last_gate[2];
    static uint32_t poll_due;
    static bool poll_started;
    if (rail_on != previous_power) {
        ready[0] = ready[1] = false;
        previous_power = rail_on;
    }
    if (previous_overflow != gate_overflows) {
        previous_overflow = gate_overflows;
        engine_panic(&engine, -1);
        uint32_t irq = save_and_disable_interrupts();
        gate_tail = gate_head;
        restore_interrupts(irq);
    }
    while (gate_tail != gate_head) {
        uint8_t ev = gate_events[gate_tail];
        __dmb();
        gate_tail = (uint8_t)((gate_tail + 1) % GATE_RING);
        unsigned input = ev >> 1;
        last_gate[input] = ev & 1;
        if (rail_on &&
            (engine.preset.studio.enabled || (cv_sequence[input] && now - cv_time[input] < 100)))
            cv_event(input, last_gate[input]);
    }
    /* Queued gate edges above stay immediate. Poll CV/UI edits at 1kHz rather than
       repeating two floating-point pitch quantizers on every main-loop spin. */
    if (poll_started && (int32_t)(now - poll_due) < 0)
        return;
    poll_started = true;
    poll_due = now + 1;
    for (unsigned input = 0; input < 2; input++) {
        bool gate = !gpio_get(input ? PIN_GATE2_N : PIN_GATE1_N);
        status.gates[input] = gate;
        bool fresh = status.adc_ok && cv_sequence[input] && now - cv_time[input] < 100;
        if (rail_on) {
            if (engine.preset.studio.enabled) {
                studio_cv(&engine, input, cv_volts(&settings, input, status.cv_raw[input]), gate,
                          fresh);
            } else if (!fresh)
                engine_cv_clear(&engine, input);
            else if (!ready[input] || gate != last_gate[input])
                cv_event(input, gate);
            else if (seen[input] != cv_sequence[input]) {
                if (engine.preset.cv_role[input] == CV_PITCH)
                    engine_cv_pitch(&engine, input, cv_note(input));
                else
                    engine_cv_control(&engine, input,
                                      cv_volts(&settings, input, status.cv_raw[input]), gate);
            }
        }
        ready[input] = fresh;
        last_gate[input] = gate;
        seen[input] = cv_sequence[input];
    }
}

static bool calibrate(void *ctx, unsigned input, bool one) {
    (void)ctx;
    if (input >= 2 || cv_count[input] < 64 || !status.adc_ok ||
        cv_sequence[input] == cv_last_cal[input])
        return false;
    if (!cv_calibration(&settings, input, cv_samples[input], one, cal_zero_set))
        return false;
    cv_last_cal[input] = cv_sequence[input];
    engine_panic(&engine, -1);
    return true;
}
static bool save_settings(void *ctx) {
    (void)ctx;
    if (!rail_on || usb_suspended || cal_zero_set[0] || cal_zero_set[1] ||
        !settings_valid(&settings))
        return false;
    engine_panic(&engine, -1);
    static StoreRecord record;
    store_record(&record, &settings, store_sequence + 1);
    static uint8_t pages[STORE_BANK_BYTES];
    memset(pages, 0xff, sizeof(pages));
    memcpy(pages, &record, sizeof(record));
    /* Commit marker is programmed last; the other bank survives any interrupted write. */
    memset(pages, 0xff, 4);
    unsigned dest = store_bank == 0 ? 1 : 0;
    uint32_t offset = STORE_FLASH_OFFSET + dest * STORE_BANK_BYTES;
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
    /* Flash erase stalls MIDI: flush partial messages and notes. No autosave during a performance.
     */
    irq = save_and_disable_interrupts();
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
    gpio_init(PIN_GATE1_N);
    gpio_pull_up(PIN_GATE1_N);
    gpio_init(PIN_GATE2_N);
    gpio_pull_up(PIN_GATE2_N);
    gpio_set_irq_enabled_with_callback(PIN_GATE1_N, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true,
                                       gate_irq_handler);
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
    const StoreRecord *banks = (const StoreRecord *)(XIP_BASE + STORE_FLASH_OFFSET);
    store_bank =
        store_select(&banks[0], (const StoreRecord *)((const uint8_t *)banks + STORE_BANK_BYTES));
    if (store_bank >= 0) {
        const StoreRecord *record =
            (const StoreRecord *)((const uint8_t *)banks + store_bank * STORE_BANK_BYTES);
        hard_assert(store_load(record, &settings, &store_sequence));
    } else {
        /* v1-v4 used the last two 4KiB sectors. Never erase them before a v5 commit. */
        const StoreRecord *old_a = (const StoreRecord *)(XIP_BASE + STORE_LEGACY_OFFSET);
        const StoreRecord *old_b = (const StoreRecord *)(XIP_BASE + STORE_LEGACY_OFFSET + 4096);
        int legacy = store_select(old_a, old_b);
        if (legacy >= 0)
            hard_assert(store_load(legacy ? old_b : old_a, &settings, &store_sequence));
        else
            settings_default(&settings);
    }
    engine_init(&engine, &settings.preset[settings.selected],
                (SoundSink){saa_write, sn_write, NULL});
    for (int s = 0; s < 2; s++)
        midi_init(&parsers[s], midi_received, &source_id[s]);
    ui_init(&ui, &settings, &engine, save_settings, calibrate, NULL);
    oled_init(&oled, ui.frame, oled_write, oled_render, NULL);
    clock_sm = pio_claim_unused_sm(clock_pio, true);
    clock_offset = pio_add_program(clock_pio, &sn_clock_program);
    tusb_init();
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
        status.clock_hz = rail_on ? sn_clock_hz(last_clock_half) : 0;
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
        gate_tasks(now);
        if (rail_on)
            sn_clock_control();
        if (now_us - tick >= 1000) {
            uint32_t elapsed = (now_us - tick) / 1000;
            tick += elapsed * 1000;
            uint32_t start = time_us_32();
            if (rail_on)
                engine_tick(&engine, elapsed);
            uint32_t duration = time_us_32() - start;
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
            oled_task(&oled, to_ms_since_boot(get_absolute_time()), rail_on, settings.brightness);
        }
        gpio_put(PIN_STATUS_LED, rail_on && (engine_active(&engine) > 0 || (now % 2000) < 50));
        tight_loop_contents();
    }
}
