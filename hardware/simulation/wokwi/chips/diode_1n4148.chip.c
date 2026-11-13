// Modelo digital aproximado de diodo unidirecional para varredura matricial
// ativo-baixo (colunas com pull-up, linhas alternando entre LOW e Hi-Z).
// Nao modela Vf, corrente reversa ou comportamento analogico de um 1N4148 real.

#include "wokwi-api.h"
#include <stdlib.h>

typedef struct {
    pin_t anode;
    pin_t cathode;
} diode_state_t;

static void on_cathode_change(void *user_data, pin_t pin, uint32_t value) {
    (void)pin;
    diode_state_t *state = (diode_state_t *)user_data;
    if (value == LOW) {
        pin_mode(state->anode, OUTPUT_LOW);
    } else {
        pin_mode(state->anode, INPUT);
    }
}

void chip_init(void) {
    diode_state_t *state = (diode_state_t *)malloc(sizeof(diode_state_t));
    state->anode = pin_init("ANODE", INPUT);
    state->cathode = pin_init("CATHODE", INPUT_PULLUP);

    const pin_watch_config_t watch_cfg = {
        .edge = BOTH,
        .pin_change = on_cathode_change,
        .user_data = state,
    };
    pin_watch(state->cathode, &watch_cfg);

    // Sincroniza estado inicial
    if (pin_read(state->cathode) == LOW) {
        pin_mode(state->anode, OUTPUT_LOW);
    }
}
