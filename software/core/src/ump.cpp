#include "harmonia/core/ump.hpp"

namespace harmonia::core {

namespace {

constexpr std::uint32_t build_word0(
    std::uint8_t mt,
    std::uint8_t group,
    std::uint8_t status,
    std::uint8_t channel,
    std::uint8_t byte2,
    std::uint8_t byte3
) noexcept {
    return (static_cast<std::uint32_t>(mt & 0x0Fu) << 28) |
           (static_cast<std::uint32_t>(group & 0x0Fu) << 24) |
           (static_cast<std::uint32_t>(status & 0x0Fu) << 20) |
           (static_cast<std::uint32_t>(channel & 0x0Fu) << 16) |
           (static_cast<std::uint32_t>(byte2) << 8) |
           static_cast<std::uint32_t>(byte3);
}

UmpError validate_note_inputs(const NoteEvent& event, std::uint8_t group) noexcept {
    if (!is_valid_ump_group(group)) {
        return UmpError::InvalidGroup;
    }
    if (event.type != EventType::NoteOn && event.type != EventType::NoteOff) {
        return UmpError::InvalidEventType;
    }
    if (!is_valid_channel(event.channel)) {
        return UmpError::InvalidChannel;
    }
    if (!is_valid_note(event.note)) {
        return UmpError::InvalidNote;
    }
    return UmpError::None;
}

UmpError validate_control_inputs(const ControlEvent& event, std::uint8_t group) noexcept {
    if (!is_valid_ump_group(group)) {
        return UmpError::InvalidGroup;
    }
    if (!is_valid_channel(event.channel)) {
        return UmpError::InvalidChannel;
    }
    if (!is_valid_controller(event.cc_number)) {
        return UmpError::InvalidController;
    }
    return UmpError::None;
}

UmpDecodeResult decode_midi1_packet(const UmpPacket& packet) noexcept {
    if (packet.word_count != 1u) {
        return {false,
                packet.word_count < 1u ? UmpError::TruncatedPacket : UmpError::InvalidWordCount,
                {}};
    }

    const std::uint32_t w0 = packet.words[0];
    const std::uint8_t group = static_cast<std::uint8_t>((w0 >> 24) & 0x0Fu);
    const std::uint8_t status = static_cast<std::uint8_t>((w0 >> 20) & 0x0Fu);
    const std::uint8_t channel = static_cast<std::uint8_t>((w0 >> 16) & 0x0Fu);
    const std::uint8_t byte2 = static_cast<std::uint8_t>((w0 >> 8) & 0xFFu);
    const std::uint8_t byte3 = static_cast<std::uint8_t>(w0 & 0xFFu);

    if ((byte2 & 0x80u) != 0u || (byte3 & 0x80u) != 0u) {
        return {false, UmpError::InvalidSevenBitField, {}};
    }

    if (status == static_cast<std::uint8_t>(UmpChannelStatus::NoteOff)) {
        const NoteEvent ev{EventType::NoteOff, channel, byte2, velocity_7_to_16(byte3)};
        return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi1ChannelVoice, ev}};
    }

    if (status == static_cast<std::uint8_t>(UmpChannelStatus::NoteOn)) {
        // Semantica MIDI 1.0: NoteOn com velocity 0 equivale a NoteOff com velocity 0.
        if (byte3 == 0u) {
            const NoteEvent ev{EventType::NoteOff, channel, byte2, 0u};
            return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi1ChannelVoice, ev}};
        }
        const NoteEvent ev{EventType::NoteOn, channel, byte2, velocity_7_to_16(byte3)};
        return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi1ChannelVoice, ev}};
    }

    if (status == static_cast<std::uint8_t>(UmpChannelStatus::ControlChange)) {
        const ControlEvent ev{channel, byte2, cc_7_to_32(byte3), 0u};
        return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi1ChannelVoice, ev}};
    }

    return {false, UmpError::UnsupportedStatus, {}};
}

UmpDecodeResult decode_midi2_packet(const UmpPacket& packet) noexcept {
    if (packet.word_count < 2u) {
        return {false, UmpError::TruncatedPacket, {}};
    }
    if (packet.word_count > 2u) {
        return {false, UmpError::InvalidWordCount, {}};
    }

    const std::uint32_t w0 = packet.words[0];
    const std::uint32_t w1 = packet.words[1];
    const std::uint8_t group = static_cast<std::uint8_t>((w0 >> 24) & 0x0Fu);
    const std::uint8_t status = static_cast<std::uint8_t>((w0 >> 20) & 0x0Fu);
    const std::uint8_t channel = static_cast<std::uint8_t>((w0 >> 16) & 0x0Fu);
    const std::uint8_t byte2 = static_cast<std::uint8_t>((w0 >> 8) & 0xFFu);
    const std::uint8_t byte3 = static_cast<std::uint8_t>(w0 & 0xFFu);

    if ((byte2 & 0x80u) != 0u) {
        return {false, UmpError::InvalidSevenBitField, {}};
    }

    if (status == static_cast<std::uint8_t>(UmpChannelStatus::NoteOff) ||
        status == static_cast<std::uint8_t>(UmpChannelStatus::NoteOn)) {
        // Sem atributos: Attribute Type == 0x00 e Attribute Data == 0x0000
        const std::uint16_t attr_data = static_cast<std::uint16_t>(w1 & 0xFFFFu);
        if (byte3 != 0u || attr_data != 0u) {
            return {false, UmpError::InvalidReservedField, {}};
        }
        const std::uint16_t vel16 = static_cast<std::uint16_t>((w1 >> 16) & 0xFFFFu);
        const EventType ev_type = (status == static_cast<std::uint8_t>(UmpChannelStatus::NoteOn))
            ? EventType::NoteOn
            : EventType::NoteOff;
        const NoteEvent ev{ev_type, channel, byte2, vel16};
        return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi2ChannelVoice, ev}};
    }

    if (status == static_cast<std::uint8_t>(UmpChannelStatus::ControlChange)) {
        if (byte3 != 0u) {
            return {false, UmpError::InvalidReservedField, {}};
        }
        const ControlEvent ev{channel, byte2, w1, 0u};
        return {true, UmpError::None, DecodedUmpMessage{group, UmpMessageType::Midi2ChannelVoice, ev}};
    }

    return {false, UmpError::UnsupportedStatus, {}};
}

} // namespace

UmpEncodeResult encode_midi2_note(const NoteEvent& event, std::uint8_t group) noexcept {
    const UmpError err = validate_note_inputs(event, group);
    if (err != UmpError::None) {
        return {false, err, {}};
    }

    const std::uint8_t status = static_cast<std::uint8_t>(
        (event.type == EventType::NoteOn) ? UmpChannelStatus::NoteOn : UmpChannelStatus::NoteOff
    );
    UmpPacket pkt{};
    pkt.word_count = 2;
    pkt.words[0] = build_word0(
        static_cast<std::uint8_t>(UmpMessageType::Midi2ChannelVoice),
        group,
        status,
        event.channel,
        event.note,
        0x00u
    );
    pkt.words[1] = static_cast<std::uint32_t>(event.velocity) << 16;
    return {true, UmpError::None, pkt};
}

UmpEncodeResult encode_midi2_control(const ControlEvent& event, std::uint8_t group) noexcept {
    const UmpError err = validate_control_inputs(event, group);
    if (err != UmpError::None) {
        return {false, err, {}};
    }

    UmpPacket pkt{};
    pkt.word_count = 2;
    pkt.words[0] = build_word0(
        static_cast<std::uint8_t>(UmpMessageType::Midi2ChannelVoice),
        group,
        static_cast<std::uint8_t>(UmpChannelStatus::ControlChange),
        event.channel,
        event.cc_number,
        0x00u
    );
    pkt.words[1] = event.value_32;
    return {true, UmpError::None, pkt};
}

UmpEncodeResult encode_midi1_note(const NoteEvent& event, std::uint8_t group) noexcept {
    const UmpError err = validate_note_inputs(event, group);
    if (err != UmpError::None) {
        return {false, err, {}};
    }

    std::uint8_t vel7 = velocity_16_to_7(event.velocity);
    if (event.type == EventType::NoteOn && event.velocity > 0u && vel7 == 0u) {
        vel7 = 1u;
    }

    const std::uint8_t status = static_cast<std::uint8_t>(
        (event.type == EventType::NoteOn) ? UmpChannelStatus::NoteOn : UmpChannelStatus::NoteOff
    );
    UmpPacket pkt{};
    pkt.word_count = 1;
    pkt.words[0] = build_word0(
        static_cast<std::uint8_t>(UmpMessageType::Midi1ChannelVoice),
        group,
        status,
        event.channel,
        event.note,
        vel7
    );
    return {true, UmpError::None, pkt};
}

UmpEncodeResult encode_midi1_control(const ControlEvent& event, std::uint8_t group) noexcept {
    const UmpError err = validate_control_inputs(event, group);
    if (err != UmpError::None) {
        return {false, err, {}};
    }

    const std::uint8_t val7 = to_midi1_7bit(event.value_32);
    UmpPacket pkt{};
    pkt.word_count = 1;
    pkt.words[0] = build_word0(
        static_cast<std::uint8_t>(UmpMessageType::Midi1ChannelVoice),
        group,
        static_cast<std::uint8_t>(UmpChannelStatus::ControlChange),
        event.channel,
        event.cc_number,
        val7
    );
    return {true, UmpError::None, pkt};
}

UmpDecodeResult decode_ump_packet(const UmpPacket& packet) noexcept {
    if (packet.word_count == 0u) {
        return {false, UmpError::TruncatedPacket, {}};
    }

    const std::uint8_t mt = packet.message_type();
    const std::uint8_t expected_words = ump_expected_word_count(mt);
    if (packet.word_count < expected_words) {
        return {false, UmpError::TruncatedPacket, {}};
    }

    if (mt == static_cast<std::uint8_t>(UmpMessageType::Midi1ChannelVoice)) {
        return decode_midi1_packet(packet);
    }
    if (mt == static_cast<std::uint8_t>(UmpMessageType::Midi2ChannelVoice)) {
        return decode_midi2_packet(packet);
    }

    return {false, UmpError::UnsupportedMessageType, {}};
}

} // namespace harmonia::core