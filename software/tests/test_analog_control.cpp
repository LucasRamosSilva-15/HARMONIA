#include <catch2/catch_test_macros.hpp>
#include "harmonia/core/analog_control.hpp"

#include <cstdint>
#include <limits>

using namespace harmonia::core;

namespace {

constexpr std::uint32_t kMax32 = std::numeric_limits<std::uint32_t>::max();

AnalogChannelConfig make_default_cfg(std::uint8_t cc = 7, std::uint16_t hyst = 16) {
    AnalogChannelConfig cfg{};
    cfg.midi_channel = 0;
    cfg.cc_number = cc;
    cfg.calibration = AnalogCalibration{0, 4095, 4095, false};
    cfg.filter_shift = 3; // alpha = 1/8
    cfg.hysteresis_raw = hyst;
    return cfg;
}

} // namespace

TEST_CASE("Minimum, midpoint and maximum ADC samples normalize accurately", "[core][analog]") {
    const AnalogCalibration cal{0, 4095, 4095, false};

    const auto v_min = normalize_adc_sample(0, cal);
    REQUIRE(v_min.has_value());
    REQUIRE(*v_min == 0u);
    REQUIRE(to_midi1_7bit(*v_min) == 0);

    const auto v_max = normalize_adc_sample(4095, cal);
    REQUIRE(v_max.has_value());
    REQUIRE(*v_max == kMax32);
    REQUIRE(to_midi1_7bit(*v_max) == 127);

    const auto v_mid = normalize_adc_sample(2048, cal);
    REQUIRE(v_mid.has_value());
    const std::uint8_t midi_mid = to_midi1_7bit(*v_mid);
    REQUIRE(midi_mid >= 63);
    REQUIRE(midi_mid <= 64);
}

TEST_CASE("Calibration saturation clamps out-of-range samples", "[core][analog]") {
    const AnalogCalibration cal{500, 3500, 4095, false};

    REQUIRE(normalize_adc_sample(0, cal) == 0u);
    REQUIRE(normalize_adc_sample(499, cal) == 0u);
    REQUIRE(normalize_adc_sample(500, cal) == 0u);
    REQUIRE(normalize_adc_sample(3500, cal) == kMax32);
    REQUIRE(normalize_adc_sample(4095, cal) == kMax32);
}

TEST_CASE("Valid inverted calibration and invalid calibrations behave safely", "[core][analog]") {
    const AnalogCalibration inv{100, 4000, 4095, true};
    REQUIRE(inv.is_valid());
    REQUIRE(normalize_adc_sample(100, inv) == kMax32);
    REQUIRE(normalize_adc_sample(4000, inv) == 0u);

    const AnalogCalibration eq{1000, 1000, 4095, false};
    const AnalogCalibration rev{2000, 1000, 4095, false};
    const AnalogCalibration over{0, 5000, 4095, false};
    REQUIRE_FALSE(eq.is_valid());
    REQUIRE_FALSE(rev.is_valid());
    REQUIRE_FALSE(over.is_valid());
    REQUIRE_FALSE(normalize_adc_sample(500, eq).has_value());
    REQUIRE_FALSE(normalize_adc_sample(500, rev).has_value());
    REQUIRE_FALSE(normalize_adc_sample(500, over).has_value());

    AnalogChannelProcessor proc;
    REQUIRE_FALSE(proc.configure(AnalogChannelConfig{0, 7, eq, 3, 16}));
    REQUIRE_FALSE(proc.configure(AnalogChannelConfig{16, 7, inv, 3, 16}));
    REQUIRE_FALSE(proc.configure(AnalogChannelConfig{0, 128, inv, 3, 16}));
}

TEST_CASE("Small noise is suppressed without repetitive events", "[core][analog]") {
    AnalogChannelProcessor proc(make_default_cfg(7, 16));

    // Primeira amostra em repouso emite estado inicial
    const auto first = proc.update(2000);
    REQUIRE(first.has_value());
    REQUIRE(first->raw_filtered == 2000);

    // Pequeno ruido +-8 counts nao deve gerar nenhum evento
    for (int i = 0; i < 100; ++i) {
        const std::uint16_t noisy = static_cast<std::uint16_t>(2000 + ((i % 2 == 0) ? 8 : -8));
        REQUIRE_FALSE(proc.update(noisy).has_value());
    }
}

TEST_CASE("High impulse noise is attenuated by IIR EMA filter", "[core][analog]") {
    ExponentialFilter filter(3); // alpha = 1/8
    REQUIRE(filter.update(2000) == 2000);

    // Um unico pico espurio de 4095 (+2095) deve ser atenuado para ~2262
    const std::uint16_t after_spike = filter.update(4095);
    REQUIRE(after_spike > 2000);
    REQUIRE(after_spike < 2300);

    // Retorno ao valor base 2000 converge de volta
    for (int i = 0; i < 32; ++i) {
        (void)filter.update(2000);
    }
    REQUIRE(filter.current() <= 2005);
}

TEST_CASE("Hysteresis boundary triggers only at or above threshold", "[core][analog]") {
    auto cfg = make_default_cfg(11, 20);
    cfg.filter_shift = 0; // sem suavizacao para testar limiar exato da histerese
    AnalogChannelProcessor proc(cfg);

    REQUIRE(proc.update(1000).has_value());
    REQUIRE_FALSE(proc.update(1019).has_value());
    REQUIRE_FALSE(proc.update(981).has_value());

    const auto up = proc.update(1020);
    REQUIRE(up.has_value());
    REQUIRE(up->raw_filtered == 1020);
    REQUIRE(up->cc_number == 11);
}

TEST_CASE("Slow ramp and fast step changes converge and reach exact extremes", "[core][analog]") {
    AnalogChannelProcessor proc(make_default_cfg(7, 24));
    REQUIRE(proc.update(0).has_value());

    // Rampa lenta de 0 ate 4095
    int slow_events = 0;
    for (std::uint16_t v = 0; v <= 4095; v = static_cast<std::uint16_t>(v + 15)) {
        if (proc.update(v).has_value()) {
            ++slow_events;
        }
    }
    // Estabiliza no maximo absoluto
    bool reached_max = (proc.last_emitted_raw() == 4095);
    for (int i = 0; i < 64 && !reached_max; ++i) {
        const auto ev = proc.update(4095);
        if (ev.has_value() && ev->value_32 == kMax32) {
            reached_max = true;
        }
    }
    REQUIRE(slow_events > 10);
    REQUIRE(reached_max);

    // Degrau rapido para 0
    bool reached_min = false;
    for (int i = 0; i < 128; ++i) {
        const auto ev = proc.update(0);
        if (ev.has_value() && ev->value_32 == 0u) {
            reached_min = true;
            break;
        }
    }
    REQUIRE(reached_min);
}

TEST_CASE("Prolonged stability and reset behavior", "[core][analog]") {
    AnalogChannelProcessor proc(make_default_cfg(7, 16));
    REQUIRE(proc.update(1500).has_value());

    for (int i = 0; i < 2000; ++i) {
        REQUIRE_FALSE(proc.update(1500).has_value());
    }

    proc.reset();
    REQUIRE_FALSE(proc.has_emitted());
    const auto after_reset = proc.update(1500);
    REQUIRE(after_reset.has_value());
    REQUIRE(after_reset->raw_filtered == 1500);
}

TEST_CASE("AnalogBank keeps channels completely independent", "[core][analog]") {
    AnalogBank<2> bank;
    REQUIRE(bank.configure_channel(0, make_default_cfg(7, 16)));
    REQUIRE(bank.configure_channel(1, make_default_cfg(11, 16)));
    REQUIRE_FALSE(bank.configure_channel(2, make_default_cfg(1, 16)));

    const auto ev0 = bank.update_channel(0, 100);
    const auto ev1 = bank.update_channel(1, 3000);
    REQUIRE(ev0.has_value());
    REQUIRE(ev1.has_value());
    REQUIRE(ev0->cc_number == 7);
    REQUIRE(ev1->cc_number == 11);
    REQUIRE(to_midi1_7bit(ev0->value_32) < 10);
    REQUIRE(to_midi1_7bit(ev1->value_32) > 90);

    // Alterar canal 0 nao afeta canal 1
    for (int i = 0; i < 32; ++i) {
        (void)bank.update_channel(0, 4095);
    }
    REQUIRE_FALSE(bank.update_channel(1, 3005).has_value());
}

#include <cstdio>

namespace {

struct FilterBenchmarkResult {
    int first_event_up_ms{-1};
    int t50_up_ms{-1};
    int t90_up_ms{-1};
    int t99_up_ms{-1};
    int t100_up_ms{-1};
    int events_up{0};
    int first_event_down_ms{-1};
    int t50_down_ms{-1};
    int t90_down_ms{-1};
    int t99_down_ms{-1};
    int t100_down_ms{-1};
    int events_down{0};
    int ramp_events{0};
    int noise_events{0};
    int hyst_near_events{0};
};

FilterBenchmarkResult run_filter_benchmark(std::uint8_t shift, int rate_hz, std::uint16_t hyst) {
    FilterBenchmarkResult res{};
    const int dt_ms = 1000 / rate_hz;

    AnalogChannelConfig cfg{};
    cfg.midi_channel = 0;
    cfg.cc_number = 7;
    cfg.calibration = AnalogCalibration{0, 4095, 4095, false};
    cfg.filter_shift = shift;
    cfg.hysteresis_raw = hyst;

    // 1. Degrau 0% -> 100%
    AnalogChannelProcessor proc(cfg);
    (void)proc.update(0); // estado inicial em repouso
    for (int step = 1; step <= 200; ++step) {
        const int t_ms = step * dt_ms;
        const auto ev = proc.update(4095);
        const std::uint16_t f = proc.current_filtered_raw();
        if (ev.has_value()) {
            ++res.events_up;
            if (res.first_event_up_ms < 0) res.first_event_up_ms = t_ms;
        }
        if (res.t50_up_ms < 0 && f >= 2048) res.t50_up_ms = t_ms;
        if (res.t90_up_ms < 0 && f >= 3686) res.t90_up_ms = t_ms;
        if (res.t99_up_ms < 0 && f >= 4054) res.t99_up_ms = t_ms;
        if (res.t100_up_ms < 0 && f == 4095) res.t100_up_ms = t_ms;
    }

    // 2. Degrau 100% -> 0%
    proc.reset();
    (void)proc.update(4095);
    for (int step = 1; step <= 200; ++step) {
        const int t_ms = step * dt_ms;
        const auto ev = proc.update(0);
        const std::uint16_t f = proc.current_filtered_raw();
        if (ev.has_value()) {
            ++res.events_down;
            if (res.first_event_down_ms < 0) res.first_event_down_ms = t_ms;
        }
        if (res.t50_down_ms < 0 && f <= 2047) res.t50_down_ms = t_ms;
        if (res.t90_down_ms < 0 && f <= 409) res.t90_down_ms = t_ms;
        if (res.t99_down_ms < 0 && f <= 41) res.t99_down_ms = t_ms;
        if (res.t100_down_ms < 0 && f == 0) res.t100_down_ms = t_ms;
    }

    // 3. Movimentacao gradual (rampa 0 -> 4095 em 2.0 s)
    proc.reset();
    (void)proc.update(0);
    const int ramp_steps = 2 * rate_hz;
    for (int i = 1; i <= ramp_steps; ++i) {
        const std::uint16_t sample = static_cast<std::uint16_t>((4095 * i) / ramp_steps);
        if (proc.update(sample).has_value()) ++res.ramp_events;
    }

    // 4. Ruido de pequena amplitude (+-8 counts ao redor de 2000 por 2.0 s)
    proc.reset();
    (void)proc.update(2000);
    for (int i = 0; i < 2 * rate_hz; ++i) {
        const std::uint16_t s = static_cast<std::uint16_t>(2000 + ((i % 2 == 0) ? 8 : -8));
        if (proc.update(s).has_value()) ++res.noise_events;
    }

    // 5. Oscilacao proxima ao limiar de histerese (+-(hyst - 1) ao redor de 2000)
    proc.reset();
    (void)proc.update(2000);
    for (int i = 0; i < 2 * rate_hz; ++i) {
        const int delta = (i % 2 == 0) ? static_cast<int>(hyst - 1) : -static_cast<int>(hyst - 1);
        const std::uint16_t s = static_cast<std::uint16_t>(2000 + delta);
        if (proc.update(s).has_value()) ++res.hyst_near_events;
    }

    return res;
}

} // namespace

TEST_CASE("Filter configurations comparison for step, ramp, noise and hysteresis", "[core][analog][benchmark]") {
    const auto r_s3_50  = run_filter_benchmark(3, 50, 24);
    const auto r_s2_50  = run_filter_benchmark(2, 50, 24);
    const auto r_s2_100 = run_filter_benchmark(2, 100, 24);
    const auto r_s1_50  = run_filter_benchmark(1, 50, 24);

    std::printf("\n[FILTER_BENCH] shift=3@50Hz : up(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | down(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | ramp_ev=%d noise_ev=%d hyst_near_ev=%d\n",
                r_s3_50.first_event_up_ms, r_s3_50.t50_up_ms, r_s3_50.t90_up_ms, r_s3_50.t99_up_ms, r_s3_50.t100_up_ms, r_s3_50.events_up,
                r_s3_50.first_event_down_ms, r_s3_50.t50_down_ms, r_s3_50.t90_down_ms, r_s3_50.t99_down_ms, r_s3_50.t100_down_ms, r_s3_50.events_down,
                r_s3_50.ramp_events, r_s3_50.noise_events, r_s3_50.hyst_near_events);
    std::printf("[FILTER_BENCH] shift=2@50Hz : up(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | down(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | ramp_ev=%d noise_ev=%d hyst_near_ev=%d\n",
                r_s2_50.first_event_up_ms, r_s2_50.t50_up_ms, r_s2_50.t90_up_ms, r_s2_50.t99_up_ms, r_s2_50.t100_up_ms, r_s2_50.events_up,
                r_s2_50.first_event_down_ms, r_s2_50.t50_down_ms, r_s2_50.t90_down_ms, r_s2_50.t99_down_ms, r_s2_50.t100_down_ms, r_s2_50.events_down,
                r_s2_50.ramp_events, r_s2_50.noise_events, r_s2_50.hyst_near_events);
    std::printf("[FILTER_BENCH] shift=2@100Hz: up(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | down(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | ramp_ev=%d noise_ev=%d hyst_near_ev=%d\n",
                r_s2_100.first_event_up_ms, r_s2_100.t50_up_ms, r_s2_100.t90_up_ms, r_s2_100.t99_up_ms, r_s2_100.t100_up_ms, r_s2_100.events_up,
                r_s2_100.first_event_down_ms, r_s2_100.t50_down_ms, r_s2_100.t90_down_ms, r_s2_100.t99_down_ms, r_s2_100.t100_down_ms, r_s2_100.events_down,
                r_s2_100.ramp_events, r_s2_100.noise_events, r_s2_100.hyst_near_events);
    std::printf("[FILTER_BENCH] shift=1@50Hz : up(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | down(1st=%dms, 50%%=%dms, 90%%=%dms, 99%%=%dms, 100%%=%dms, ev=%d) | ramp_ev=%d noise_ev=%d hyst_near_ev=%d\n",
                r_s1_50.first_event_up_ms, r_s1_50.t50_up_ms, r_s1_50.t90_up_ms, r_s1_50.t99_up_ms, r_s1_50.t100_up_ms, r_s1_50.events_up,
                r_s1_50.first_event_down_ms, r_s1_50.t50_down_ms, r_s1_50.t90_down_ms, r_s1_50.t99_down_ms, r_s1_50.t100_down_ms, r_s1_50.events_down,
                r_s1_50.ramp_events, r_s1_50.noise_events, r_s1_50.hyst_near_events);

    // Todas as configuracoes devem suprimir ruido pequeno e oscilacao abaixo da histerese
    REQUIRE(r_s3_50.noise_events == 0);
    REQUIRE(r_s2_50.noise_events == 0);
    REQUIRE(r_s2_100.noise_events == 0);
    REQUIRE(r_s1_50.noise_events == 0);
    REQUIRE(r_s2_100.hyst_near_events == 0);

    // shift=2 @ 100Hz deve atingir 99% em < 200ms e 100% muito mais rapido que shift=3 @ 50Hz
    REQUIRE(r_s2_100.t99_up_ms > 0);
    REQUIRE(r_s2_100.t99_up_ms < 200);
    REQUIRE(r_s2_100.t100_up_ms > 0);
    REQUIRE(r_s2_100.t100_up_ms < r_s3_50.t100_up_ms);
    REQUIRE(r_s2_100.t100_down_ms > 0);
    REQUIRE(r_s2_100.t100_down_ms < r_s3_50.t100_down_ms);
}
