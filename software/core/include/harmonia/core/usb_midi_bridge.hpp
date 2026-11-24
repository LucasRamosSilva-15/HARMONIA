#pragma once

#include "harmonia/core/ump.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Estado negociado pelo host USB para a interface Audio/MIDI.
enum class UsbAltSetting : std::int8_t {
    Disconnected = -1,
    Midi1Fallback = 0,
    Midi2Native = 1
};

/// Quadro de saída pronto para transmissão USB MIDI (MIDI 2.0 UMP ou MIDI 1.0 USB-MIDI).
struct UsbMidiFrame {
    UsbAltSetting mode{UsbAltSetting::Disconnected};
    UmpPacket ump{};
    std::array<std::uint8_t, 4> midi1_usb_packet{};
};

/// Adaptador portátil (C++17, zero heap) entre pacotes UMP canônicos do núcleo
/// e o transporte físico USB MIDI (TinyUSB Alt 0 MIDI 1.0 ou Alt 1 MIDI 2.0 UMP).
class UsbMidiBridge {
public:
    constexpr explicit UsbMidiBridge(std::uint8_t cable_number = 0) noexcept
        : cable_number_(cable_number <= kMaxUmpGroup ? cable_number : 0) {}

    constexpr void set_alt_setting(UsbAltSetting alt) noexcept {
        alt_setting_ = alt;
    }

    [[nodiscard]] constexpr UsbAltSetting alt_setting() const noexcept {
        return alt_setting_;
    }

    [[nodiscard]] constexpr bool is_mounted() const noexcept {
        return alt_setting_ != UsbAltSetting::Disconnected;
    }

    /// Prepara um pacote UMP canônico para envio no modo USB negociado.
    /// Retorna std::nullopt e contabiliza descarte se desconectado ou pacote inválido.
    [[nodiscard]] std::optional<UsbMidiFrame> prepare_tx(const UmpPacket& canonical_ump) noexcept;

    /// Registra o resultado real da tentativa não bloqueante de escrita no endpoint USB.
    constexpr void record_tx_result(bool sent) noexcept {
        if (sent) {
            ++tx_ok_count_;
        } else {
            ++drop_count_;
        }
    }

    /// Decodifica um pacote UMP recebido do host USB (loopback/controle).
    [[nodiscard]] UmpDecodeResult receive_ump(const UmpPacket& rx_packet) noexcept;

    /// Converte e decodifica um pacote USB-MIDI 1.0 de 4 bytes recebido via tud_midi_packet_read.
    [[nodiscard]] UmpDecodeResult receive_midi1_usb_packet(const std::uint8_t pkt4[4]) noexcept;

    [[nodiscard]] constexpr std::uint32_t tx_ok_count() const noexcept { return tx_ok_count_; }
    [[nodiscard]] constexpr std::uint32_t drop_count() const noexcept { return drop_count_; }
    [[nodiscard]] constexpr std::uint32_t rx_ok_count() const noexcept { return rx_ok_count_; }
    [[nodiscard]] constexpr std::uint32_t rx_error_count() const noexcept { return rx_error_count_; }

    constexpr void reset_counters() noexcept {
        tx_ok_count_ = 0;
        drop_count_ = 0;
        rx_ok_count_ = 0;
        rx_error_count_ = 0;
    }

private:
    UsbAltSetting alt_setting_{UsbAltSetting::Disconnected};
    std::uint8_t cable_number_{0};
    std::uint32_t tx_ok_count_{0};
    std::uint32_t drop_count_{0};
    std::uint32_t rx_ok_count_{0};
    std::uint32_t rx_error_count_{0};
};

} // namespace harmonia::core