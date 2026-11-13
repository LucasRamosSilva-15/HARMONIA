#pragma once

#include "harmonia/core/key_debouncer.hpp"
#include "harmonia/core/key_mapper.hpp"
#include "harmonia/core/key_matrix.hpp"
#include "harmonia/core/midi_event.hpp"

#include <cstddef>
#include <cstdint>

namespace harmonia::core {

/// Orquestrador determinístico de aquisição lógica:
/// recebe um snapshot da matriz de teclas + timestamp (us), executa debounce
/// por tecla e emite eventos NoteEvent em buffer pré-alocado pelo chamador.
template <std::size_t MaxRows, std::size_t MaxCols>
class KeyScanner {
public:
    static constexpr std::size_t kMaxKeys = MaxRows * MaxCols;

    constexpr KeyScanner() noexcept = default;

    constexpr KeyDebouncer<kMaxKeys>& debouncer() noexcept { return debouncer_; }
    constexpr const KeyDebouncer<kMaxKeys>& debouncer() const noexcept { return debouncer_; }

    constexpr KeyMapper<kMaxKeys>& mapper() noexcept { return mapper_; }
    constexpr const KeyMapper<kMaxKeys>& mapper() const noexcept { return mapper_; }

    constexpr void reset(bool initial_pressed = false) noexcept {
        debouncer_.reset(initial_pressed);
    }

    /// Processa todas as teclas ativas da matriz no instante `now_us`.
    /// Retorna a quantidade de eventos escritos em `out_events` (limitada por `max_events`).
    [[nodiscard]] std::size_t scan(
        const KeyMatrixState<MaxRows, MaxCols>& matrix,
        std::uint32_t now_us,
        NoteEvent* out_events,
        std::size_t max_events
    ) noexcept {
        const std::size_t count = matrix.key_count();
        std::size_t emitted = 0;

        for (std::size_t idx = 0; idx < count; ++idx) {
            const bool raw = matrix.get_by_index(idx);
            const KeyTransition tr = debouncer_.update(idx, raw, now_us);
            if (tr == KeyTransition::None) {
                continue;
            }
            const auto ev = mapper_.map_transition(idx, tr);
            if (ev.has_value() && out_events != nullptr && emitted < max_events) {
                out_events[emitted] = *ev;
                ++emitted;
            }
        }

        return emitted;
    }

private:
    KeyDebouncer<kMaxKeys> debouncer_{};
    KeyMapper<kMaxKeys> mapper_{};
};

} // namespace harmonia::core
