#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Configuracao TinyUSB Device (oficial do Pico SDK) para HARMONIA RP2350.
// Nota: TinyUSB 0.17.0 empacotado no Pico SDK fornece apenas CFG_TUD_MIDI (USB MIDI 1.0).
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT        0
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS             OPT_OS_PICO
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))
#endif

#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_FULL_SPEED
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#define CFG_TUD_ENDPOINT0_SIZE  64

// Apenas classe USB MIDI habilitada (sem CDC/MSC/HID para nao conflitar com stdio_uart)
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            1
#define CFG_TUD_VENDOR          0

#define CFG_TUD_MIDI_RX_BUFSIZE 64
#define CFG_TUD_MIDI_TX_BUFSIZE 64
#define CFG_TUD_MIDI_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif