#pragma once

#include "harmonia/core/analog_control.hpp"
#include "harmonia/core/midi_event.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>

namespace harmonia::core {

/// Tipos de mensagem UMP (4 bits mais significativos da Palavra 0, M2-104-UM).
enum class UmpMessageType : std::uint8_t {
    Utility = 0x0,
    System = 0x1,
    Midi1ChannelVoice = 0x2,
    Data64 = 0x3,
    Midi2ChannelVoice = 0x4,
    Data128 = 0x5
};

/// Códigos de status Channel Voice (4 bits, bits 23..20 da Palavra 0).
enum class UmpChannelStatus : std::uint8_t {
    NoteOff = 0x8,
    NoteOn = 0x9,
    ControlChange = 0xB
};

/// Erros de codificação/decodificação/validação UMP.
enum class UmpError : std::uint8_t {
    None = 0,
    InvalidGroup,
    InvalidChannel,
    InvalidNote,
    InvalidController,
    InvalidEventType,
    UnsupportedMessageType,
    UnsupportedStatus,
    TruncatedPacket,
    InvalidWordCount,
    InvalidSevenBitField,
    InvalidReservedField
};

inline constexpr std::uint8_t kMinUmpGroup = 0;
inline constexpr std::uint8_t kMaxUmpGroup = 15;
inline constexpr std::uint8_t kMaxController = 127;

/// Verifica se o grupo UMP está no intervalo válido [0, 15].
[[nodiscard]] constexpr bool is_valid_ump_group(std::uint8_t group) noexcept {
    return group <= kMaxUmpGroup;
}

/// Verifica se o índice de controlador MIDI está no intervalo válido [0, 127].
[[nodiscard]] constexpr bool is_valid_controller(std::uint8_t cc_number) noexcept {
    return cc_number <= kMaxController;
}

/// Retorna a quantidade normativa de palavras de 32 bits para um Message Type (0x0..0xF).
[[nodiscard]] constexpr std::uint8_t ump_expected_word_count(std::uint8_t mt) noexcept {
    switch (mt & 0x0Fu) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x6:
        case 0x7:
            return 1;
        case 0x3:
        case 0x4:
        case 0x8:
        case 0x9:
        case 0xA:
            return 2;
        case 0xB:
        case 0xC:
            return 3;
        case 0x5:
        case 0xD:
        case 0xE:
        case 0xF:
        default:
            return 4;
    }
}

/// Converte velocity MIDI 2.0 (16 bits [0, 65535]) para MIDI 1.0 (7 bits [0, 127])
/// por truncamento dos 9 bits menos significativos (preserva extremos 0->0 e 65535->127).
[[nodiscard]] constexpr std::uint8_t velocity_16_to_7(std::uint16_t vel16) noexcept {
    return static_cast<std::uint8_t>(vel16 >> 9);
}

/// Converte velocity MIDI 1.0 (7 bits [0, 127]) para MIDI 2.0 (16 bits [0, 65535])
/// conforme escalonamento min-center-max (bit-replication) da especificação MIDI 2.0.
[[nodiscard]] constexpr std::uint16_t velocity_7_to_16(std::uint8_t vel7) noexcept {
    const std::uint8_t clamped = (vel7 <= 127u) ? vel7 : 127u;
    if (clamped <= 64u) {
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(clamped) << 9);
    }
    const std::uint16_t base = static_cast<std::uint16_t>(static_cast<std::uint16_t>(clamped) << 9);
    const std::uint16_t repeat_bits = static_cast<std::uint16_t>(clamped & 0x3Fu);
    return static_cast<std::uint16_t>(
        base | (repeat_bits << 3) | (repeat_bits >> 3)
    );
}

/// Converte valor CC MIDI 1.0 (7 bits [0, 127]) para MIDI 2.0 (32 bits [0, UINT32_MAX])
/// conforme escalonamento min-center-max da especificação MIDI 2.0.
[[nodiscard]] constexpr std::uint32_t cc_7_to_32(std::uint8_t val7) noexcept {
    const std::uint32_t v = (val7 <= 127u) ? val7 : 127u;
    if (v <= 64u) {
        return v << 25;
    }
    const std::uint32_t repeat = v & 0x3Fu;
    return (v << 25) | (repeat << 19) | (repeat << 13) | (repeat << 7) | (repeat << 1) | (repeat >> 5);
}

/// Pacote UMP estático com capacidade para até 4 palavras de 32 bits (128 bits).
struct UmpPacket {
    std::array<std::uint32_t, 4> words{};
    std::uint8_t word_count{0};

    [[nodiscard]] constexpr std::uint8_t message_type() const noexcept {
        return static_cast<std::uint8_t>((words[0] >> 28) & 0x0Fu);
    }

    [[nodiscard]] constexpr std::uint8_t group() const noexcept {
        return static_cast<std::uint8_t>((words[0] >> 24) & 0x0Fu);
    }

    [[nodiscard]] constexpr std::uint8_t status_nibble() const noexcept {
        return static_cast<std::uint8_t>((words[0] >> 20) & 0x0Fu);
    }

    [[nodiscard]] constexpr std::uint8_t channel() const noexcept {
        return static_cast<std::uint8_t>((words[0] >> 16) & 0x0Fu);
    }

    constexpr bool operator==(const UmpPacket& other) const noexcept {
        if (word_count != other.word_count) {
            return false;
        }
        for (std::size_t i = 0; i < word_count && i < words.size(); ++i) {
            if (words[i] != other.words[i]) {
                return false;
            }
        }
        return true;
    }

    constexpr bool operator!=(const UmpPacket& other) const noexcept {
        return !(*this == other);
    }
};

/// Serializa uma palavra UMP de 32 bits em 4 bytes Big-Endian (independente da máquina).
constexpr void ump_word_to_be_bytes(std::uint32_t word, std::uint8_t* out4) noexcept {
    if (out4 == nullptr) {
        return;
    }
    out4[0] = static_cast<std::uint8_t>((word >> 24) & 0xFFu);
    out4[1] = static_cast<std::uint8_t>((word >> 16) & 0xFFu);
    out4[2] = static_cast<std::uint8_t>((word >> 8) & 0xFFu);
    out4[3] = static_cast<std::uint8_t>(word & 0xFFu);
}

/// Reconstrói uma palavra UMP de 32 bits a partir de 4 bytes Big-Endian.
[[nodiscard]] constexpr std::uint32_t ump_word_from_be_bytes(const std::uint8_t* in4) noexcept {
    if (in4 == nullptr) {
        return 0;
    }
    return (static_cast<std::uint32_t>(in4[0]) << 24) |
           (static_cast<std::uint32_t>(in4[1]) << 16) |
           (static_cast<std::uint32_t>(in4[2]) << 8) |
           static_cast<std::uint32_t>(in4[3]);
}

/// Evento musical decodificado a partir de um pacote UMP.
struct DecodedUmpMessage {
    std::uint8_t group{0};
    UmpMessageType message_type{UmpMessageType::Midi2ChannelVoice};
    std::variant<NoteEvent, ControlEvent> event{NoteEvent{}};
};

struct UmpEncodeResult {
    bool ok{false};
    UmpError error{UmpError::None};
    UmpPacket packet{};
};

struct UmpDecodeResult {
    bool ok{false};
    UmpError error{UmpError::None};
    DecodedUmpMessage message{};
};

/// Codifica NoteEvent em UMP MIDI 2.0 Channel Voice (MT=0x4, 64 bits).
[[nodiscard]] UmpEncodeResult encode_midi2_note(
    const NoteEvent& event,
    std::uint8_t group = 0
) noexcept;

/// Codifica ControlEvent em UMP MIDI 2.0 Channel Voice (MT=0x4, 64 bits).
[[nodiscard]] UmpEncodeResult encode_midi2_control(
    const ControlEvent& event,
    std::uint8_t group = 0
) noexcept;

/// Codifica NoteEvent em UMP MIDI 1.0 Channel Voice (MT=0x2, 32 bits).
/// Para NoteOn com velocity_16 > 0 que quantize para 0 em 7 bits, satura em 1
/// para evitar interpretação como NoteOff no protocolo MIDI 1.0.
[[nodiscard]] UmpEncodeResult encode_midi1_note(
    const NoteEvent& event,
    std::uint8_t group = 0
) noexcept;

/// Codifica ControlEvent em UMP MIDI 1.0 Channel Voice (MT=0x2, 32 bits).
[[nodiscard]] UmpEncodeResult encode_midi1_control(
    const ControlEvent& event,
    std::uint8_t group = 0
) noexcept;

/// Valida e decodifica um pacote UMP (MT=0x2 ou MT=0x4) para evento interno.
[[nodiscard]] UmpDecodeResult decode_ump_packet(const UmpPacket& packet) noexcept;

/// Modo de protocolo para o roteador UMP.
enum class UmpProtocolMode : std::uint8_t {
    Midi2 = 0,
    Midi1
};

/// Fila circular estática (sem heap) para desacoplar aquisição e transporte/diagnóstico.
template <std::size_t Capacity>
class UmpPacketQueue {
public:
    static_assert(Capacity > 0, "UmpPacketQueue Capacity must be > 0");

    constexpr void clear() noexcept {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        dropped_ = 0;
    }

    bool push(const UmpPacket& pkt) noexcept {
        if (count_ >= Capacity) {
            ++dropped_;
            return false;
        }
        buffer_[tail_] = pkt;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        return true;
    }

    [[nodiscard]] std::optional<UmpPacket> pop() noexcept {
        if (count_ == 0) {
            return std::nullopt;
        }
        const UmpPacket pkt = buffer_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return pkt;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return count_; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return count_ == 0; }
    [[nodiscard]] constexpr std::size_t dropped_count() const noexcept { return dropped_; }

private:
    std::array<UmpPacket, Capacity> buffer_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t count_{0};
    std::size_t dropped_{0};
};

/// Camada de roteamento independente de transporte: converte eventos de KeyScanner
/// e AnalogBank em pacotes UMP e entrega para um sink ou fila estática.
class UmpRouter {
public:
    constexpr explicit UmpRouter(
        std::uint8_t group = 0,
        UmpProtocolMode mode = UmpProtocolMode::Midi2
    ) noexcept
        : group_(group), mode_(mode) {}

    bool set_group(std::uint8_t group) noexcept {
        if (!is_valid_ump_group(group)) {
            return false;
        }
        group_ = group;
        return true;
    }

    constexpr void set_mode(UmpProtocolMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] constexpr std::uint8_t group() const noexcept { return group_; }
    [[nodiscard]] constexpr UmpProtocolMode mode() const noexcept { return mode_; }

    [[nodiscard]] UmpEncodeResult encode_note(const NoteEvent& event) const noexcept {
        return (mode_ == UmpProtocolMode::Midi2)
            ? encode_midi2_note(event, group_)
            : encode_midi1_note(event, group_);
    }

    [[nodiscard]] UmpEncodeResult encode_control(const ControlEvent& event) const noexcept {
        return (mode_ == UmpProtocolMode::Midi2)
            ? encode_midi2_control(event, group_)
            : encode_midi1_control(event, group_);
    }

    template <typename QueueType>
    bool route_note(const NoteEvent& event, QueueType& queue) const noexcept {
        const auto res = encode_note(event);
        if (!res.ok) {
            return false;
        }
        return queue.push(res.packet);
    }

    template <typename QueueType>
    bool route_control(const ControlEvent& event, QueueType& queue) const noexcept {
        const auto res = encode_control(event);
        if (!res.ok) {
            return false;
        }
        return queue.push(res.packet);
    }

private:
    std::uint8_t group_{0};
    UmpProtocolMode mode_{UmpProtocolMode::Midi2};
};

} // namespace harmonia::core