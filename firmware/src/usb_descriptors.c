#include "tusb.h"
#include "pico/unique_id.h"
#include <string.h>
static const tusb_desc_device_t device = {.bLength = sizeof(tusb_desc_device_t),
                                          .bDescriptorType = TUSB_DESC_DEVICE,
                                          .bcdUSB = 0x0200,
                                          .bDeviceClass = 0,
                                          .bDeviceSubClass = 0,
                                          .bDeviceProtocol = 0,
                                          .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
                                          .idVendor = CHIPTUNE_USB_VID,
                                          .idProduct = CHIPTUNE_USB_PID,
                                          .bcdDevice = 0x0080,
                                          .iManufacturer = 1,
                                          .iProduct = 2,
                                          .iSerialNumber = 3,
                                          .bNumConfigurations = 1};
static const uint8_t config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 2, 0, TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN, 0x00, 500),
    TUD_MIDI_DESCRIPTOR(0, 0, 0x01, 0x81, 64)};
const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&device;
}
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return config;
}
const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t buffer[64];
    static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *text = NULL;
    if (index == 0) {
        buffer[0] = (TUSB_DESC_STRING << 8) | 4;
        buffer[1] = 0x0409;
        return buffer;
    }
    if (index == 1)
        text = "Chiptune USB/CV";
    if (index == 2)
        text = "SAA1099 + Dual SN76489AN";
    if (index == 3) {
        pico_get_unique_board_id_string(serial, sizeof(serial));
        text = serial;
    }
    if (!text)
        return NULL;
    size_t n = strlen(text);
    if (n > 63)
        n = 63;
    for (size_t i = 0; i < n; i++)
        buffer[i + 1] = (uint8_t)text[i];
    buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * n + 2));
    return buffer;
}
