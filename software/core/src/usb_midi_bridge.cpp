#include "harmonia/core/usb_midi_bridge.hpp"

#include <variant>

namespace harmonia::core {

std::optional<UsbMidiFrame> UsbMidiBridge::prepare_tx(const UmpPacket& canonical_ump) noexcept {
    if (alt_setting_ == UsbAltSetting::Disconnected) {
        ++drop_count_;
        return std::nullopt;
    }

    const UmpDecodeResult decoded = decode_ump_packet(canonical_ump);
    if (!decoded.ok) {
        ++drop_count_;
        return std::nullopt;
    }

    const std::uint8_t group = decoded.message.group;

    if (alt_setting_ == UsbAltSetting::Midi2Native) {
        UmpEncodeResult enc{};
        if (std::holds_alternative<NoteEvent>(decoded.message.event)) {
            enc = encode_midi2_note(std::get<NoteEvent>(decoded.message.event), group);
        } else {
            enc = encode_midi2_control(std::get<ControlEvent>(decoded.message.event), group);
        }
        if (!enc.ok) {
            ++drop_count_;
            return std::nullopt;
        }
        UsbMidiFrame frame{};
        frame.mode = UsbAltSetting::Midi2Native;
        frame.ump = enc.packet;
        return frame;
    }

    // Alt Setting 0: Fallback MIDI 1.0 (UMP MT=0x2 + pacote USB-MIDI 1.0 de 4 bytes)
    UmpEncodeResult enc{};
    if (std::holds_alternative<NoteEvent>(decoded.message.event)) {
        enc = encode_midi1_note(std::get<NoteEvent>(decoded.message.event), group);
    } else {
        enc = encode_midi1_control(std::get<ControlEvent>(decoded.message.event), group);
    }
    if (!enc.ok || enc.packet.word_count != 1u) {
        ++drop_count_;
        return std::nullopt;
    }

    const std::uint32_t w0 = enc.packet.words[0];
    const std::uint8_t status_nibble = static_cast<std::uint8_t>((w0 >> 20) & 0x0Fu);
    const std::uint8_t channel = static_cast<std::uint8_t>((w0 >> 16) & 0x0Fu);
    const std::uint8_t byte2 = static_cast<std::uint8_t>((w0 >> 8) & 0xFFu);
    const std::uint8_t byte3 = static_cast<std::uint8_t>(w0 & 0xFFu);

    UsbMidiFrame frame{};
    frame.mode = UsbAltSetting::Midi1Fallback;
    frame.ump = enc.packet;
    frame.midi1_usb_packet[0] = static_cast<std::uint8_t>(((cable_number_ & 0x0Fu) << 4) | status_nibble);
    frame.midi1_usb_packet[1] = static_cast<std::uint8_t>((status_nibble << 4) | channel);
    frame.midi1_usb_packet[2] = byte2;
    frame.midi1_usb_packet[3] = byte3;
    return frame;
}

UmpDecodeResult UsbMidiBridge::receive_ump(const UmpPacket& rx_packet) noexcept {
    const UmpDecodeResult res = decode_ump_packet(rx_packet);
    if (res.ok) {
        ++rx_ok_count_;
    } else {
        ++rx_error_count_;
    }
    return res;
}

UmpDecodeResult UsbMidiBridge::receive_midi1_usb_packet(const std::uint8_t pkt4[4]) noexcept {
    if (pkt4 == nullptr) {
        ++rx_error_count_;
        return {false, UmpError::TruncatedPacket, {}};
    }

    const std::uint8_t cable = static_cast<std::uint8_t>((pkt4[0] >> 4) & 0x0Fu);
    const std::uint8_t cin = static_cast<std::uint8_t>(pkt4[0] & 0x0Fu);
    const std::uint8_t status_ch = pkt4[1];
    const std::uint8_t status_nibble = static_cast<std::uint8_t>((status_ch >> 4) & 0x0Fu);

    if (cin != status_nibble || (cin != 0x8u && cin != 0x9u && cin != 0xBu)) {
        ++rx_error_count_;
        return {false, UmpError::UnsupportedStatus, {}};
    }

    UmpPacket pkt{};
    pkt.word_count = 1;
    pkt.words[0] = (static_cast<std::uint32_t>(UmpMessageType::Midi1ChannelVoice) << 28) |
                   (static_cast<std::uint32_t>(cable) << 24) |
                   (static_cast<std::uint32_t>(pkt4[1]) << 16) |
                   (static_cast<std::uint32_t>(pkt4[2]) << 8) |
                   static_cast<std::uint32_t>(pkt4[3]);

    return receive_ump(pkt);
}

} // namespace harmonia::core