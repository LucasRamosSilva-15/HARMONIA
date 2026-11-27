#include "uart_link_driver.hpp"

#include "hardware/gpio.h"
#include "hardware/uart.h"

namespace harmonia::rp2350 {

UartLinkDriver::UartLinkDriver(
    std::uint8_t tx_pin,
    std::uint8_t rx_pin,
    std::uint32_t baud_rate
) noexcept
    : tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate) {}

void UartLinkDriver::init() noexcept {
    actual_baud_ = uart_init(uart1, baud_rate_);
    gpio_set_function(tx_pin_, GPIO_FUNC_UART);
    gpio_set_function(rx_pin_, GPIO_FUNC_UART);
    uart_set_format(uart1, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart1, true);
}

bool UartLinkDriver::try_send_frame(const std::uint8_t* data, std::size_t len) noexcept {
    if (data == nullptr || len == 0u || (kTxRingCapacity - tx_count_) < len) {
        ++tx_drop_count_;
        return false;
    }

    for (std::size_t i = 0; i < len; ++i) {
        tx_buf_[tx_tail_] = data[i];
        tx_tail_ = (tx_tail_ + 1u) % kTxRingCapacity;
        ++tx_count_;
    }

    poll_tx();
    ++tx_ok_count_;
    return true;
}

void UartLinkDriver::poll_tx() noexcept {
    while (tx_count_ > 0u && uart_is_writable(uart1)) {
        uart_putc_raw(uart1, static_cast<char>(tx_buf_[tx_head_]));
        tx_head_ = (tx_head_ + 1u) % kTxRingCapacity;
        --tx_count_;
    }
}

void UartLinkDriver::poll_rx(harmonia::core::LinkFrameDecoder& decoder) noexcept {
    while (uart_is_readable(uart1)) {
        const std::uint8_t b = static_cast<std::uint8_t>(uart_getc(uart1));
        (void)decoder.feed_byte(b);
    }
}

} // namespace harmonia::rp2350