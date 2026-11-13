#include "adc_driver.hpp"

#include "hardware/adc.h"
#include "pico/time.h"

namespace harmonia::rp2350 {

AdcDriver::AdcDriver(std::uint32_t mux_settle_us) noexcept
    : mux_settle_us_(mux_settle_us) {}

void AdcDriver::init_gpio_channels(bool ch0_gp26, bool ch1_gp27, bool ch2_gp28) noexcept {
    adc_init();
    if (ch0_gp26) {
        adc_gpio_init(kAdcBaseGpio + 0);
    }
    if (ch1_gp27) {
        adc_gpio_init(kAdcBaseGpio + 1);
    }
    if (ch2_gp28) {
        adc_gpio_init(kAdcBaseGpio + 2);
    }
}

std::uint16_t AdcDriver::read_channel(std::uint8_t adc_input) const noexcept {
    if (adc_input >= kMaxUserChannels) {
        return 0;
    }
    adc_select_input(adc_input);
    if (mux_settle_us_ > 0) {
        busy_wait_us_32(mux_settle_us_);
    }
    return adc_read();
}

} // namespace harmonia::rp2350
