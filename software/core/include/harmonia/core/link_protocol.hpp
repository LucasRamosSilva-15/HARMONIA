#pragma once

#include "harmonia/core/ump.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmonia::core {

inline constexpr std::uint8_t kLinkMagic0 = 0xAAu;
inline constexpr std::uint8_t kLinkMagic1 = 0x55u;
inline constexpr std::uint8_t kLinkProtocolVersion = 0x01u;
inline constexpr std::size_t kLinkMaxPayloadBytes = 16u;
inline constexpr std::size_t kLinkHeaderBytes = 6u; // magic(2) + ver(1) + type(1) + seq(1) + len(1)
inline constexpr std::size_t kLinkCrcBytes = 2u;
inline constexpr std::size_t kLinkMaxRawFrameBytes = kLinkHeaderBytes + kLinkMaxPayloadBytes + kLinkCrcBytes; // 24
inline constexpr std::size_t kLinkMaxEncodedFrameBytes = kLinkMaxRawFrameBytes + 2u; // COBS + 0x00 = 26

enum class LinkMessageType : std::uint8_t {
    UmpData = 0x01,
    Heartbeat = 0x02,
    Ack = 0x03
};

struct LinkDecodedFrame {
    LinkMessageType msg_type{LinkMessageType::UmpData};
    std::uint8_t seq{0};
    UmpPacket ump{};
    DecodedUmpMessage decoded_ump{};
    std::uint32_t heartbeat_arg{0};
};

struct LinkDecoderStats {
    std::uint32_t frames_ok{0};
    std::uint32_t crc_errors{0};
    std::uint32_t framing_errors{0};
    std::uint32_t invalid_ump_errors{0};
    std::uint32_t seq_gaps{0};
};

/// CRC-16/CCITT-FALSE (polinomio 0x1021, inicial 0xFFFF, sem reflexao/XOR final).
[[nodiscard]] std::uint16_t crc16_ccitt_false(const std::uint8_t* data, std::size_t len) noexcept;

/// Codifica um UmpPacket em quadro binario com cabecalho, CRC-16 e framing COBS + 0x00.
/// Retorna o numero de bytes escritos em `out_buf` (0 se invalido ou capacidade insuficiente).
[[nodiscard]] std::size_t encode_ump_frame(
    const UmpPacket& pkt,
    std::uint8_t seq,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept;

/// Codifica um quadro Heartbeat/Ping (payload de 4 bytes Big-Endian com argumento/uptime).
[[nodiscard]] std::size_t encode_heartbeat_frame(
    std::uint8_t seq,
    std::uint32_t arg,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept;

/// Codifica um quadro Ack (payload vazio).
[[nodiscard]] std::size_t encode_ack_frame(
    std::uint8_t seq,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept;

/// Decodificador incremental byte-a-byte (sem alocacao dinamica) com COBS, CRC-16 e
/// validacao UMP + deteccao de saltos de sequencia.
class LinkFrameDecoder {
public:
    constexpr LinkFrameDecoder() noexcept = default;

    void reset() noexcept;

    /// Alimenta 1 byte ao decodificador. Retorna um frame valido se um delimitador 0x00
    /// concluiu um quadro integro e valido.
    [[nodiscard]] std::optional<LinkDecodedFrame> feed_byte(std::uint8_t byte) noexcept;

    /// Alimenta um bloco de bytes, invocando `on_frame(const LinkDecodedFrame&)` para cada frame valido.
    template <typename Callback>
    void feed(const std::uint8_t* data, std::size_t len, Callback&& on_frame) {
        if (data == nullptr) {
            return;
        }
        for (std::size_t i = 0; i < len; ++i) {
            const auto frame = feed_byte(data[i]);
            if (frame.has_value()) {
                on_frame(*frame);
            }
        }
    }

    [[nodiscard]] const LinkDecoderStats& stats() const noexcept { return stats_; }

private:
    std::optional<LinkDecodedFrame> process_complete_frame() noexcept;

    std::array<std::uint8_t, kLinkMaxRawFrameBytes + 1u> rx_buf_{}; // max COBS body = 25 bytes
    std::size_t rx_len_{0};
    bool overflowed_{false};
    bool has_last_seq_{false};
    std::uint8_t last_seq_{0};
    LinkDecoderStats stats_{};
};

} // namespace harmonia::core