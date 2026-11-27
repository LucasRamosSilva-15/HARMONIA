#include "harmonia/core/link_protocol.hpp"
#include "harmonia/core/spsc_ring_buffer.hpp"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <variant>

namespace {

bool configure_raw_serial(int fd) {
    struct termios tio{};
    if (::tcgetattr(fd, &tio) != 0) {
        return false;
    }
    ::cfmakeraw(&tio);
#ifdef B921600
    ::cfsetispeed(&tio, B921600);
    ::cfsetospeed(&tio, B921600);
#endif
    tio.c_cflag |= (CLOCAL | CREAD);
    return ::tcsetattr(fd, TCSANOW, &tio) == 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Uso: %s <dispositivo_serial_ou_pty>\n", argv[0]);
        return 2;
    }

    const int fd = ::open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::fprintf(stderr, "Erro ao abrir %s: %s\n", argv[1], std::strerror(errno));
        return 1;
    }
    (void)configure_raw_serial(fd);

    harmonia::core::LinkFrameDecoder decoder;
    harmonia::core::SpscRingBuffer<harmonia::core::LinkDecodedFrame, 128> queue;
    std::array<std::uint8_t, 256> buf{};

    while (true) {
        const ssize_t n = ::read(fd, buf.data(), buf.size());
        if (n <= 0) {
            break;
        }
        decoder.feed(buf.data(), static_cast<std::size_t>(n), [&](const harmonia::core::LinkDecodedFrame& frame) {
            (void)queue.push(frame);
        });

        harmonia::core::LinkDecodedFrame item{};
        while (queue.pop(item)) {
            if (item.msg_type == harmonia::core::LinkMessageType::Heartbeat) {
                std::printf("[LINK_RX] seq=%u HEARTBEAT arg=%lu\n",
                            static_cast<unsigned>(item.seq),
                            static_cast<unsigned long>(item.heartbeat_arg));
            } else if (item.msg_type == harmonia::core::LinkMessageType::UmpData) {
                if (std::holds_alternative<harmonia::core::NoteEvent>(item.decoded_ump.event)) {
                    const auto& nev = std::get<harmonia::core::NoteEvent>(item.decoded_ump.event);
                    std::printf("[LINK_RX] seq=%u %s ch=%u note=%u vel=%u\n",
                                static_cast<unsigned>(item.seq),
                                nev.type == harmonia::core::EventType::NoteOn ? "NOTE_ON" : "NOTE_OFF",
                                static_cast<unsigned>(nev.channel),
                                static_cast<unsigned>(nev.note),
                                static_cast<unsigned>(nev.velocity));
                } else {
                    const auto& cev = std::get<harmonia::core::ControlEvent>(item.decoded_ump.event);
                    std::printf("[LINK_RX] seq=%u CC ch=%u cc=%u val32=%lu\n",
                                static_cast<unsigned>(item.seq),
                                static_cast<unsigned>(cev.channel),
                                static_cast<unsigned>(cev.cc_number),
                                static_cast<unsigned long>(cev.value_32));
                }
            }
        }
    }

    const auto& st = decoder.stats();
    std::printf("[LINK_STATS] ok=%lu crc_err=%lu frame_err=%lu ump_err=%lu seq_gaps=%lu\n",
                static_cast<unsigned long>(st.frames_ok),
                static_cast<unsigned long>(st.crc_errors),
                static_cast<unsigned long>(st.framing_errors),
                static_cast<unsigned long>(st.invalid_ump_errors),
                static_cast<unsigned long>(st.seq_gaps));

    ::close(fd);
    return 0;
}