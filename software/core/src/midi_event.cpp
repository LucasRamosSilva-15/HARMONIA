#include "harmonia/core/midi_event.hpp"

namespace harmonia::core {

bool is_valid_note_event(const NoteEvent& event) noexcept {
    const bool valid_type = (event.type == EventType::NoteOn || event.type == EventType::NoteOff);
    return valid_type && is_valid_channel(event.channel) && is_valid_note(event.note);
}

std::optional<NoteEvent> make_note_on(
    std::uint8_t channel,
    std::uint8_t note,
    std::uint16_t velocity
) noexcept {
    if (!is_valid_channel(channel) || !is_valid_note(note)) {
        return std::nullopt;
    }
    return NoteEvent{
        .type = EventType::NoteOn,
        .channel = channel,
        .note = note,
        .velocity = velocity
    };
}

std::optional<NoteEvent> make_note_off(
    std::uint8_t channel,
    std::uint8_t note,
    std::uint16_t velocity
) noexcept {
    if (!is_valid_channel(channel) || !is_valid_note(note)) {
        return std::nullopt;
    }
    return NoteEvent{
        .type = EventType::NoteOff,
        .channel = channel,
        .note = note,
        .velocity = velocity
    };
}

} // namespace harmonia::core
