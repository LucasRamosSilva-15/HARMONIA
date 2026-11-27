#include "harmonia/core/link_protocol.hpp"

#include <array>

namespace harmonia::core {

namespace {

std::size_t cobs_encode(
    const std::uint8_t* src,
    std::size_t src_len,
    std::uint8_t* dst,
    std::size_t dst_cap
) noexcept {
    if (src == nullptr || dst == nullptr || dst_cap < src_len + 2u) {
        return 0;
    }

    std::size_t read_idx = 0;
    std::size_t write_idx = 1;
    std::size_t code_idx = 0;
    std::uint8_t code = 1;

    while (read_idx < src_len) {
        if (src[read_idx] == 0u) {
            dst[code_idx] = code;
            code = 1u;
            code_idx = write_idx++;
            ++read_idx;
        } else {
            dst[write_idx++] = src[read_idx++];
            ++code;
            if (code == 0xFFu) {
                dst[code_idx] = code;
                code = 1u;
                code_idx = write_idx++;
            }
        }
    }

    dst[code_idx] = code;
    dst[write_idx++] = 0x00u; // Delimitador de fim de quadro
    return write_idx;
}

std::size_t cobs_decode(
    const std::uint8_t* src,
    std::size_t src_len,
    std::uint8_t* dst,
    std::size_t dst_cap
) noexcept {
    if (src == nullptr || dst == nullptr || src_len == 0u) {
        return 0;
    }

    std::size_t read_idx = 0;
    std::size_t write_idx = 0;

    while (read_idx < src_len) {
        const std::uint8_t code = src[read_idx++];
        if (code == 0u) {
            return 0; // Byte 0 inesperado dentro do corpo COBS
        }
        const std::size_t copy_count = static_cast<std::size_t>(code - 1u);
        if (read_idx + copy_count > src_len || write_idx + copy_count > dst_cap) {
            return 0; // Quadro truncado ou overflow
        }
        for (std::size_t i = 0; i < copy_count; ++i) {
            dst[write_idx++] = src[read_idx++];
        }
        if (code < 0xFFu && read_idx < src_len) {
            if (write_idx >= dst_cap) {
                return 0;
            }
            dst[write_idx++] = 0u;
        }
    }

    return write_idx;
}

std::size_t build_and_encode_frame(
    LinkMessageType msg_type,
    std::uint8_t seq,
    const std::uint8_t* payload,
    std::uint8_t payload_len,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept {
    if (payload_len > kLinkMaxPayloadBytes || out_buf == nullptr) {
        return 0;
    }

    std::array<std::uint8_t, kLinkMaxRawFrameBytes> raw{};
    raw[0] = kLinkMagic0;
    raw[1] = kLinkMagic1;
    raw[2] = kLinkProtocolVersion;
    raw[3] = static_cast<std::uint8_t>(msg_type);
    raw[4] = seq;
    raw[5] = payload_len;

    for (std::size_t i = 0; i < payload_len; ++i) {
        raw[kLinkHeaderBytes + i] = payload[i];
    }

    const std::size_t crc_input_len = kLinkHeaderBytes + payload_len;
    const std::uint16_t crc = crc16_ccitt_false(raw.data(), crc_input_len);
    raw[crc_input_len] = static_cast<std::uint8_t>((crc >> 8) & 0xFFu);
    raw[crc_input_len + 1u] = static_cast<std::uint8_t>(crc & 0xFFu);

    return cobs_encode(raw.data(), crc_input_len + kLinkCrcBytes, out_buf, out_cap);
}

} // namespace

std::uint16_t crc16_ccitt_false(const std::uint8_t* data, std::size_t len) noexcept {
    if (data == nullptr) {
        return 0xFFFFu;
    }
    std::uint16_t crc = 0xFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

std::size_t encode_ump_frame(
    const UmpPacket& pkt,
    std::uint8_t seq,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept {
    if (pkt.word_count == 0u || pkt.word_count > 4u) {
        return 0;
    }

    std::array<std::uint8_t, kLinkMaxPayloadBytes> payload{};
    for (std::size_t i = 0; i < pkt.word_count; ++i) {
        ump_word_to_be_bytes(pkt.words[i], &payload[i * 4u]);
    }

    return build_and_encode_frame(
        LinkMessageType::UmpData,
        seq,
        payload.data(),
        static_cast<std::uint8_t>(pkt.word_count * 4u),
        out_buf,
        out_cap
    );
}

std::size_t encode_heartbeat_frame(
    std::uint8_t seq,
    std::uint32_t arg,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept {
    std::array<std::uint8_t, 4> payload{};
    ump_word_to_be_bytes(arg, payload.data());
    return build_and_encode_frame(
        LinkMessageType::Heartbeat,
        seq,
        payload.data(),
        4u,
        out_buf,
        out_cap
    );
}

std::size_t encode_ack_frame(
    std::uint8_t seq,
    std::uint8_t* out_buf,
    std::size_t out_cap
) noexcept {
    return build_and_encode_frame(
        LinkMessageType::Ack,
        seq,
        nullptr,
        0u,
        out_buf,
        out_cap
    );
}

void LinkFrameDecoder::reset() noexcept {
    rx_len_ = 0;
    overflowed_ = false;
    has_last_seq_ = false;
    last_seq_ = 0;
    stats_ = LinkDecoderStats{};
}

std::optional<LinkDecodedFrame> LinkFrameDecoder::feed_byte(std::uint8_t byte) noexcept {
    if (byte == 0x00u) {
        if (rx_len_ == 0u && !overflowed_) {
            // Delimitador ocioso / repetido: ignora sem contar erro
            return std::nullopt;
        }
        if (overflowed_) {
            rx_len_ = 0;
            overflowed_ = false;
            ++stats_.framing_errors;
            return std::nullopt;
        }
        const auto res = process_complete_frame();
        rx_len_ = 0;
        return res;
    }

    if (overflowed_) {
        return std::nullopt;
    }

    if (rx_len_ >= rx_buf_.size()) {
        overflowed_ = true;
        return std::nullopt;
    }

    rx_buf_[rx_len_++] = byte;
    return std::nullopt;
}

std::optional<LinkDecodedFrame> LinkFrameDecoder::process_complete_frame() noexcept {
    std::array<std::uint8_t, kLinkMaxRawFrameBytes> raw{};
    const std::size_t raw_len = cobs_decode(rx_buf_.data(), rx_len_, raw.data(), raw.size());
    if (raw_len < kLinkHeaderBytes + kLinkCrcBytes) {
        ++stats_.framing_errors;
        return std::nullopt;
    }

    if (raw[0] != kLinkMagic0 || raw[1] != kLinkMagic1 || raw[2] != kLinkProtocolVersion) {
        ++stats_.framing_errors;
        return std::nullopt;
    }

    const std::uint8_t msg_type_raw = raw[3];
    const std::uint8_t seq = raw[4];
    const std::uint8_t payload_len = raw[5];

    if (payload_len > kLinkMaxPayloadBytes ||
        raw_len != kLinkHeaderBytes + static_cast<std::size_t>(payload_len) + kLinkCrcBytes) {
        ++stats_.framing_errors;
        return std::nullopt;
    }

    const std::size_t crc_offset = kLinkHeaderBytes + static_cast<std::size_t>(payload_len);
    const std::uint16_t expected_crc = crc16_ccitt_false(raw.data(), crc_offset);
    const std::uint16_t rx_crc = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(raw[crc_offset]) << 8) |
        static_cast<std::uint16_t>(raw[crc_offset + 1u])
    );

    if (rx_crc != expected_crc) {
        ++stats_.crc_errors;
        return std::nullopt;
    }

    LinkDecodedFrame out{};
    out.seq = seq;

    if (msg_type_raw == static_cast<std::uint8_t>(LinkMessageType::UmpData)) {
        if (payload_len == 0u || (payload_len % 4u) != 0u) {
            ++stats_.invalid_ump_errors;
            return std::nullopt;
        }
        out.msg_type = LinkMessageType::UmpData;
        out.ump.word_count = static_cast<std::uint8_t>(payload_len / 4u);
        for (std::size_t i = 0; i < out.ump.word_count; ++i) {
            out.ump.words[i] = ump_word_from_be_bytes(&raw[kLinkHeaderBytes + i * 4u]);
        }
        const auto dec = decode_ump_packet(out.ump);
        if (!dec.ok) {
            ++stats_.invalid_ump_errors;
            return std::nullopt;
        }
        out.decoded_ump = dec.message;
    } else if (msg_type_raw == static_cast<std::uint8_t>(LinkMessageType::Heartbeat)) {
        if (payload_len != 4u) {
            ++stats_.framing_errors;
            return std::nullopt;
        }
        out.msg_type = LinkMessageType::Heartbeat;
        out.heartbeat_arg = ump_word_from_be_bytes(&raw[kLinkHeaderBytes]);
    } else if (msg_type_raw == static_cast<std::uint8_t>(LinkMessageType::Ack)) {
        if (payload_len != 0u) {
            ++stats_.framing_errors;
            return std::nullopt;
        }
        out.msg_type = LinkMessageType::Ack;
    } else {
        ++stats_.framing_errors;
        return std::nullopt;
    }

    if (has_last_seq_) {
        const std::uint8_t expected_seq = static_cast<std::uint8_t>(last_seq_ + 1u);
        if (seq != expected_seq) {
            ++stats_.seq_gaps;
        }
    } else {
        has_last_seq_ = true;
    }
    last_seq_ = seq;
    ++stats_.frames_ok;
    return out;
}

} // namespace harmonia::core