#pragma once

#include "harmonia/core/key_matrix.hpp"

#include <array>
#include <cstdint>

namespace harmonia::rp2350 {

/// Driver de varredura matricial 2x2 para GPIO do RP2350.
/// Estratégia:
/// - Colunas configuradas como entrada com pull-up interno (ativo-baixo).
/// - Linha selecionada colocada em saída LOW (0 V); linhas não selecionadas
///   mantidas em alta impedância (GPIO_IN sem pull) para evitar conflitos elétricos.
class GpioMatrixDriver2x2 {
public:
    GpioMatrixDriver2x2(
        std::array<std::uint8_t, 2> row_pins,
        std::array<std::uint8_t, 2> col_pins,
        std::uint32_t settle_us = 5
    ) noexcept;

    void init() noexcept;

    void read_snapshot(harmonia::core::KeyMatrixState<2, 2>& snapshot) const noexcept;

private:
    std::array<std::uint8_t, 2> row_pins_;
    std::array<std::uint8_t, 2> col_pins_;
    std::uint32_t settle_us_;
};

} // namespace harmonia::rp2350
