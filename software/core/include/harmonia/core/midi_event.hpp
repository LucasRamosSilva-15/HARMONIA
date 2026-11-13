#pragma once

#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Tipos fundamentais de eventos musicais (independentes de protocolo/hardware).
enum class EventType : std::uint8_t {
    Unknown = 0,
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend
};

/// Limites padrão MIDI para canal (0-15) e número de nota (0-127).
inline constexpr std::uint8_t kMinChannel = 0;
inline constexpr std::uint8_t kMaxChannel = 15;
inline constexpr std::uint8_t kMinNote = 0;
inline constexpr std::uint8_t kMaxNote = 127;

/// Representação de evento de nota independente de TinyUSB, Qt ou FluidSynth.
/// Velocity usa uint16_t para acomodar resolução de 16 bits (MIDI 2.0) ou escalonamento MIDI 1.0.
struct NoteEvent {
    EventType type{EventType::Unknown};
    std::uint8_t channel{0};
    std::uint8_t note{0};
    std::uint16_t velocity{0};

    constexpr bool operator==(const NoteEvent& other) const noexcept {
        return type == other.type &&
               channel == other.channel &&
               note == other.note &&
               velocity == other.velocity;
    }

    constexpr bool operator!=(const NoteEvent& other) const noexcept {
        return !(*this == other);
    }
};

/// Verifica se o canal MIDI está no intervalo válido [0, 15].
[[nodiscard]] constexpr bool is_valid_channel(std::uint8_t channel) noexcept {
    return channel <= kMaxChannel;
}

/// Verifica se o número da nota MIDI está no intervalo válido [0, 127].
[[nodiscard]] constexpr bool is_valid_note(std::uint8_t note) noexcept {
    return note <= kMaxNote;
}

/// Verifica se um NoteEvent possui tipo de nota válido, canal válido e nota válida.
[[nodiscard]] bool is_valid_note_event(const NoteEvent& event) noexcept;

/// Constrói um evento NoteOn validado; retorna std::nullopt se canal ou nota forem inválidos.
[[nodiscard]] std::optional<NoteEvent> make_note_on(
    std::uint8_t channel,
    std::uint8_t note,
    std::uint16_t velocity
) noexcept;

/// Constrói um evento NoteOff validado; retorna std::nullopt se canal ou nota forem inválidos.
[[nodiscard]] std::optional<NoteEvent> make_note_off(
    std::uint8_t channel,
    std::uint8_t note,
    std::uint16_t velocity = 0
) noexcept;

} // namespace harmonia::core
