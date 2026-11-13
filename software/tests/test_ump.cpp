#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/ump.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>

using namespace harmonia::core;

TEST_CASE("UMP MIDI 2.0 Note On, Note Off and CC match independent golden vectors", "[core][ump][golden]") {
    // 1. Note On MIDI 2.0: Group 0, Ch 0, Note 60 (0x3C), Vel 40000 (0x9C40)
    // w0 = (0x4 << 28) | (0x0 << 24) | (0x9 << 20) | (0x0 << 16) | (0x3C << 8) | 0x00 = 0x40903C00
    // w1 = (0x9C40 << 16) | 0x0000 = 0x9C400000
    const auto note_on = *make_note_on(0, 60, 40000);
    const auto enc_on = encode_midi2_note(note_on, 0);
    REQUIRE(enc_on.ok);
    REQUIRE(enc_on.packet.word_count == 2);
    REQUIRE(enc_on.packet.words[0] == 0x40903C00u);
    REQUIRE(enc_on.packet.words[1] == 0x9C400000u);

    // 2. Note Off MIDI 2.0: Group 0, Ch 0, Note 60 (0x3C), Vel 0
    // w0 = 0x40803C00, w1 = 0x00000000
    const auto note_off = *make_note_off(0, 60, 0);
    const auto enc_off = encode_midi2_note(note_off, 0);
    REQUIRE(enc_off.ok);
    REQUIRE(enc_off.packet.word_count == 2);
    REQUIRE(enc_off.packet.words[0] == 0x40803C00u);
    REQUIRE(enc_off.packet.words[1] == 0x00000000u);

    // 3. CC MIDI 2.0: Group 0, Ch 0, CC 7 (0x07), Val 0xFFFFFFFF
    // w0 = (0x4 << 28) | (0x0 << 24) | (0xB << 20) | (0x0 << 16) | (0x07 << 8) | 0x00 = 0x40B00700
    const ControlEvent cc_max{0, 7, 0xFFFFFFFFu, 4095};
    const auto enc_cc = encode_midi2_control(cc_max, 0);
    REQUIRE(enc_cc.ok);
    REQUIRE(enc_cc.packet.word_count == 2);
    REQUIRE(enc_cc.packet.words[0] == 0x40B00700u);
    REQUIRE(enc_cc.packet.words[1] == 0xFFFFFFFFu);
}

TEST_CASE("UMP MIDI 1.0 Channel Voice matches independent golden vectors", "[core][ump][golden]") {
    // 4. Note On MIDI 1.0: Group 0, Ch 0, Note 60 (0x3C), Vel16=40000 -> Vel7 = 40000 >> 9 = 78 (0x4E)
    // w0 = 0x20903C4E
    const auto note_on = *make_note_on(0, 60, 40000);
    const auto enc_on = encode_midi1_note(note_on, 0);
    REQUIRE(enc_on.ok);
    REQUIRE(enc_on.packet.word_count == 1);
    REQUIRE(enc_on.packet.words[0] == 0x20903C4Eu);

    // 5. Note Off MIDI 1.0: Group 0, Ch 0, Note 60 (0x3C), Vel 0 -> 0x20803C00
    const auto note_off = *make_note_off(0, 60, 0);
    const auto enc_off = encode_midi1_note(note_off, 0);
    REQUIRE(enc_off.ok);
    REQUIRE(enc_off.packet.word_count == 1);
    REQUIRE(enc_off.packet.words[0] == 0x20803C00u);

    // 6. CC MIDI 1.0: Group 15 (0xF), Ch 15 (0xF), CC 11 (0x0B), Val32=0xFFFFFFFF -> Val7=127 (0x7F)
    // w0 = 0x2FBF0B7F
    const ControlEvent cc11_max{15, 11, 0xFFFFFFFFu, 4095};
    const auto enc_cc = encode_midi1_control(cc11_max, 15);
    REQUIRE(enc_cc.ok);
    REQUIRE(enc_cc.packet.word_count == 1);
    REQUIRE(enc_cc.packet.words[0] == 0x2FBF0B7Fu);
}

TEST_CASE("Group, channel, note, controller and extreme value bounds", "[core][ump]") {
    // 7, 8, 9, 10: Groups 0 & 15, Channels 0 & 15, Notes 0 & 127, Velocity min 0 & max 0xFFFF
    const NoteEvent min_ev{EventType::NoteOn, 0, 0, 0};
    const auto r_min = encode_midi2_note(min_ev, 0);
    REQUIRE(r_min.ok);
    REQUIRE(r_min.packet.words[0] == 0x40900000u);
    REQUIRE(r_min.packet.words[1] == 0x00000000u);

    const NoteEvent max_ev{EventType::NoteOn, 15, 127, 0xFFFFu};
    const auto r_max = encode_midi2_note(max_ev, 15);
    REQUIRE(r_max.ok);
    REQUIRE(r_max.packet.words[0] == 0x4F9F7F00u);
    REQUIRE(r_max.packet.words[1] == 0xFFFF0000u);

    // 11: CC min & max
    const ControlEvent cc_min{0, 0, 0u, 0};
    const auto r_cc_min = encode_midi2_control(cc_min, 0);
    REQUIRE(r_cc_min.ok);
    REQUIRE(r_cc_min.packet.words[0] == 0x40B00000u);
    REQUIRE(r_cc_min.packet.words[1] == 0x00000000u);

    const ControlEvent cc_hi{15, 127, 0xFFFFFFFFu, 4095};
    const auto r_cc_hi = encode_midi2_control(cc_hi, 15);
    REQUIRE(r_cc_hi.ok);
    REQUIRE(r_cc_hi.packet.words[0] == 0x4FBF7F00u);
    REQUIRE(r_cc_hi.packet.words[1] == 0xFFFFFFFFu);

    // Invalid inputs rejected
    REQUIRE(encode_midi2_note(min_ev, 16).error == UmpError::InvalidGroup);
    REQUIRE(encode_midi2_note(NoteEvent{EventType::NoteOn, 16, 60, 100}, 0).error == UmpError::InvalidChannel);
    REQUIRE(encode_midi2_note(NoteEvent{EventType::NoteOn, 0, 128, 100}, 0).error == UmpError::InvalidNote);
    REQUIRE(encode_midi2_control(ControlEvent{0, 128, 0, 0}, 0).error == UmpError::InvalidController);
    REQUIRE(encode_midi2_note(NoteEvent{EventType::ControlChange, 0, 60, 100}, 0).error == UmpError::InvalidEventType);
}

TEST_CASE("MIDI 1.0 Note On velocity 0 semantics and soft-note clamping", "[core][ump]") {
    // Soft NoteOn with 16-bit velocity = 1 must not quantize to 0 (which means NoteOff in MIDI 1.0)
    const auto soft_on = *make_note_on(0, 60, 1);
    const auto enc_soft = encode_midi1_note(soft_on, 0);
    REQUIRE(enc_soft.ok);
    REQUIRE((enc_soft.packet.words[0] & 0x7Fu) == 1u);

    // Decoding MIDI 1.0 NoteOn with velocity 0 must yield NoteOff with velocity 0
    UmpPacket m1_note_on_vel0{};
    m1_note_on_vel0.word_count = 1;
    m1_note_on_vel0.words[0] = 0x20903C00u;
    const auto dec = decode_ump_packet(m1_note_on_vel0);
    REQUIRE(dec.ok);
    REQUIRE(std::holds_alternative<NoteEvent>(dec.message.event));
    const auto& nev = std::get<NoteEvent>(dec.message.event);
    REQUIRE(nev.type == EventType::NoteOff);
    REQUIRE(nev.channel == 0);
    REQUIRE(nev.note == 60);
    REQUIRE(nev.velocity == 0);
}

TEST_CASE("Truncated, unsupported and invalid reserved field packets are rejected", "[core][ump][validation]") {
    // 12. Truncated packets
    UmpPacket empty{};
    REQUIRE(decode_ump_packet(empty).error == UmpError::TruncatedPacket);

    UmpPacket trunc_m2{};
    trunc_m2.word_count = 1;
    trunc_m2.words[0] = 0x40903C00u;
    REQUIRE(decode_ump_packet(trunc_m2).error == UmpError::TruncatedPacket);

    // 13. Unsupported Message Types (MT=0x0 Utility, MT=0x3 Data64, MT=0x5 Data128, MT=0xF Stream)
    UmpPacket utility{};
    utility.word_count = 1;
    utility.words[0] = 0x00000000u;
    REQUIRE(decode_ump_packet(utility).error == UmpError::UnsupportedMessageType);

    UmpPacket data64{};
    data64.word_count = 2;
    data64.words[0] = 0x30000000u;
    REQUIRE(decode_ump_packet(data64).error == UmpError::UnsupportedMessageType);

    UmpPacket unsupported_status{};
    unsupported_status.word_count = 2;
    unsupported_status.words[0] = 0x40A03C00u; // Poly Pressure (0xA) not supported in Etapa 05
    REQUIRE(decode_ump_packet(unsupported_status).error == UmpError::UnsupportedStatus);

    // 14. Invalid reserved fields & invalid 7-bit fields
    UmpPacket bad_cc_reserved{};
    bad_cc_reserved.word_count = 2;
    bad_cc_reserved.words[0] = 0x40B00701u; // byte3 != 0
    bad_cc_reserved.words[1] = 0x12345678u;
    REQUIRE(decode_ump_packet(bad_cc_reserved).error == UmpError::InvalidReservedField);

    UmpPacket bad_note_attr{};
    bad_note_attr.word_count = 2;
    bad_note_attr.words[0] = 0x40903C01u; // Attribute Type != 0
    bad_note_attr.words[1] = 0x80000000u;
    REQUIRE(decode_ump_packet(bad_note_attr).error == UmpError::InvalidReservedField);

    UmpPacket bad_7bit{};
    bad_7bit.word_count = 1;
    bad_7bit.words[0] = 0x20908040u; // note = 0x80 (bit 7 set)
    REQUIRE(decode_ump_packet(bad_7bit).error == UmpError::InvalidSevenBitField);

    // Expected word counts per spec
    REQUIRE(ump_expected_word_count(0x2) == 1);
    REQUIRE(ump_expected_word_count(0x4) == 2);
    REQUIRE(ump_expected_word_count(0xB) == 3);
    REQUIRE(ump_expected_word_count(0x5) == 4);
    REQUIRE(ump_expected_word_count(0xF) == 4);
}

TEST_CASE("Round-trip encode/decode and resolution conversions", "[core][ump]") {
    // 15. MIDI 2.0 exact round-trip
    const NoteEvent orig_note{EventType::NoteOn, 5, 72, 0xABCDu};
    const auto enc_n = encode_midi2_note(orig_note, 9);
    REQUIRE(enc_n.ok);
    const auto dec_n = decode_ump_packet(enc_n.packet);
    REQUIRE(dec_n.ok);
    REQUIRE(dec_n.message.group == 9);
    REQUIRE(dec_n.message.message_type == UmpMessageType::Midi2ChannelVoice);
    REQUIRE(std::get<NoteEvent>(dec_n.message.event) == orig_note);

    const ControlEvent orig_cc{3, 11, 0x87654321u, 0};
    const auto enc_c = encode_midi2_control(orig_cc, 14);
    REQUIRE(enc_c.ok);
    const auto dec_c = decode_ump_packet(enc_c.packet);
    REQUIRE(dec_c.ok);
    REQUIRE(dec_c.message.group == 14);
    REQUIRE(std::get<ControlEvent>(dec_c.message.event) == orig_cc);

    // 16. Resolution conversions (extremes and center preserved)
    REQUIRE(velocity_16_to_7(0u) == 0u);
    REQUIRE(velocity_16_to_7(0x8000u) == 64u);
    REQUIRE(velocity_16_to_7(0xFFFFu) == 127u);

    REQUIRE(velocity_7_to_16(0u) == 0u);
    REQUIRE(velocity_7_to_16(64u) == 0x8000u);
    REQUIRE(velocity_7_to_16(127u) == 0xFFFFu);

    REQUIRE(cc_7_to_32(0u) == 0u);
    REQUIRE(cc_7_to_32(64u) == 0x80000000u);
    REQUIRE(cc_7_to_32(127u) == 0xFFFFFFFFu);

    // Byte-order independence (Big-Endian wire serialization)
    std::array<std::uint8_t, 4> be{};
    ump_word_to_be_bytes(0x40903C00u, be.data());
    REQUIRE(be[0] == 0x40u);
    REQUIRE(be[1] == 0x90u);
    REQUIRE(be[2] == 0x3Cu);
    REQUIRE(be[3] == 0x00u);
    REQUIRE(ump_word_from_be_bytes(be.data()) == 0x40903C00u);
}

TEST_CASE("UmpPacketQueue and UmpRouter operate statically without heap allocation", "[core][ump]") {
    // 17. Static layout & zero-heap routing
    static_assert(std::is_trivially_copyable_v<UmpPacket>);

    UmpPacketQueue<4> queue;
    UmpRouter router(0, UmpProtocolMode::Midi2);

    REQUIRE(router.route_note(*make_note_on(0, 60, 40000), queue));
    REQUIRE(router.route_control(ControlEvent{0, 7, 0xFFFFFFFFu, 4095}, queue));
    REQUIRE(queue.size() == 2);

    const auto p1 = queue.pop();
    const auto p2 = queue.pop();
    REQUIRE(p1.has_value());
    REQUIRE(p2.has_value());
    REQUIRE(p1->words[0] == 0x40903C00u);
    REQUIRE(p1->words[1] == 0x9C400000u);
    REQUIRE(p2->words[0] == 0x40B00700u);
    REQUIRE(p2->words[1] == 0xFFFFFFFFu);
    REQUIRE(queue.empty());
}