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

TEST_CASE("Small caller buffer does not lose events and drains on next call", "[core][acquisition][integrity]") {
    auto scanner = make_2x2_scanner(60, 40000);
    KeyMatrixState<2, 2> matrix(2, 2);

    // Pressiona todas as 4 teclas simultaneamente
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true);
    matrix.set_key(1, 0, true);
    matrix.set_key(1, 1, true);

    std::array<NoteEvent, 2> small_buf{};
    REQUIRE(scanner.scan_with_status(matrix, 0, small_buf.data(), small_buf.size()).emitted == 0);

    // Primeiro scan com buffer de apenas 2 posições: recebe 2 e mantém 2 pendentes sem overflow
    const auto r1 = scanner.scan_with_status(matrix, 20000, small_buf.data(), small_buf.size());
    REQUIRE(r1.emitted == 2);
    REQUIRE_FALSE(r1.overflow);
    REQUIRE(r1.pending_count == 2);
    REQUIRE(small_buf[0] == NoteEvent{EventType::NoteOn, 0, 60, 40000});
    REQUIRE(small_buf[1] == NoteEvent{EventType::NoteOn, 0, 61, 40000});

    // Drena os 2 restantes sem perder nenhum
    const auto r2 = scanner.scan_with_status(matrix, 25000, small_buf.data(), small_buf.size());
    REQUIRE(r2.emitted == 2);
    REQUIRE_FALSE(r2.overflow);
    REQUIRE(r2.pending_count == 0);
    REQUIRE(small_buf[0] == NoteEvent{EventType::NoteOn, 0, 62, 40000});
    REQUIRE(small_buf[1] == NoteEvent{EventType::NoteOn, 0, 63, 40000});
}

TEST_CASE("Extreme internal queue saturation signals overflow and preserves NoteOff", "[core][acquisition][integrity]") {
    auto scanner = make_2x2_scanner(60, 40000);
    KeyMatrixState<2, 2> matrix(2, 2);

    // Satura a fila interna (capacidade = 8 para matriz 2x2) passando max_events = 0
    // Ciclo 1: 4 NoteOn (fila = 4)
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true);
    matrix.set_key(1, 0, true);
    matrix.set_key(1, 1, true);
    REQUIRE(scanner.scan_with_status(matrix, 0, nullptr, 0).emitted == 0);
    const auto s1 = scanner.scan_with_status(matrix, 20000, nullptr, 0);
    REQUIRE_FALSE(s1.overflow);
    REQUIRE(s1.pending_count == 4);

    // Ciclo 2: solta 4 teclas -> 4 NoteOff (fila = 8, cheia)
    matrix.clear();
    REQUIRE(scanner.scan_with_status(matrix, 30000, nullptr, 0).emitted == 0);
    const auto s2 = scanner.scan_with_status(matrix, 50000, nullptr, 0);
    REQUIRE_FALSE(s2.overflow);
    REQUIRE(s2.pending_count == 8);

    // Ciclo 3: pressiona 4 teclas novamente com fila cheia -> deve sinalizar overflow e descartar NoteOn
    matrix.set_key(0, 0, true);
    matrix.set_key(0, 1, true);
    matrix.set_key(1, 0, true);
    matrix.set_key(1, 1, true);
    REQUIRE(scanner.scan_with_status(matrix, 60000, nullptr, 0).emitted == 0);
    const auto s3 = scanner.scan_with_status(matrix, 80000, nullptr, 0);
    REQUIRE(s3.overflow);
    REQUIRE(s3.dropped_note_on == 4);
    REQUIRE(s3.pending_count == 8);

    // Drena tudo e verifica que os 4 NoteOff preservados foram entregues e o sistema se recuperou
    std::array<NoteEvent, 8> out{};
    const auto drained = scanner.drain(out.data(), out.size());
    REQUIRE(drained == 8);
    REQUIRE(scanner.pending_count() == 0);

    std::size_t note_off_count = 0;
    for (std::size_t i = 0; i < drained; ++i) {
        if (out[i].type == EventType::NoteOff) {
            ++note_off_count;
        }
    }
    REQUIRE(note_off_count == 4);

    // Recuperação: soltar as teclas cujo NoteOn foi descartado não gera NoteOff órfão
    matrix.clear();
    REQUIRE(scanner.scan_with_status(matrix, 90000, out.data(), out.size()).emitted == 0);
    const auto s4 = scanner.scan_with_status(matrix, 110000, out.data(), out.size());
    REQUIRE(s4.emitted == 0);
}

TEST_CASE("Remapping MIDI note while key is held still releases the original note", "[core][acquisition][integrity]") {
    auto scanner = make_2x2_scanner(60, 45000);
    KeyMatrixState<2, 2> matrix(2, 2);
    std::array<NoteEvent, 4> out{};

    // Pressiona tecla 0 (nota 60, canal 0)
    matrix.set_key(0, 0, true);
    REQUIRE(scanner.scan(matrix, 0, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 20000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOn, 0, 60, 45000});

    // Muda o mapeamento e canal enquanto a tecla continua pressionada
    REQUIRE(scanner.mapper().set_note(0, 72));
    REQUIRE(scanner.mapper().set_channel(2));

    // Solta tecla 0: deve emitir NoteOff para nota 60, canal 0 (e não 72, canal 2!)
    matrix.set_key(0, 0, false);
    REQUIRE(scanner.scan(matrix, 40000, out.data(), out.size()) == 0);
    REQUIRE(scanner.scan(matrix, 60000, out.data(), out.size()) == 1);
    REQUIRE(out[0] == NoteEvent{EventType::NoteOff, 0, 60, 0});
}
