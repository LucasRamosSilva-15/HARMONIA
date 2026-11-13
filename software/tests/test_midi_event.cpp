#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/midi_event.hpp"

#include <type_traits>

using namespace harmonia::core;

TEST_CASE("Channel validation respects MIDI 0..15 bounds", "[core][midi_event]") {
    REQUIRE(is_valid_channel(0));
    REQUIRE(is_valid_channel(15));
    REQUIRE_FALSE(is_valid_channel(16));
    REQUIRE_FALSE(is_valid_channel(255));
}

TEST_CASE("Note validation respects MIDI 0..127 bounds", "[core][midi_event]") {
    REQUIRE(is_valid_note(0));
    REQUIRE(is_valid_note(60));
    REQUIRE(is_valid_note(127));
    REQUIRE_FALSE(is_valid_note(128));
    REQUIRE_FALSE(is_valid_note(255));
}

TEST_CASE("make_note_on constructs valid events and rejects invalid inputs", "[core][midi_event]") {
    const auto valid = make_note_on(0, 60, 32768);
    REQUIRE(valid.has_value());
    REQUIRE(valid->type == EventType::NoteOn);
    REQUIRE(valid->channel == 0);
    REQUIRE(valid->note == 60);
    REQUIRE(valid->velocity == 32768);
    REQUIRE(is_valid_note_event(*valid));

    REQUIRE_FALSE(make_note_on(16, 60, 1000).has_value());
    REQUIRE_FALSE(make_note_on(0, 128, 1000).has_value());
}

TEST_CASE("make_note_off constructs valid events and rejects invalid inputs", "[core][midi_event]") {
    const auto valid = make_note_off(15, 127);
    REQUIRE(valid.has_value());
    REQUIRE(valid->type == EventType::NoteOff);
    REQUIRE(valid->channel == 15);
    REQUIRE(valid->note == 127);
    REQUIRE(valid->velocity == 0);
    REQUIRE(is_valid_note_event(*valid));

    REQUIRE_FALSE(make_note_off(16, 0).has_value());
    REQUIRE_FALSE(make_note_off(0, 128).has_value());
}

TEST_CASE("is_valid_note_event rejects non-note event types and out-of-range fields", "[core][midi_event]") {
    const NoteEvent unknown_ev{EventType::Unknown, 0, 60, 100};
    const NoteEvent cc_ev{EventType::ControlChange, 0, 60, 100};
    const NoteEvent bad_chan{EventType::NoteOn, 16, 60, 100};
    const NoteEvent bad_note{EventType::NoteOff, 0, 128, 0};

    REQUIRE_FALSE(is_valid_note_event(unknown_ev));
    REQUIRE_FALSE(is_valid_note_event(cc_ev));
    REQUIRE_FALSE(is_valid_note_event(bad_chan));
    REQUIRE_FALSE(is_valid_note_event(bad_note));
}

TEST_CASE("NoteEvent is trivially copyable and hardware-independent", "[core][midi_event]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<NoteEvent>);
    STATIC_REQUIRE(std::is_nothrow_destructible_v<NoteEvent>);
}
