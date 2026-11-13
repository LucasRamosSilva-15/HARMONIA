#include <cstdio>
#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

namespace {
constexpr uint kButtonPin = 14;
constexpr uint32_t kDebounceMs = 20;
constexpr uint32_t kHeartbeatMs = 1000;
constexpr uint32_t kPollIntervalMs = 2;
} // namespace

int main() {
    stdio_init_all();

    // Configura LED onboard de diagnóstico
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
#endif

    // Configura botão de teste em GP14 com pull-up interno (ativo-baixo)
    gpio_init(kButtonPin);
    gpio_set_dir(kButtonPin, GPIO_IN);
    gpio_pull_up(kButtonPin);

    printf("\n========================================\n");
    printf("[HARMONIA] RP2350 Diagnostic Firmware\n");
    printf("[HARMONIA] Etapa 01.8 - Wokwi Simulation\n");
    printf("[HARMONIA] Button: GP%u (pull-up, active-low)\n", kButtonPin);
    printf("========================================\n");

    bool last_raw_pressed = false;
    bool stable_pressed = false;
    uint32_t last_change_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_heartbeat_ms = last_change_ms;
    uint32_t heartbeat_count = 0;
    bool led_state = false;

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        const bool raw_pressed = !gpio_get(kButtonPin); // ativo-baixo

        if (raw_pressed != last_raw_pressed) {
            last_raw_pressed = raw_pressed;
            last_change_ms = now_ms;
        }

        if ((now_ms - last_change_ms) >= kDebounceMs && raw_pressed != stable_pressed) {
            stable_pressed = raw_pressed;
            printf("[%lu ms] KEY_TEST %s\n",
                   static_cast<unsigned long>(now_ms),
                   stable_pressed ? "PRESSED" : "RELEASED");
        }

        if ((now_ms - last_heartbeat_ms) >= kHeartbeatMs) {
            last_heartbeat_ms = now_ms;
            ++heartbeat_count;
            led_state = !led_state;
#ifdef PICO_DEFAULT_LED_PIN
            gpio_put(PICO_DEFAULT_LED_PIN, led_state);
#endif
            printf("[%lu ms] HEARTBEAT #%lu\n",
                   static_cast<unsigned long>(now_ms),
                   static_cast<unsigned long>(heartbeat_count));
        }

        sleep_ms(kPollIntervalMs);
    }

    return 0;
}
