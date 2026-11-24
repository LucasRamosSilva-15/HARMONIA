#include "pico/unique_id.h"
#include "tusb.h"

#include <cstring>

namespace {

// VID/PID de desenvolvimento/prototipo (Raspberry Pi / TinyUSB)
constexpr uint16_t kUsbVid = 0x2E8Au;
constexpr uint16_t kUsbPid = 0x000Au;
constexpr uint16_t kUsbBcd = 0x0200u;

enum {
    ITF_NUM_MIDI = 0,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_TOTAL
};

constexpr uint8_t kEpMidiOut = 0x01u;
constexpr uint8_t kEpMidiIn  = 0x81u;
constexpr uint16_t kConfigTotalLen = TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN;

constexpr tusb_desc_device_t kDeviceDescriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = kUsbBcd,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = kUsbVid,
    .idProduct          = kUsbPid,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

constexpr uint8_t kConfigDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, kConfigTotalLen, 0x00, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 0, kEpMidiOut, kEpMidiIn, 64)
};

constexpr const char* kStringDescArr[] = {
    (const char[]){0x09, 0x04},          // 0: Idioma Ingles (US)
    "HARMONIA",                          // 1: Fabricante
    "HARMONIA MIDI 2.0 Controller",      // 2: Produto
    nullptr                              // 3: Serial (preenchido via pico_unique_id)
};

uint16_t g_desc_str[32];

} // namespace

extern "C" {

uint8_t const* tud_descriptor_device_cb(void) {
    return reinterpret_cast<uint8_t const*>(&kDeviceDescriptor);
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return kConfigDescriptor;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count = 0;

    if (index == 0) {
        std::memcpy(&g_desc_str[1], kStringDescArr[0], 2);
        chr_count = 1;
    } else if (index == 3) {
        char serial_buf[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1]{};
        pico_get_unique_board_id_string(serial_buf, sizeof(serial_buf));
        chr_count = static_cast<uint8_t>(std::strlen(serial_buf));
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; ++i) {
            g_desc_str[1 + i] = static_cast<uint16_t>(serial_buf[i]);
        }
    } else {
        if (index >= (sizeof(kStringDescArr) / sizeof(kStringDescArr[0]))) {
            return nullptr;
        }
        const char* str = kStringDescArr[index];
        if (str == nullptr) {
            return nullptr;
        }
        chr_count = static_cast<uint8_t>(std::strlen(str));
        if (chr_count > 31) {
            chr_count = 31;
        }
        for (uint8_t i = 0; i < chr_count; ++i) {
            g_desc_str[1 + i] = static_cast<uint16_t>(str[i]);
        }
    }

    g_desc_str[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return g_desc_str;
}

} // extern "C"