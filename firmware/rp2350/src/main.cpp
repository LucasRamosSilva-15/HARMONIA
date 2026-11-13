#include <array>
#include <cstdint>
#include <cstdio>

#include "adc_driver.hpp"
#include "gpio_matrix_driver.hpp"
#include "harmonia/core/analog_control.hpp"
#include "harmonia/core/key_matrix.hpp"
#include "harmonia/core/key_scanner.hpp"
#include "harmonia/core/midi_event.hpp"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace {

constexpr std::array<std::uint8_t, 2> kRowPins{10, 11};
constexpr std::array<std::uint8_t, 2> kColPins{14, 15};

constexpr std::uint32_t kScanIntervalUs = 1000;          // 1 kHz matriz digital
constexpr std::uint32_t kDebounceUs = 20000;             // 20 ms (diagnóstico inicial)
constexpr std::uint32_t kAdcPollIntervalUs = 10000;      // 10 ms por leitura alternada (50 Hz/canal)
constexpr std::uint32_t kHeartbeatIntervalUs = 2000000;  // 2 s

constexpr std::uint8_t kMidiChannel = 0;
constexpr std::uint8_t kBaseNote = 60;
constexpr std::uint16_t kFixedVelocity = 40000;
constexpr std::uint16_t kAdcHysteresisRaw = 24;

} // namespace

int main() {
    stdio_init_all();

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
#endif

    printf("\n================================================\n");
    printf("[BOOT] HARMONIA RP2350 - Etapa 04 (Matriz 2x2 + ADC)\n");
    printf("[BOOT] Single-core | C++17 | UART0 (GP0/GP1)\n");

    harmonia::rp2350::GpioMatrixDriver2x2 matrix_driver(kRowPins, kColPins, 5);
    matrix_driver.init();
    printf("[GPIO] Rows: GP%u, GP%u (Hi-Z / active-low) | Cols: GP%u, GP%u (pull-up)\n",
           kRowPins[0], kRowPins[1], kColPins[0], kColPins[1]);

    harmonia::rp2350::AdcDriver adc_driver(10);
    adc_driver.init_gpio_channels(true, true, false);
    printf("[ADC]  GP26 (ADC0->CC7 Vol) | GP27 (ADC1->CC11 Expr) | 50 Hz/ch | Hyst=%u\n",
           static_cast<unsigned>(kAdcHysteresisRaw));

    harmonia::core::KeyMatrixState<2, 2> snapshot(2, 2);
    harmonia::core::KeyScanner<2, 2> scanner;
    scanner.debouncer().set_debounce_us(kDebounceUs);
    scanner.mapper().set_channel(kMidiChannel);
    scanner.mapper().set_fixed_velocity(kFixedVelocity);

    if (!scanner.mapper().assign_chromatic(kBaseNote, 4)) {
        printf("[ERROR] Falha ao configurar mapeamento cromático de notas\n");
    }

    harmonia::core::AnalogBank<2> analog_bank;
    const harmonia::core::AnalogChannelConfig ch0_cfg{
        kMidiChannel, 7, harmonia::core::AnalogCalibration{0, 4095, 4095, false}, 3, kAdcHysteresisRaw
    };
    const harmonia::core::AnalogChannelConfig ch1_cfg{
        kMidiChannel, 11, harmonia::core::AnalogCalibration{0, 4095, 4095, false}, 3, kAdcHysteresisRaw
    };
    if (!analog_bank.configure_channel(0, ch0_cfg) || !analog_bank.configure_channel(1, ch1_cfg)) {
        printf("[ERROR] Falha ao configurar canais analógicos\n");
    }

    printf("[SCAN] Interval: %lu us | Debounce: %lu us | Notes: %u..%u | Vel: %u\n",
           static_cast<unsigned long>(kScanIntervalUs),
           static_cast<unsigned long>(kDebounceUs),
           static_cast<unsigned>(kBaseNote),
           static_cast<unsigned>(kBaseNote + 3),
           static_cast<unsigned>(kFixedVelocity));
    printf("================================================\n");

    std::array<harmonia::core::NoteEvent, 8> events{};
    std::uint32_t next_scan_us = time_us_32();
    std::uint32_t next_adc_us = next_scan_us + 500; // defasado 500 us da borda do scan digital
    std::uint32_t last_heartbeat_us = next_scan_us;
    std::uint32_t heartbeat_count = 0;
    std::uint8_t active_adc_ch = 0;
    bool led_state = false;

    while (true) {
        const std::uint32_t now_us = time_us_32();

        if (static_cast<std::int32_t>(now_us - next_scan_us) >= 0) {
            next_scan_us += kScanIntervalUs;

            matrix_driver.read_snapshot(snapshot);
            const auto res = scanner.scan_with_status(snapshot, now_us, events.data(), events.size());

            if (res.overflow) {
                printf("[WARN] [%lu us] Scan overflow! dropped_note_on=%u pending=%u\n",
                       static_cast<unsigned long>(now_us),
                       static_cast<unsigned>(res.dropped_note_on),
                       static_cast<unsigned>(res.pending_count));
            }

            for (std::size_t i = 0; i < res.emitted; ++i) {
                const auto& ev = events[i];
                if (ev.type == harmonia::core::EventType::NoteOn) {
                    printf("[NOTE_ON]  [%lu us] ch=%u note=%u vel=%u\n",
                           static_cast<unsigned long>(now_us),
                           static_cast<unsigned>(ev.channel),
                           static_cast<unsigned>(ev.note),
                           static_cast<unsigned>(ev.velocity));
                } else if (ev.type == harmonia::core::EventType::NoteOff) {
                    printf("[NOTE_OFF] [%lu us] ch=%u note=%u vel=%u\n",
                           static_cast<unsigned long>(now_us),
                           static_cast<unsigned>(ev.channel),
                           static_cast<unsigned>(ev.note),
                           static_cast<unsigned>(ev.velocity));
                }
            }
        }

        if (static_cast<std::int32_t>(now_us - next_adc_us) >= 0) {
            next_adc_us += kAdcPollIntervalUs;

            const std::uint16_t raw = adc_driver.read_channel(active_adc_ch);
            const auto ctrl = analog_bank.update_channel(active_adc_ch, raw);
            if (ctrl.has_value()) {
                printf("[ADC_CC]   [%lu us] ch=%u cc=%u raw=%u midi7=%u val32=%lu\n",
                       static_cast<unsigned long>(now_us),
                       static_cast<unsigned>(ctrl->channel),
                       static_cast<unsigned>(ctrl->cc_number),
                       static_cast<unsigned>(ctrl->raw_filtered),
                       static_cast<unsigned>(harmonia::core::to_midi1_7bit(ctrl->value_32)),
                       static_cast<unsigned long>(ctrl->value_32));
            }

            active_adc_ch = static_cast<std::uint8_t>((active_adc_ch + 1u) % 2u);
        }

        if (static_cast<std::uint32_t>(now_us - last_heartbeat_us) >= kHeartbeatIntervalUs) {
            last_heartbeat_us = now_us;
            ++heartbeat_count;
            led_state = !led_state;
#ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, led_state);
#endif
            printf("[HEARTBEAT] [%lu us] #%lu\n",
                   static_cast<unsigned long>(now_us),
                   static_cast<unsigned long>(heartbeat_count));
        }
    }

    return 0;
}
