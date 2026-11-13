#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/key_scanner.hpp"

#include <array>

using namespace harmonia::core;

namespace {

constexpr std::uint32_t kDebounceUs = 20000; // 20 ms

KeyScanner<2, 2> make_2x2_scanner(std::uint8_t base_note = 60, std::uint16_t vel = 40000) {
    KeyScanner<2, 2> scanner;
    scanner.debouncer().set_debounce_us(kDebounceUs);
    scanner.mapper().set_channel(0);
    scanner.mapper().set_fixed_velocity(vel);
    REQUIRE(scanner.mapper().assign_chromatic(base_note));
    return scanner;
}

} // namespace

TEST_CASE("Single key press emits NoteOn after debounce window", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 40000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    // Pressiona (0,0) em t = 1000 us
    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, 1000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20999, out.data(), out.size()) == 0);

    // Atinge 20000 us desde t = 1000 us (t = 21000 us)
    const auto count = scanner.scan(matrix, 21000, out.data(), out.size());
    REQUIRE(count == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 40000});
    REQUIRE(scanner.debouncer().is_pressed(0));
}

TEST_CASE("Single key release emits NoteOff after debounce window", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 40000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(0, 1, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 61, 40000});

    // Solta (0,1) em t = 50000 us
    matrix.set_key(0, 1, false);
    REQUIRE(scanner.scan(matrix, 50000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 69999, out.data(), out.size()) == 0);

    const auto count = scanner.scan(matrix, 70000, out.data(), out.size());
    REQUIRE(count == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOff, 0, 61, 0});
    REQUIRE_FALSE(scanner.debouncer().is_pressed(1));
}

TEST_CASE("Bouncing on press resets debounce timer and emits only one NoteOn", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 32768);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(1, 0, true);
    REQUIRE(scanner.scan(matrix, 1000, out.data(), out.size()) == 0);

    // Bounce para solto em 10000 us e volta para pressionado em 15000 us
    matrix.set_key(1, 0, false);
    REQUIRE(scanner.scan(matrix, 10000, out.data(), out.size()) == 0);
    matrix.set_key(1, 0, true);
    REQUIRE(scanner.scan(matrix, 15000, out.data(), out.size()) == 0);

    // 20 ms apos 1000 us (21000) ainda nao deve disparar porque reiniciou em 15000
    REQUIRE(scanner.scan(matrix, 21000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 34999, out.data(), out.size()) == 0);

    // Dispara exatamente em 15000 + 20000 = 35000
    REQUIRE(scanner.scan(matrix, 35000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 62, 32768});

    // Scans subsequentes sem mudanca nao geram duplicatas
    REQUIRE(scanner.scan(matrix, 50000, out.data(), out.size()) == 0);
}

TEST_CASE("Bouncing on release resets debounce timer and emits only one NoteOff", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 32768);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(1, 1, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 1);

    // Comeca a soltar em 30000, faz bounce para true em 35000 e volta para false em 40000
    matrix.set_key(1, 1, false);
    REQUIRE(scanner.scan(matrix, 30000, out.data(), out.size()) == 0);
    matrix.set_key(1, 1, true);
    REQUIRE(scanner.scan(matrix, 35000, out.data(), out.size()) == 0);
    matrix.set_key(1, 1, false);
    REQUIRE(scanner.scan(matrix, 40000, out.data(), out.size()) == 0);

    REQUIRE(scanner.scan(matrix, 59999, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 60000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOff, 0, 63, 0});
}

TEST_CASE("Simultaneous key presses emit distinct NoteOn events", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 50000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(0, 0, true);
    matrix.set_key(1, 1, true);
    REQUIRE(scanner.scan(matrix, 100, out.data(), out.size()) == 0);

    const auto count = scanner.scan(matrix, 20100, out.data(), out.size());
    REQUIRE(count == 2);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 50000});
    REQUIRE(out[1] == NoteEvent{EventType::NoteOn, 0, 63, 50000});
}

TEST_CASE("Long hold does not emit duplicate NoteOn events", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 30000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 1);

    // Mantem pressionada por varios segundos
    for (std::uint32_t t = 40000; t <= 2000000; t += 20000) {
        REQUIRE(scanner.scan(matrix, t, out.data(), out.size()) == 0);
    }
    REQUIRE(scanner.debouncer().is_pressed(0));
}

TEST_CASE("Glitch shorter than debounce time is ignored", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 30000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, 1000, out.data(), out.size()) == 0);

    matrix.set_key(0, 0, false);
    REQUIRE(scanner.scan(matrix, 10000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 50000, out.data(), out.size()) == 0);
    REQUIRE_FALSE(scanner.debouncer().is_pressed(0));
}

TEST_CASE("Chord sequence emits expected NoteOn and NoteOff ordering", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 42000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    // Acorde 1: notas 60 e 61
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 2);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 42000});
    REQUIRE(out[1] == NoteEvent{EventType::NoteOn, 0, 61, 42000});

    // Troca para acorde 2: solta 60, pressiona 62
    matrix.set_key(0, 0, false);
    matrix.set_key(1, 0, true);
    REQUIRE(scanner.scan(matrix, 100000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 120000, out.data(), out.size()) == 2);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOff, 0, 60, 0});
    REQUIRE(out[1] == NoteEvent{EventType::NoteOn, 0, 62, 42000});
}

TEST_CASE("Matrix bounds and invalid configurations are handled safely", "[core][acquisition]") {
    KeyMatrixState<2, 2> matrix;
    REQUIRE_FALSE(matrix.configure(0, 2));
    REQUIRE_FALSE(matrix.configure(2, 0));
    REQUIRE_FALSE(matrix.configure(3, 2));
    REQUIRE_FALSE(matrix.configure(2, 3));
    REQUIRE(matrix.configure(2, 2));

    REQUIRE_FALSE(matrix.set_key(2, 0, true));
    REQUIRE_FALSE(matrix.set_key(0, 2, true));
    REQUIRE_FALSE(matrix.get_key(2, 0));
    REQUIRE_FALSE(matrix.index_of(2, 0).has_value());
    REQUIRE_FALSE(matrix.coord_of(4).has_value());
}

TEST_CASE("Custom note mapping, channel and unmapped keys work as configured", "[core][acquisition]") {
    KeyScanner<2, 2> scanner;
    scanner.debouncer().set_debounce_us(kDebounceUs);
    REQUIRE(scanner.mapper().set_channel(3));
    REQUIRE_FALSE(scanner.mapper().set_channel(16));
    scanner.mapper().set_fixed_velocity(65535);

    REQUIRE(scanner.mapper().set_note(0, 48));
    REQUIRE(scanner.mapper().set_note(1, KeyMapper<4>::kUnmappedNote));
    REQUIRE_FALSE(scanner.mapper().set_note(0, 128));

    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true); // unmapped

    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    const auto count = scanner.scan(matrix, 20000, out.data(), out.size());
    REQUIRE(count == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 3, 48, 65535});
}

TEST_CASE("Reset restores initial state cleanly", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 32768);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 1);
    REQUIRE(scanner.debouncer().is_pressed(0));

    scanner.reset(false);
    REQUIRE_FALSE(scanner.debouncer().is_pressed(0));

    // Apos reset para solto, manter matriz pressionada permite nova deteccao limpa
    REQUIRE(scanner.scan(matrix, 30000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 50000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 32768});
}

TEST_CASE("Debouncer is independent of regular sampling intervals and handles uint32 wrap-around", "[core][acquisition]") {
    auto scanner = make_2x2_scanner(60, 32768);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 8> out{};

    // Comeca proximo ao overflow de uint32_t (UINT32_MAX - 5000)
    const std::uint32_t t_start = 0xFFFFEC77u; // 4294962295
    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, t_start, out.data(), out.size()) == 0);

    // Amostragem irregular: 7 ms depois (apos wrap-around) -> ainda < 20 ms
    REQUIRE(scanner.scan(matrix, t_start + 7000u, out.data(), out.size()) == 0);
    // 25 ms depois do inicio (salto irregular de 18 ms) -> deve confirmar
    REQUIRE(scanner.scan(matrix, t_start + 25000u, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 32768});
}
