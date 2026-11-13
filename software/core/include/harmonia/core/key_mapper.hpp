#pragma once

#include "harmonia/core/key_matrix.hpp"
#include "harmonia/core/midi_event.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Mapeamento configurável de índice de tecla lógica para evento MIDI (NoteOn/NoteOff)
/// com canal e velocity fixa configuráveis. Sem alocação dinâmica.
template <std::size_t MaxKeys>
class KeyMapper {
public:
    static constexpr std::uint8_t kUnmappedNote = 0xFF;
    static constexpr std::uint16_t kDefaultFixedVelocity = 32768; // ~50% em 16 bits

    constexpr KeyMapper() noexcept {
        notes_.fill(kUnmappedNote);
    }

    constexpr bool set_channel(std::uint8_t channel) noexcept {
        if (!is_valid_channel(channel)) {
            return false;
        }
        channel_ = channel;
        return true;
    }

    [[nodiscard]] constexpr std::uint8_t channel() const noexcept {
        return channel_;
    }

    constexpr void set_fixed_velocity(std::uint16_t velocity) noexcept {
        fixed_velocity_ = velocity;
    }

    [[nodiscard]] constexpr std::uint16_t fixed_velocity() const noexcept {
        return fixed_velocity_;
    }

    /// Mapeia um índice de tecla para uma nota MIDI válida [0..127] ou kUnmappedNote.
    constexpr bool set_note(std::size_t key_index, std::uint8_t midi_note) noexcept {
        if (key_index >= MaxKeys) {
            return false;
        }
        if (midi_note != kUnmappedNote && !is_valid_note(midi_note)) {
            return false;
        }
        notes_[key_index] = midi_note;
        return true;
    }

    /// Preenche mapeamento cromático sequencial a partir de `base_note` para `count` teclas.
    constexpr bool assign_chromatic(std::uint8_t base_note, std::size_t count = MaxKeys) noexcept {
        if (count > MaxKeys || !is_valid_note(base_note)) {
            return false;
        }
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t note_val = static_cast<std::size_t>(base_note) + i;
            if (note_val > kMaxNote) {
                notes_[i] = kUnmappedNote;
            } else {
                notes_[i] = static_cast<std::uint8_t>(note_val);
            }
        }
        return true;
    }

    [[nodiscard]] constexpr std::uint8_t note_for(std::size_t key_index) const noexcept {
        if (key_index >= MaxKeys) {
            return kUnmappedNote;
        }
        return notes_[key_index];
    }

    /// Converte uma transição estável em NoteEvent (ou std::nullopt se None ou não mapeada).
    [[nodiscard]] std::optional<NoteEvent> map_transition(
        std::size_t key_index,
        KeyTransition transition
    ) const noexcept {
        if (transition == KeyTransition::None || key_index >= MaxKeys) {
            return std::nullopt;
        }
        const std::uint8_t note = notes_[key_index];
        if (!is_valid_note(note)) {
            return std::nullopt;
        }
        if (transition == KeyTransition::Pressed) {
            return make_note_on(channel_, note, fixed_velocity_);
        }
        return make_note_off(channel_, note, 0);
    }

private:
    std::uint8_t channel_{0};
    std::uint16_t fixed_velocity_{kDefaultFixedVelocity};
    std::array<std::uint8_t, MaxKeys> notes_{};
};

} // namespace harmonia::core
