#include <array>
#include <cstdint>
#include <cstdio>

#include "adc_driver.hpp"
#include "gpio_matrix_driver.hpp"
#include "uart_link_driver.hpp"
#include "harmonia/core/analog_control.hpp"
#include "harmonia/core/key_matrix.hpp"
#include "harmonia/core/key_scanner.hpp"
#include "harmonia/core/link_protocol.hpp"
#include "harmonia/core/midi_event.hpp"
#include "harmonia/core/ump.hpp"
#include "harmonia/core/usb_midi_bridge.hpp"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "tusb.h"

namespace {

constexpr std::array<std::uint8_t, 2> kRowPins{10, 11};
constexpr std::array<std::uint8_t, 2> kColPins{14, 15};

constexpr std::uint32_t kScanIntervalUs = 1000;          // 1 kHz matriz digital
constexpr std::uint32_t kDebounceUs = 20000;             // 20 ms (diagnóstico inicial)
constexpr std::uint32_t kAdcPollIntervalUs = 5000;       // 5 ms por leitura alternada (100 Hz/canal)
constexpr std::uint32_t kHeartbeatIntervalUs = 2000000;  // 2 s

constexpr std::uint8_t kMidiChannel = 0;
constexpr std::uint8_t kBaseNote = 60;
constexpr std::uint16_t kFixedVelocity = 40000;
constexpr std::uint8_t kAdcFilterShift = 2;              // alpha = 1/4 = 0.25
constexpr std::uint16_t kAdcHysteresisRaw = 24;

volatile bool g_usb_mounted = false;

} // namespace

extern "C" {

void tud_mount_cb(void) {
    g_usb_mounted = true;
}

void tud_umount_cb(void) {
    g_usb_mounted = false;
}

void tud_midi_rx_cb(uint8_t itf) {
    (void)itf;
}

} // extern "C"

int main() {
    stdio_init_all();
    tud_init(BOARD_TUD_RHPORT);

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
    printf("[ADC]  GP26 (ADC0->CC7 Vol) | GP27 (ADC1->CC11 Expr) | 100 Hz/ch | shift=%u | Hyst=%u\n",
           static_cast<unsigned>(kAdcFilterShift),
           static_cast<unsigned>(kAdcHysteresisRaw));

    harmonia::rp2350::UartLinkDriver uart_link;
    uart_link.init();
    printf("[LINK] UART1 GP4(TX)/GP5(RX) @ %lu bps | COBS + CRC-16\n",
           static_cast<unsigned long>(uart_link.actual_baud()));

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
        kMidiChannel, 7, harmonia::core::AnalogCalibration{0, 4095, 4095, false}, kAdcFilterShift, kAdcHysteresisRaw
    };
    const harmonia::core::AnalogChannelConfig ch1_cfg{
        kMidiChannel, 11, harmonia::core::AnalogCalibration{0, 4095, 4095, false}, kAdcFilterShift, kAdcHysteresisRaw
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
    harmonia::core::UmpRouter ump_router(0, harmonia::core::UmpProtocolMode::Midi2);
    harmonia::core::UmpPacketQueue<32> ump_queue;
    harmonia::core::UsbMidiBridge usb_bridge(0);
    harmonia::core::LinkFrameDecoder link_rx_decoder;
    std::uint8_t link_seq = 0;
    std::uint32_t next_scan_us = time_us_32();
    std::uint32_t next_adc_us = next_scan_us + 500; // defasado 500 us da borda do scan digital
    std::uint32_t last_heartbeat_us = next_scan_us;
    std::uint32_t heartbeat_count = 0;
    std::uint8_t active_adc_ch = 0;
    bool led_state = false;

    std::uint32_t prev_scan_start_us = 0;
    bool has_prev_scan = false;
    std::uint32_t max_scan_dt_us = 0;
    std::uint32_t max_scan_late_us = 0;
    std::uint32_t max_adc_proc_us = 0;
    std::uint32_t max_adc_late_us = 0;
    std::uint32_t adc_events_emitted = 0;

    while (true) {
        tud_task();
        uart_link.poll_tx();
        uart_link.poll_rx(link_rx_decoder);
        const bool usb_ready = g_usb_mounted && tud_midi_mounted();
        usb_bridge.set_alt_setting(
            usb_ready ? harmonia::core::UsbAltSetting::Midi1Fallback
                      : harmonia::core::UsbAltSetting::Disconnected
        );

        const std::uint32_t now_us = time_us_32();

        if (static_cast<std::int32_t>(now_us - next_scan_us) >= 0) {
            const std::uint32_t scan_late_us = now_us - next_scan_us;
            if (scan_late_us > max_scan_late_us) {
                max_scan_late_us = scan_late_us;
            }
            if (has_prev_scan) {
                const std::uint32_t scan_dt_us = now_us - prev_scan_start_us;
                if (scan_dt_us > max_scan_dt_us) {
                    max_scan_dt_us = scan_dt_us;
                }
            } else {
                has_prev_scan = true;
            }
            prev_scan_start_us = now_us;
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
                (void)ump_router.route_note(ev, ump_queue);
            }
        }

        if (static_cast<std::int32_t>(now_us - next_adc_us) >= 0) {
            const std::uint32_t adc_late_us = now_us - next_adc_us;
            if (adc_late_us > max_adc_late_us) {
                max_adc_late_us = adc_late_us;
            }
            next_adc_us += kAdcPollIntervalUs;

            const std::uint32_t adc_start_us = time_us_32();
            const std::uint16_t raw = adc_driver.read_channel(active_adc_ch);
            const auto ctrl = analog_bank.update_channel(active_adc_ch, raw);
            const std::uint32_t adc_proc_us = time_us_32() - adc_start_us;
            if (adc_proc_us > max_adc_proc_us) {
                max_adc_proc_us = adc_proc_us;
            }

            if (ctrl.has_value()) {
                ++adc_events_emitted;
                printf("[ADC_CC]   [%lu us] ch=%u cc=%u raw=%u midi7=%u val32=%lu\n",
                       static_cast<unsigned long>(now_us),
                       static_cast<unsigned>(ctrl->channel),
                       static_cast<unsigned>(ctrl->cc_number),
                       static_cast<unsigned>(ctrl->raw_filtered),
                       static_cast<unsigned>(harmonia::core::to_midi1_7bit(ctrl->value_32)),
                       static_cast<unsigned long>(ctrl->value_32));
                (void)ump_router.route_control(*ctrl, ump_queue);
            }

            active_adc_ch = static_cast<std::uint8_t>((active_adc_ch + 1u) % 2u);
        }

        // Drena ate 4 pacotes UMP por iteracao para diagnostico UART0 e envio USB MIDI nao bloqueante
        for (int drained = 0; drained < 4 && !ump_queue.empty(); ++drained) {
            const auto pkt = ump_queue.pop();
            if (!pkt.has_value()) {
                break;
            }
            if (pkt->word_count == 2u) {
                printf("[UMP] mt=%u group=%u words=2 data=%08lX %08lX\n",
                       static_cast<unsigned>(pkt->message_type()),
                       static_cast<unsigned>(pkt->group()),
                       static_cast<unsigned long>(pkt->words[0]),
                       static_cast<unsigned long>(pkt->words[1]));
            } else if (pkt->word_count == 1u) {
                printf("[UMP] mt=%u group=%u words=1 data=%08lX\n",
                       static_cast<unsigned>(pkt->message_type()),
                       static_cast<unsigned>(pkt->group()),
                       static_cast<unsigned long>(pkt->words[0]));
            }

            const auto usb_frame = usb_bridge.prepare_tx(*pkt);
            if (usb_frame.has_value() && usb_ready) {
                const bool sent = tud_midi_packet_write(usb_frame->midi1_usb_packet.data());
                usb_bridge.record_tx_result(sent);
            }

            std::array<std::uint8_t, harmonia::core::kLinkMaxEncodedFrameBytes> link_frame{};
            const std::size_t frame_len = harmonia::core::encode_ump_frame(
                *pkt, link_seq++, link_frame.data(), link_frame.size()
            );
            if (frame_len > 0u) {
                (void)uart_link.try_send_frame(link_frame.data(), frame_len);
            }
        }

        // Leitura nao bloqueante de pacotes vindos do host USB
        while (usb_ready && tud_midi_available() > 0) {
            std::uint8_t rx_pkt[4]{};
            if (!tud_midi_packet_read(rx_pkt)) {
                break;
            }
            (void)usb_bridge.receive_midi1_usb_packet(rx_pkt);
        }

        if (static_cast<std::uint32_t>(now_us - last_heartbeat_us) >= kHeartbeatIntervalUs) {
            last_heartbeat_us = now_us;
            ++heartbeat_count;
            led_state = !led_state;
#ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, led_state);
#endif
            std::array<std::uint8_t, harmonia::core::kLinkMaxEncodedFrameBytes> hb_frame{};
            const std::size_t hb_len = harmonia::core::encode_heartbeat_frame(
                link_seq++, heartbeat_count, hb_frame.data(), hb_frame.size()
            );
            if (hb_len > 0u) {
                (void)uart_link.try_send_frame(hb_frame.data(), hb_len);
            }
            printf("[HEARTBEAT] [%lu us] #%lu\n",
                   static_cast<unsigned long>(now_us),
                   static_cast<unsigned long>(heartbeat_count));
            printf("[METRICS] [%lu us] max_scan_dt=%lu max_scan_late=%lu max_adc_proc=%lu max_adc_late=%lu adc_events=%lu ump_drops=%lu usb_mounted=%u usb_alt=%d usb_tx_ok=%lu usb_drops=%lu link_tx_ok=%lu link_drops=%lu link_crc_err=%lu\n",
                   static_cast<unsigned long>(now_us),
                   static_cast<unsigned long>(max_scan_dt_us),
                   static_cast<unsigned long>(max_scan_late_us),
                   static_cast<unsigned long>(max_adc_proc_us),
                   static_cast<unsigned long>(max_adc_late_us),
                   static_cast<unsigned long>(adc_events_emitted),
                   static_cast<unsigned long>(ump_queue.dropped_count()),
                   static_cast<unsigned>(usb_bridge.is_mounted() ? 1u : 0u),
                   static_cast<int>(usb_bridge.alt_setting()),
                   static_cast<unsigned long>(usb_bridge.tx_ok_count()),
                   static_cast<unsigned long>(usb_bridge.drop_count()),
                   static_cast<unsigned long>(uart_link.tx_ok_count()),
                   static_cast<unsigned long>(uart_link.tx_drop_count()),
                   static_cast<unsigned long>(link_rx_decoder.stats().crc_errors));
        }
    }

    return 0;
}
