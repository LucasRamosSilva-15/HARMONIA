#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "harmonia/core/link_protocol.hpp"

namespace harmonia::rp2350 {

/// Driver nao bloqueante para enlace binario dedicado RP2350 <-> Raspberry Pi 5 via UART1.
/// Padrao: GP4 (TX) / GP5 (RX), 921600 bps, 8N1 (preserva UART0 GP0/GP1 para diagnostico).
class UartLinkDriver {
public:
    static constexpr std::uint8_t kDefaultTxPin = 4;
    static constexpr std::uint8_t kDefaultRxPin = 5;
    static constexpr std::uint32_t kDefaultBaud = 921600;
    static constexpr std::size_t kTxRingCapacity = 256;

    explicit UartLinkDriver(
        std::uint8_t tx_pin = kDefaultTxPin,
        std::uint8_t rx_pin = kDefaultRxPin,
        std::uint32_t baud_rate = kDefaultBaud
    ) noexcept;

    void init() noexcept;

    /// Enfileira quadro binario no buffer TX estatico e drena o maximo possivel para o FIFO PL011.
    /// Retorna false sem bloquear se o buffer TX nao comportar o quadro completo.
    bool try_send_frame(const std::uint8_t* data, std::size_t len) noexcept;

    /// Bomba nao bloqueante do buffer TX para o hardware UART1.
    void poll_tx() noexcept;

    /// Leitura nao bloqueante do FIFO RX UART1 alimentando o LinkFrameDecoder.
    void poll_rx(harmonia::core::LinkFrameDecoder& decoder) noexcept;

    [[nodiscard]] std::uint32_t tx_ok_count() const noexcept { return tx_ok_count_; }
    [[nodiscard]] std::uint32_t tx_drop_count() const noexcept { return tx_drop_count_; }
    [[nodiscard]] std::uint32_t actual_baud() const noexcept { return actual_baud_; }

private:
    std::uint8_t tx_pin_{kDefaultTxPin};
    std::uint8_t rx_pin_{kDefaultRxPin};
    std::uint32_t baud_rate_{kDefaultBaud};
    std::uint32_t actual_baud_{0};

    std::array<std::uint8_t, kTxRingCapacity> tx_buf_{};
    std::size_t tx_head_{0};
    std::size_t tx_tail_{0};
    std::size_t tx_count_{0};

    std::uint32_t tx_ok_count_{0};
    std::uint32_t tx_drop_count_{0};
};

} // namespace harmonia::rp2350