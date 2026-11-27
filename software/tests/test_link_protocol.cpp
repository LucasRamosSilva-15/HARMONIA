#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/link_protocol.hpp"
#include "harmonia/core/spsc_ring_buffer.hpp"

#include <array>
#include <cstdint>
#include <thread>
#include <variant>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace harmonia::core;

TEST_CASE("CRC-16/CCITT-FALSE matches standard test vector 123456789", "[core][link]") {
    const std::uint8_t msg[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    REQUIRE(crc16_ccitt_false(msg, sizeof(msg)) == 0x29B1u);
}

TEST_CASE("COBS framing handles 0x00, 0xAA, 0x55 and 0xFF transparently", "[core][link]") {
    // Pacote UMP MIDI 2.0 com bytes 0x00, 0xAA, 0x55, 0xFF no payload
    const ControlEvent cc{0, 0, 0x00AA55FFu, 0};
    const auto enc = encode_midi2_control(cc, 0);
    REQUIRE(enc.ok);

    std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> wire{};
    const std::size_t n = encode_ump_frame(enc.packet, 42, wire.data(), wire.size());
    REQUIRE(n > 0);
    REQUIRE(n <= kLinkMaxEncodedFrameBytes);
    REQUIRE(wire[n - 1] == 0x00u);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        REQUIRE(wire[i] != 0x00u); // Nenhum 0x00 dentro do corpo COBS
    }

    // Alimentacao byte-a-byte
    LinkFrameDecoder dec;
    std::optional<LinkDecodedFrame> out;
    for (std::size_t i = 0; i < n; ++i) {
        const auto res = dec.feed_byte(wire[i]);
        if (i + 1 < n) {
            REQUIRE_FALSE(res.has_value());
        } else {
            out = res;
        }
    }

    REQUIRE(out.has_value());
    REQUIRE(out->msg_type == LinkMessageType::UmpData);
    REQUIRE(out->seq == 42);
    REQUIRE(out->ump == enc.packet);
    REQUIRE(std::get<ControlEvent>(out->decoded_ump.event).value_32 == 0x00AA55FFu);
    REQUIRE(dec.stats().frames_ok == 1);
    REQUIRE(dec.stats().crc_errors == 0);
    REQUIRE(dec.stats().framing_errors == 0);
}

TEST_CASE("LinkFrameDecoder supports MIDI 1.0 UMP, Heartbeat, Ack and chunked feeding", "[core][link]") {
    const auto note_m1 = encode_midi1_note(*make_note_on(1, 60, 40000), 2);
    REQUIRE(note_m1.ok);

    std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> f1{}, f2{}, f3{};
    const std::size_t n1 = encode_ump_frame(note_m1.packet, 0, f1.data(), f1.size());
    const std::size_t n2 = encode_heartbeat_frame(1, 0x12345678u, f2.data(), f2.size());
    const std::size_t n3 = encode_ack_frame(2, f3.data(), f3.size());
    REQUIRE(n1 > 0);
    REQUIRE(n2 > 0);
    REQUIRE(n3 > 0);

    std::vector<std::uint8_t> stream;
    stream.insert(stream.end(), f1.begin(), f1.begin() + static_cast<std::ptrdiff_t>(n1));
    stream.insert(stream.end(), f2.begin(), f2.begin() + static_cast<std::ptrdiff_t>(n2));
    stream.insert(stream.end(), f3.begin(), f3.begin() + static_cast<std::ptrdiff_t>(n3));

    LinkFrameDecoder dec;
    std::vector<LinkDecodedFrame> decoded;
    // Alimenta em chunks irregulares de 3 bytes
    for (std::size_t pos = 0; pos < stream.size(); pos += 3) {
        const std::size_t chunk = (pos + 3 <= stream.size()) ? 3 : (stream.size() - pos);
        dec.feed(&stream[pos], chunk, [&](const LinkDecodedFrame& f) { decoded.push_back(f); });
    }

    REQUIRE(decoded.size() == 3);
    REQUIRE(decoded[0].msg_type == LinkMessageType::UmpData);
    REQUIRE(decoded[1].msg_type == LinkMessageType::Heartbeat);
    REQUIRE(decoded[1].heartbeat_arg == 0x12345678u);
    REQUIRE(decoded[2].msg_type == LinkMessageType::Ack);
    REQUIRE(dec.stats().seq_gaps == 0);
}

TEST_CASE("LinkFrameDecoder rejects corrupted CRC, noise, invalid UMP and detects seq gaps", "[core][link]") {
    const auto note_on = encode_midi2_note(*make_note_on(0, 60, 40000), 0);
    std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> f10{}, f11{}, f13{};
    const std::size_t n10 = encode_ump_frame(note_on.packet, 10, f10.data(), f10.size());
    const std::size_t n11 = encode_ump_frame(note_on.packet, 11, f11.data(), f11.size());
    const std::size_t n13 = encode_ump_frame(note_on.packet, 13, f13.data(), f13.size()); // salto 11 -> 13

    // Corrompe 1 bit no payload de f11 (antes do delimitador 0x00)
    f11[n11 - 3] ^= 0x01u;

    LinkFrameDecoder dec;
    std::vector<LinkDecodedFrame> ok_frames;

    // Lixo antes + delimitador 0x00 + frame 10 valido
    const std::uint8_t noise[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00};
    dec.feed(noise, sizeof(noise), [&](const LinkDecodedFrame& f) { ok_frames.push_back(f); });
    dec.feed(f10.data(), n10, [&](const LinkDecodedFrame& f) { ok_frames.push_back(f); });
    // Frame 11 corrompido
    dec.feed(f11.data(), n11, [&](const LinkDecodedFrame& f) { ok_frames.push_back(f); });
    // Frame 13 valido (ressincroniza e detecta seq_gap de 10 -> 13)
    dec.feed(f13.data(), n13, [&](const LinkDecodedFrame& f) { ok_frames.push_back(f); });

    REQUIRE(ok_frames.size() == 2);
    REQUIRE(ok_frames[0].seq == 10);
    REQUIRE(ok_frames[1].seq == 13);
    REQUIRE(dec.stats().framing_errors == 1); // ruido antes do primeiro 0x00
    REQUIRE(dec.stats().crc_errors == 1);     // f11 corrompido
    REQUIRE(dec.stats().seq_gaps == 1);       // 10 -> 13

    // Frame com CRC valido mas UMP semanticamente invalido (CC byte3 != 0)
    UmpPacket bad_ump{};
    bad_ump.word_count = 2;
    bad_ump.words[0] = 0x40B00701u;
    bad_ump.words[1] = 0xFFFFFFFFu;
    std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> f_bad{};
    const std::size_t n_bad = encode_ump_frame(bad_ump, 14, f_bad.data(), f_bad.size());
    REQUIRE(n_bad > 0);
    dec.feed(f_bad.data(), n_bad, [&](const LinkDecodedFrame& f) { ok_frames.push_back(f); });
    REQUIRE(ok_frames.size() == 2);
    REQUIRE(dec.stats().invalid_ump_errors == 1);
}

TEST_CASE("SpscRingBuffer concurrent producer/consumer transfers all items without loss", "[core][spsc]") {
    SpscRingBuffer<std::uint32_t, 64> rb;
    REQUIRE(rb.capacity() == 63);
    REQUIRE(rb.empty());

    constexpr std::uint32_t kTotal = 20000;
    std::thread producer([&]() {
        for (std::uint32_t i = 0; i < kTotal; ++i) {
            while (!rb.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::uint32_t expected = 0;
    std::thread consumer([&]() {
        while (expected < kTotal) {
            std::uint32_t val = 0;
            if (rb.pop(val)) {
                REQUIRE(val == expected);
                ++expected;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
    REQUIRE(expected == kTotal);
    REQUIRE(rb.empty());
}

#ifdef __linux__
TEST_CASE("POSIX PTY loopback recovers 100% of valid frames and rejects corrupted ones", "[core][link][pty]") {
    int master_fd = -1;
    int slave_fd = -1;
    REQUIRE(::openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == 0);

    struct termios tio{};
    REQUIRE(::tcgetattr(master_fd, &tio) == 0);
    ::cfmakeraw(&tio);
    REQUIRE(::tcsetattr(master_fd, TCSANOW, &tio) == 0);
    REQUIRE(::tcgetattr(slave_fd, &tio) == 0);
    ::cfmakeraw(&tio);
    REQUIRE(::tcsetattr(slave_fd, TCSANOW, &tio) == 0);

    const int flags = ::fcntl(slave_fd, F_GETFL, 0);
    REQUIRE(::fcntl(slave_fd, F_SETFL, flags | O_NONBLOCK) == 0);

    LinkFrameDecoder decoder;
    SpscRingBuffer<LinkDecodedFrame, 64> rx_queue;

    // Envia 20 frames validos + 1 frame corrompido no meio via master_fd
    std::uint8_t seq = 0;
    for (int i = 0; i < 20; ++i) {
        const auto ump = (i % 2 == 0)
            ? encode_midi2_note(*make_note_on(0, static_cast<std::uint8_t>(60 + (i % 4)), 40000), 0)
            : encode_midi2_control(ControlEvent{0, 7, static_cast<std::uint32_t>(i * 100000), 0}, 0);
        std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> buf{};
        const std::size_t n = encode_ump_frame(ump.packet, seq++, buf.data(), buf.size());
        REQUIRE(::write(master_fd, buf.data(), n) == static_cast<ssize_t>(n));

        if (i == 9) {
            // Injeta frame corrompido
            std::array<std::uint8_t, kLinkMaxEncodedFrameBytes> bad{};
            const std::size_t nb = encode_heartbeat_frame(99, 0xDEADBEEFu, bad.data(), bad.size());
            bad[nb - 2] ^= 0xFFu;
            REQUIRE(::write(master_fd, bad.data(), nb) == static_cast<ssize_t>(nb));
        }
    }

    // Drena slave_fd
    std::array<std::uint8_t, 128> rx_raw{};
    for (int tries = 0; tries < 50 && decoder.stats().frames_ok < 20; ++tries) {
        const ssize_t r = ::read(slave_fd, rx_raw.data(), rx_raw.size());
        if (r > 0) {
            decoder.feed(rx_raw.data(), static_cast<std::size_t>(r), [&](const LinkDecodedFrame& f) {
                REQUIRE(rx_queue.push(f));
            });
        } else {
            usleep(1000);
        }
    }

    ::close(slave_fd);
    ::close(master_fd);

    REQUIRE(decoder.stats().frames_ok == 20);
    REQUIRE(decoder.stats().crc_errors == 1);
    REQUIRE(decoder.stats().seq_gaps == 0);
    REQUIRE(rx_queue.size_approx() == 20);
}
#endif