#pragma once

#include <array>
#include <cstdint>

namespace harmonia::rp2350 {

/// Driver ADC de 12 bits (0..4095, 0..3.3V) para RP2350 (Pico 2).
/// Canais de usuário no RP2350 QFN-60: ADC0=GP26, ADC1=GP27, ADC2=GP28.
class AdcDriver {
public:
    static constexpr std::uint8_t kAdcBaseGpio = 26;
    static constexpr std::uint8_t kMaxUserChannels = 3;

    explicit AdcDriver(std::uint32_t mux_settle_us = 10) noexcept;

    void init_gpio_channels(bool ch0_gp26, bool ch1_gp27, bool ch2_gp28 = false) noexcept;

    [[nodiscard]] std::uint16_t read_channel(std::uint8_t adc_input) const noexcept;

private:
    std::uint32_t mux_settle_us_;
};

} // namespace harmonia::rp2350
