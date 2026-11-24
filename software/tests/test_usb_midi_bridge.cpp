#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/usb_midi_bridge.hpp"

#include <type_traits>
#include <variant>

using namespace harmonia::core;

TEST_CASE("UsbMidiBridge preserves MT=0x4 in Alt Setting 1 (Midi2Native)", "[core][usb_midi]") {
    static_assert(std::is_trivially_copyable_v<UsbMidiFrame>);
    static_assert(std::is_trivially_copyable_v<UsbMidiBridge>);

    UsbMidiBridge bridge(0);
    bridge.set_alt_setting(UsbAltSetting::Midi2Native);
    REQUIRE(bridge.is_mounted());

    const auto note_on = encode_midi2_note(*make_note_on(0, 60, 40000), 0);
    REQUIRE(note_on.ok);

    const auto frame = bridge.prepare_tx(note_on.packet);
    REQUIRE(frame.has_value());
    REQUIRE(frame->mode == UsbAltSetting::Midi2Native);
    REQUIRE(frame->ump.word_count == 2);
    REQUIRE(frame->ump.words[0] == 0x40903C00u);
    REQUIRE(frame->ump.words[1] == 0x9C400000u);

    bridge.record_tx_result(true);
    REQUIRE(bridge.tx_ok_count() == 1);
    REQUIRE(bridge.drop_count() == 0);
}

TEST_CASE("UsbMidiBridge downgrades deterministically in Alt Setting 0 (Midi1Fallback)", "[core][usb_midi]") {
    UsbMidiBridge bridge(0);
    bridge.set_alt_setting(UsbAltSetting::Midi1Fallback);

    // 1. Note On (vel=40000 -> vel7=78=0x4E)
    const auto enc_on = encode_midi2_note(*make_note_on(0, 60, 40000), 0);
    const auto f_on = bridge.prepare_tx(enc_on.packet);
    REQUIRE(f_on.has_value());
    REQUIRE(f_on->mode == UsbAltSetting::Midi1Fallback);
    REQUIRE(f_on->ump.word_count == 1);
    REQUIRE(f_on->ump.words[0] == 0x20903C4Eu);
    REQUIRE(f_on->midi1_usb_packet == std::array<std::uint8_t, 4>{0x09u, 0x90u, 60u, 78u});

    // 2. Soft Note On (vel16=1 -> clamped to vel7=1 so it is NOT treated as NoteOff)
    const auto enc_soft = encode_midi2_note(*make_note_on(0, 61, 1), 0);
    const auto f_soft = bridge.prepare_tx(enc_soft.packet);
    REQUIRE(f_soft.has_value());
    REQUIRE(f_soft->midi1_usb_packet == std::array<std::uint8_t, 4>{0x09u, 0x90u, 61u, 1u});

    // 3. Note Off
    const auto enc_off = encode_midi2_note(*make_note_off(0, 60, 0), 0);
    const auto f_off = bridge.prepare_tx(enc_off.packet);
    REQUIRE(f_off.has_value());
    REQUIRE(f_off->ump.words[0] == 0x20803C00u);
    REQUIRE(f_off->midi1_usb_packet == std::array<std::uint8_t, 4>{0x08u, 0x80u, 60u, 0u});

    // 4. Control Change max (0xFFFFFFFF -> 127 = 0x7F)
    const auto enc_cc = encode_midi2_control(ControlEvent{0, 7, 0xFFFFFFFFu, 4095}, 0);
    const auto f_cc = bridge.prepare_tx(enc_cc.packet);
    REQUIRE(f_cc.has_value());
    REQUIRE(f_cc->ump.words[0] == 0x20B0077Fu);
    REQUIRE(f_cc->midi1_usb_packet == std::array<std::uint8_t, 4>{0x0Bu, 0xB0u, 7u, 127u});
}

TEST_CASE("UsbMidiBridge drops without blocking when disconnected or busy", "[core][usb_midi]") {
    UsbMidiBridge bridge(0);
    REQUIRE_FALSE(bridge.is_mounted());
    REQUIRE(bridge.alt_setting() == UsbAltSetting::Disconnected);

    const auto enc = encode_midi2_note(*make_note_on(0, 60, 40000), 0);
    REQUIRE_FALSE(bridge.prepare_tx(enc.packet).has_value());
    REQUIRE(bridge.drop_count() == 1);

    bridge.set_alt_setting(UsbAltSetting::Midi1Fallback);
    REQUIRE(bridge.prepare_tx(enc.packet).has_value());
    bridge.record_tx_result(false); // endpoint busy
    REQUIRE(bridge.drop_count() == 2);
    REQUIRE(bridge.tx_ok_count() == 0);
}

TEST_CASE("UsbMidiBridge decodes host RX packets in UMP and USB-MIDI 1.0 formats", "[core][usb_midi]") {
    UsbMidiBridge bridge(0);

    // Loopback UMP MIDI 2.0
    const auto enc = encode_midi2_control(ControlEvent{2, 11, 0x80000000u, 0}, 0);
    const auto dec_ump = bridge.receive_ump(enc.packet);
    REQUIRE(dec_ump.ok);
    REQUIRE(std::get<ControlEvent>(dec_ump.message.event).cc_number == 11);

    // Loopback USB-MIDI 1.0 4-byte packet (Note On ch=0 note=64 vel=100)
    const std::uint8_t pkt4[4] = {0x09u, 0x90u, 64u, 100u};
    const auto dec_m1 = bridge.receive_midi1_usb_packet(pkt4);
    REQUIRE(dec_m1.ok);
    const auto& nev = std::get<NoteEvent>(dec_m1.message.event);
    REQUIRE(nev.type == EventType::NoteOn);
    REQUIRE(nev.note == 64);
    REQUIRE(velocity_16_to_7(nev.velocity) == 100);

    // Invalid packet rejected
    const std::uint8_t bad4[4] = {0x0Fu, 0xF0u, 0u, 0u};
    REQUIRE_FALSE(bridge.receive_midi1_usb_packet(bad4).ok);
    REQUIRE(bridge.rx_ok_count() == 2);
    REQUIRE(bridge.rx_error_count() == 1);
}