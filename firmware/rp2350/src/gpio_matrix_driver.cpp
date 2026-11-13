#include "gpio_matrix_driver.hpp"

#include "hardware/gpio.h"
#include "pico/time.h"

namespace harmonia::rp2350 {

GpioMatrixDriver2x2::GpioMatrixDriver2x2(
    std::array<std::uint8_t, 2> row_pins,
    std::array<std::uint8_t, 2> col_pins,
    std::uint32_t settle_us
) noexcept
    : row_pins_(row_pins), col_pins_(col_pins), settle_us_(settle_us) {}

void GpioMatrixDriver2x2::init() noexcept {
    // Linhas iniciam em alta impedância (GPIO_IN sem pull)
    for (const std::uint8_t pin : row_pins_) {
        gpio_init(pin);
        gpio_disable_pulls(pin);
        gpio_set_dir(pin, GPIO_IN);
    }

    // Colunas iniciam em entrada com pull-up interno (repouso em nível HIGH)
    for (const std::uint8_t pin : col_pins_) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
}

void GpioMatrixDriver2x2::read_snapshot(
    harmonia::core::KeyMatrixState<2, 2>& snapshot
) const noexcept {
    for (std::uint8_t r = 0; r < 2; ++r) {
        const std::uint8_t row_pin = row_pins_[r];

        // Seleciona apenas esta linha em nível LOW
        gpio_set_dir(row_pin, GPIO_OUT);
        gpio_put(row_pin, 0);

        if (settle_us_ > 0) {
            busy_wait_us_32(settle_us_);
        }

        for (std::uint8_t c = 0; c < 2; ++c) {
            // Ativo-baixo: tecla pressionada puxa a coluna para LOW via diodo
            const bool pressed = !gpio_get(col_pins_[c]);
            snapshot.set_key(r, c, pressed);
        }

        // Retorna a linha para alta impedância antes de selecionar a próxima
        gpio_set_dir(row_pin, GPIO_IN);
    }
}

} // namespace harmonia::rp2350
