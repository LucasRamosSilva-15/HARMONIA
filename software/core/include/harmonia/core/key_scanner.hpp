#pragma once

#include "harmonia/core/key_debouncer.hpp"
#include "harmonia/core/key_mapper.hpp"
#include "harmonia/core/key_matrix.hpp"
#include "harmonia/core/midi_event.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Resultado detalhado de um ciclo de varredura, expondo saturação e pendências.
struct ScanResult {
    std::size_t emitted{0};
    bool overflow{false};
    std::size_t dropped_note_on{0};
    std::size_t pending_count{0};
};

/// Nota efetivamente ativada por uma tecla, garantindo que o NoteOff use
/// o mesmo canal e nota do NoteOn original mesmo se o mapeamento mudar.
struct ActiveNote {
    std::uint8_t channel{0};
    std::uint8_t note{0};
};

/// Orquestrador determinístico de aquisição lógica:
/// - Executa debounce por tecla
/// - Garante consistência de NoteOff mesmo com remapeamento em tempo real
/// - Mantém fila circular estática (sem heap) para evitar perda silenciosa quando
///   o buffer do chamador é menor que o lote de eventos
/// - Nunca descarta silenciosamente um NoteOff em caso de saturação extrema
template <std::size_t MaxRows, std::size_t MaxCols>
class KeyScanner {
public:
    static constexpr std::size_t kMaxKeys = MaxRows * MaxCols;
    static constexpr std::size_t kQueueCapacity = kMaxKeys * 2;

    constexpr KeyScanner() noexcept = default;

    constexpr KeyDebouncer<kMaxKeys>& debouncer() noexcept { return debouncer_; }
    constexpr const KeyDebouncer<kMaxKeys>& debouncer() const noexcept { return debouncer_; }

    constexpr KeyMapper<kMaxKeys>& mapper() noexcept { return mapper_; }
    constexpr const KeyMapper<kMaxKeys>& mapper() const noexcept { return mapper_; }

    constexpr void reset(bool initial_pressed = false) noexcept {
        debouncer_.reset(initial_pressed);
        active_notes_.fill(std::nullopt);
        queue_head_ = 0;
        queue_tail_ = 0;
        queue_count_ = 0;
    }

    [[nodiscard]] constexpr std::size_t pending_count() const noexcept {
        return queue_count_;
    }

    /// Drena eventos pendentes da fila interna para `out_events` sem nova varredura.
    [[nodiscard]] std::size_t drain(NoteEvent* out_events, std::size_t max_events) noexcept {
        if (out_events == nullptr || max_events == 0) {
            return 0;
        }
        std::size_t emitted = 0;
        while (queue_count_ > 0 && emitted < max_events) {
            out_events[emitted] = queue_[queue_head_];
            queue_head_ = (queue_head_ + 1) % kQueueCapacity;
            --queue_count_;
            ++emitted;
        }
        return emitted;
    }

    /// Processa todas as teclas ativas da matriz no instante `now_us`, retornando
    /// informações completas de emissão, overflow e eventos ainda pendentes.
    [[nodiscard]] ScanResult scan_with_status(
        const KeyMatrixState<MaxRows, MaxCols>& matrix,
        std::uint32_t now_us,
        NoteEvent* out_events,
        std::size_t max_events
    ) noexcept {
        ScanResult result{};
        const std::size_t count = matrix.key_count();

        for (std::size_t idx = 0; idx < count; ++idx) {
            const bool raw = matrix.get_by_index(idx);
            const KeyTransition tr = debouncer_.update(idx, raw, now_us);
            if (tr == KeyTransition::None) {
                continue;
            }

            if (tr == KeyTransition::Pressed) {
                const auto ev = mapper_.map_transition(idx, tr);
                if (!ev.has_value()) {
                    continue;
                }
                if (enqueue_note_on(*ev)) {
                    active_notes_[idx] = ActiveNote{ev->channel, ev->note};
                } else {
                    result.overflow = true;
                    ++result.dropped_note_on;
                }
            } else {
                // NoteOff usa a nota originalmente ativada, evitando notas presas
                // caso o mapeamento MIDI tenha mudado com a tecla pressionada.
                std::optional<NoteEvent> ev;
                if (active_notes_[idx].has_value()) {
                    ev = make_note_off(active_notes_[idx]->channel, active_notes_[idx]->note, 0);
                    active_notes_[idx] = std::nullopt;
                } else {
                    // Se o NoteOn foi descartado por overflow, não há nota ativa para desligar.
                    continue;
                }

                if (ev.has_value()) {
                    if (!enqueue_note_off_priority(*ev)) {
                        result.overflow = true;
                    }
                }
            }
        }

        result.emitted = drain(out_events, max_events);
        result.pending_count = queue_count_;
        return result;
    }

    /// Compatibilidade direta: retorna apenas a quantidade de eventos emitidos.
    [[nodiscard]] std::size_t scan(
        const KeyMatrixState<MaxRows, MaxCols>& matrix,
        std::uint32_t now_us,
        NoteEvent* out_events,
        std::size_t max_events
    ) noexcept {
        return scan_with_status(matrix, now_us, out_events, max_events).emitted;
    }

private:
    bool enqueue_note_on(const NoteEvent& ev) noexcept {
        if (queue_count_ >= kQueueCapacity) {
            return false;
        }
        queue_[queue_tail_] = ev;
        queue_tail_ = (queue_tail_ + 1) % kQueueCapacity;
        ++queue_count_;
        return true;
    }

    /// Garante que NoteOff nunca seja descartado silenciosamente:
    /// 1) Se há espaço, enfileira normalmente.
    /// 2) Se cheia, tenta cancelar o NoteOn pendente correspondente (mesmo canal/nota).
    /// 3) Caso contrário, substitui o NoteOn mais antigo na fila.
    bool enqueue_note_off_priority(const NoteEvent& ev) noexcept {
        if (queue_count_ < kQueueCapacity) {
            queue_[queue_tail_] = ev;
            queue_tail_ = (queue_tail_ + 1) % kQueueCapacity;
            ++queue_count_;
            return true;
        }

        // Procura um NoteOn na fila para ceder lugar ao NoteOff (preferindo o mesmo canal/nota)
        std::size_t fallback_pos = kQueueCapacity;
        for (std::size_t i = 0; i < queue_count_; ++i) {
            const std::size_t pos = (queue_head_ + i) % kQueueCapacity;
            if (queue_[pos].type == EventType::NoteOn) {
                if (queue_[pos].channel == ev.channel && queue_[pos].note == ev.note) {
                    // Substitui no local por NoteOff (ou mantém consistência)
                    queue_[pos] = ev;
                    return false; // sinaliza que houve saturação tratada
                }
                if (fallback_pos == kQueueCapacity) {
                    fallback_pos = pos;
                }
            }
        }

        if (fallback_pos != kQueueCapacity) {
            queue_[fallback_pos] = ev;
        }
        return false;
    }

    KeyDebouncer<kMaxKeys> debouncer_{};
    KeyMapper<kMaxKeys> mapper_{};
    std::array<std::optional<ActiveNote>, kMaxKeys> active_notes_{};
    std::array<NoteEvent, kQueueCapacity> queue_{};
    std::size_t queue_head_{0};
    std::size_t queue_tail_{0};
    std::size_t queue_count_{0};
};

} // namespace harmonia::core
