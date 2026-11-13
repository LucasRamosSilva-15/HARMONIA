#include "harmonia/core/analog_control.hpp"
#include "harmonia/core/midi_event.hpp"

#include <limits>

namespace harmonia::core {

std::optional<std::uint32_t> normalize_adc_sample(
    std::uint16_t sample,
    const AnalogCalibration& cal
) noexcept {
    if (!cal.is_valid()) {
        return std::nullopt;
    }

    std::uint16_t clamped = sample;
    if (clamped < cal.min_raw) {
        clamped = cal.min_raw;
    } else if (clamped > cal.max_raw) {
        clamped = cal.max_raw;
    }

    const std::uint64_t span = static_cast<std::uint64_t>(cal.max_raw - cal.min_raw);
    const std::uint64_t offset = static_cast<std::uint64_t>(clamped - cal.min_raw);
    constexpr std::uint64_t kMax32 = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t scaled = static_cast<std::uint32_t>((offset * kMax32) / span);
    if (cal.inverted) {
        scaled = static_cast<std::uint32_t>(kMax32) - scaled;
    }
    return scaled;
}

std::uint16_t ExponentialFilter::update(std::uint16_t raw_sample) noexcept {
    const std::uint32_t scaled_in = static_cast<std::uint32_t>(raw_sample) << 8;

    if (!initialized_) {
        accumulator_ = scaled_in;
        initialized_ = true;
        return raw_sample;
    }

    if (shift_ == 0) {
        accumulator_ = scaled_in;
    } else {
        const std::int32_t diff =
            static_cast<std::int32_t>(scaled_in) - static_cast<std::int32_t>(accumulator_);
        const std::int32_t step = diff / static_cast<std::int32_t>(1u << shift_);
        accumulator_ = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(accumulator_) + step
        );
    }

    return static_cast<std::uint16_t>((accumulator_ + 128u) >> 8);
}

bool AnalogChannelProcessor::configure(const AnalogChannelConfig& config) noexcept {
    if (!is_valid_channel(config.midi_channel) ||
        config.cc_number > 127 ||
        !config.calibration.is_valid()) {
        return false;
    }
    config_ = config;
    filter_.set_shift(config.filter_shift);
    reset();
    return true;
}

void AnalogChannelProcessor::reset() noexcept {
    filter_.reset();
    last_emitted_raw_ = 0;
    last_emitted_val32_ = 0;
    has_emitted_ = false;
}

std::optional<ControlEvent> AnalogChannelProcessor::update(std::uint16_t raw_sample) noexcept {
    if (!config_.calibration.is_valid()) {
        return std::nullopt;
    }

    const std::uint16_t filtered = filter_.update(raw_sample);
    const auto normalized = normalize_adc_sample(filtered, config_.calibration);
    if (!normalized.has_value()) {
        return std::nullopt;
    }

    const std::uint32_t val32 = *normalized;

    if (!has_emitted_) {
        has_emitted_ = true;
        last_emitted_raw_ = filtered;
        last_emitted_val32_ = val32;
        return ControlEvent{config_.midi_channel, config_.cc_number, val32, filtered};
    }

    const std::uint16_t delta = (filtered >= last_emitted_raw_)
        ? static_cast<std::uint16_t>(filtered - last_emitted_raw_)
        : static_cast<std::uint16_t>(last_emitted_raw_ - filtered);

    constexpr std::uint32_t kMax32 = std::numeric_limits<std::uint32_t>::max();
    const bool hit_min_extreme = (val32 == 0u && last_emitted_val32_ != 0u);
    const bool hit_max_extreme = (val32 == kMax32 && last_emitted_val32_ != kMax32);

    if (delta >= config_.hysteresis_raw || hit_min_extreme || hit_max_extreme) {
        last_emitted_raw_ = filtered;
        last_emitted_val32_ = val32;
        return ControlEvent{config_.midi_channel, config_.cc_number, val32, filtered};
    }

    return std::nullopt;
}

} // namespace harmonia::core
