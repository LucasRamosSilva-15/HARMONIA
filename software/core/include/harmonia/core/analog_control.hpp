#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace harmonia::core {

/// Evento interno de controle contínuo, preservando resolução de 32 bits (MIDI 2.0)
/// e mantendo compatibilidade futura para quantização MIDI 1.0 (7 bits).
struct ControlEvent {
    std::uint8_t channel{0};
    std::uint8_t cc_number{0};
    std::uint32_t value_32{0};
    std::uint16_t raw_filtered{0};

    constexpr bool operator==(const ControlEvent& other) const noexcept {
        return channel == other.channel &&
               cc_number == other.cc_number &&
               value_32 == other.value_32 &&
               raw_filtered == other.raw_filtered;
    }

    constexpr bool operator!=(const ControlEvent& other) const noexcept {
        return !(*this == other);
    }
};

/// Converte valor contínuo de 32 bits [0, UINT32_MAX] para MIDI 1.0 de 7 bits [0, 127].
[[nodiscard]] constexpr std::uint8_t to_midi1_7bit(std::uint32_t value_32) noexcept {
    return static_cast<std::uint8_t>(value_32 >> 25);
}

/// Parâmetros de calibração de um canal ADC (12 bits padrão: 0..4095).
struct AnalogCalibration {
    std::uint16_t min_raw{0};
    std::uint16_t max_raw{4095};
    std::uint16_t adc_max_limit{4095};
    bool inverted{false};

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return min_raw < max_raw && max_raw <= adc_max_limit;
    }
};

/// Normaliza uma amostra bruta para [0, UINT32_MAX] aplicando saturação e calibração.
/// Retorna std::nullopt se a calibração for inválida.
[[nodiscard]] std::optional<std::uint32_t> normalize_adc_sample(
    std::uint16_t sample,
    const AnalogCalibration& cal
) noexcept;

/// Filtro IIR exponencial (EMA) em ponto fixo (sub-resolução de 8 bits)
/// com fator alpha = 1 / 2^shift.
/// Complexidade: O(1) tempo, O(1) memória (~8 bytes por instância).
class ExponentialFilter {
public:
    static constexpr std::uint8_t kDefaultShift = 3; // alpha = 1/8 = 0.125
    static constexpr std::uint8_t kMaxShift = 6;     // alpha = 1/64

    constexpr explicit ExponentialFilter(std::uint8_t shift = kDefaultShift) noexcept
        : shift_(shift <= kMaxShift ? shift : kMaxShift) {}

    constexpr void set_shift(std::uint8_t shift) noexcept {
        shift_ = (shift <= kMaxShift ? shift : kMaxShift);
    }

    [[nodiscard]] constexpr std::uint8_t shift() const noexcept { return shift_; }

    constexpr void reset() noexcept {
        accumulator_ = 0;
        initialized_ = false;
    }

    [[nodiscard]] std::uint16_t update(std::uint16_t raw_sample) noexcept;

    [[nodiscard]] constexpr bool is_initialized() const noexcept { return initialized_; }

    [[nodiscard]] constexpr std::uint16_t current() const noexcept {
        return initialized_ ? static_cast<std::uint16_t>((accumulator_ + 128u) >> 8) : 0;
    }

private:
    std::uint32_t accumulator_{0}; // Q24.8 (raw << 8)
    std::uint8_t shift_{kDefaultShift};
    bool initialized_{false};
};

/// Configuração completa de um canal de controle analógico.
struct AnalogChannelConfig {
    std::uint8_t midi_channel{0};
    std::uint8_t cc_number{7};
    AnalogCalibration calibration{};
    std::uint8_t filter_shift{ExponentialFilter::kDefaultShift};
    std::uint16_t hysteresis_raw{16};
};

/// Processador determinístico de um canal analógico (filtragem + histerese + normalização).
/// Sem alocação dinâmica, sem mutexes, sem sleep().
class AnalogChannelProcessor {
public:
    constexpr AnalogChannelProcessor() noexcept = default;

    explicit AnalogChannelProcessor(const AnalogChannelConfig& config) noexcept {
        configure(config);
    }

    bool configure(const AnalogChannelConfig& config) noexcept;

    [[nodiscard]] const AnalogChannelConfig& config() const noexcept { return config_; }

    void reset() noexcept;

    /// Processa uma nova amostra ADC e retorna ControlEvent apenas quando houver
    /// mudança relevante acima da histerese (ou ao atingir extremos calibrados).
    [[nodiscard]] std::optional<ControlEvent> update(std::uint16_t raw_sample) noexcept;

    [[nodiscard]] bool has_emitted() const noexcept { return has_emitted_; }
    [[nodiscard]] std::uint16_t last_emitted_raw() const noexcept { return last_emitted_raw_; }
    [[nodiscard]] std::uint16_t current_filtered_raw() const noexcept { return filter_.current(); }

private:
    AnalogChannelConfig config_{};
    ExponentialFilter filter_{};
    std::uint16_t last_emitted_raw_{0};
    std::uint32_t last_emitted_val32_{0};
    bool has_emitted_{false};
};

/// Banco estático de múltiplos canais analógicos independentes.
template <std::size_t MaxChannels>
class AnalogBank {
public:
    constexpr AnalogBank() noexcept = default;

    bool configure_channel(std::size_t index, const AnalogChannelConfig& config) noexcept {
        if (index >= MaxChannels) {
            return false;
        }
        return channels_[index].configure(config);
    }

    void reset_all() noexcept {
        for (auto& ch : channels_) {
            ch.reset();
        }
    }

    [[nodiscard]] std::optional<ControlEvent> update_channel(
        std::size_t index,
        std::uint16_t raw_sample
    ) noexcept {
        if (index >= MaxChannels) {
            return std::nullopt;
        }
        return channels_[index].update(raw_sample);
    }

    [[nodiscard]] const AnalogChannelProcessor* channel(std::size_t index) const noexcept {
        if (index >= MaxChannels) {
            return nullptr;
        }
        return &channels_[index];
    }

private:
    std::array<AnalogChannelProcessor, MaxChannels> channels_{};
};

} // namespace harmonia::core
