#pragma once

#include "harmonia/core/key_matrix.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace harmonia::core {

/// Estado interno de debounce por tecla.
struct DebounceSlot {
    bool last_raw{false};
    bool stable{false};
    std::uint32_t last_change_us{0};
    bool pending_change{false};
};

/// Debouncer determinístico baseado em timestamps em microssegundos (uint32_t).
/// Independente de frequência de CPU e de intervalos regulares de amostragem.
/// Não utiliza sleep(), threads ou alocação dinâmica.
template <std::size_t MaxKeys>
class KeyDebouncer {
public:
    static constexpr std::uint32_t kDefaultDebounceUs = 20000; // 20 ms

    constexpr explicit KeyDebouncer(std::uint32_t debounce_us = kDefaultDebounceUs) noexcept
        : debounce_us_(debounce_us) {}

    constexpr void set_debounce_us(std::uint32_t debounce_us) noexcept {
        debounce_us_ = debounce_us;
    }

    [[nodiscard]] constexpr std::uint32_t debounce_us() const noexcept {
        return debounce_us_;
    }

    /// Reinicia todas as teclas para um estado inicial definido (padrão: solto).
    constexpr void reset(bool initial_pressed = false) noexcept {
        for (auto& slot : slots_) {
            slot.last_raw = initial_pressed;
            slot.stable = initial_pressed;
            slot.last_change_us = 0;
            slot.pending_change = false;
        }
    }

    /// Atualiza o estado bruto de uma tecla no instante `now_us`.
    /// Retorna KeyTransition::Pressed ou KeyTransition::Released apenas quando
    /// o novo estado permanecer estável por pelo menos `debounce_us_`.
    [[nodiscard]] constexpr KeyTransition update(
        std::size_t key_index,
        bool raw_pressed,
        std::uint32_t now_us
    ) noexcept {
        if (key_index >= MaxKeys) {
            return KeyTransition::None;
        }

        auto& slot = slots_[key_index];

        if (raw_pressed != slot.last_raw) {
            slot.last_raw = raw_pressed;
            slot.last_change_us = now_us;
            slot.pending_change = (raw_pressed != slot.stable);
        }

        if (slot.pending_change) {
            const std::uint32_t elapsed = static_cast<std::uint32_t>(now_us - slot.last_change_us);
            if (elapsed >= debounce_us_) {
                slot.stable = slot.last_raw;
                slot.pending_change = false;
                return slot.stable ? KeyTransition::Pressed : KeyTransition::Released;
            }
        }

        return KeyTransition::None;
    }

    [[nodiscard]] constexpr bool is_pressed(std::size_t key_index) const noexcept {
        if (key_index >= MaxKeys) {
            return false;
        }
        return slots_[key_index].stable;
    }

private:
    std::uint32_t debounce_us_{kDefaultDebounceUs};
    std::array<DebounceSlot, MaxKeys> slots_{};
};

} // namespace harmonia::core
