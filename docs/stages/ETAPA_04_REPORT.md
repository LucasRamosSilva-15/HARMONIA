# Relatório da Etapa 04 — Aquisição Analógica e Filtragem no RP2350

## 1. Arquitetura em 3 Camadas
- **Camada A — Núcleo Portátil (`harmonia_core`, C++17):**
  - `software/core/include/harmonia/core/analog_control.hpp`
  - `software/core/src/analog_control.cpp`
  - Componentes: `ControlEvent` (32 bits para MIDI 2.0 + `to_midi1_7bit()` para 7 bits), `AnalogCalibration` (validação `min_raw < max_raw <= adc_max_limit`, clamping e inversão opcional), `ExponentialFilter` (IIR EMA inteiro em ponto fixo Q24.8 com $\alpha = 1/2^k$), `AnalogChannelProcessor` (histerese configurável + disparo garantido nos extremos `0` e `UINT32_MAX`) e `AnalogBank<MaxChannels>`.
  - Zero alocação dinâmica, zero mutexes, zero `sleep()`.
- **Camada B — Driver RP2350 (`Pico SDK` / `hardware_adc`):**
  - `firmware/rp2350/include/adc_driver.hpp`
  - `firmware/rp2350/src/adc_driver.cpp`
  - Canais utilizados no RP2350 QFN-60 (0 a 3,3 V, 12 bits `0..4095`):
    - **ADC0 (`GP26`):** Potenciômetro A -> MIDI CC `7` (Volume).
    - **ADC1 (`GP27`):** Potenciômetro B -> MIDI CC `11` (Expressão).
  - Tempo de estabilização após seleção do canal (`adc_select_input`): `10 µs` (`busy_wait_us_32(10)`).
- **Camada C — Integração ao Firmware (`firmware/rp2350/src/main.cpp`):**
  - Agendamento não bloqueante a cada `10000 µs` (10 ms), lendo **1 canal por ciclo** de forma alternada (`50 Hz` por canal), defasado em `+500 µs` da varredura digital de `1 kHz` para não interferir na matriz.

## 2. Métricas e Complexidade
- **Taxa configurada por canal:** `50 Hz` (`100 Hz` agregada alternando ADC0 e ADC1).
- **Resolução ADC:** `12 bits` (`0..4095`), escalada para `32 bits` (`0..4294967295`) sem perda prematura de precisão.
- **Filtro IIR EMA (`shift = 3`, $\alpha = 1/8 = 0.125$):**
  - Estado por canal: `ExponentialFilter` (~8 bytes) + `AnalogChannelProcessor` (~40 bytes).
  - Custo computacional: $O(1)$ por amostra (subtração, divisão por potência de 2 e soma).
  - Tempo de acomodação observado no Wokwi (degrau `0 -> 4095` @ `50 Hz`): `~1.42 s` (`1420912 us` para atingir `4095` exato com histerese `24`).
- **Interferência na matriz digital:** Nenhuma interferência observada na taxa de `1 kHz` ou no anti-ghosting.

## 3. Resultados Reais Executados
1. **CTest no host (`ctest --test-dir build --output-on-failure`):** **30/30 testes aprovados** em `0.07 sec` (21 anteriores + 9 novos casos em `test_analog_control.cpp` cobrindo os 16 cenários).
2. **Compilação estrita C++17 (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `analog_control.cpp` compilado com **0 warnings** e **0 erros**.
3. **Build RP2350 (`pico2`, Ninja, `PICO_SDK_PATH=/home/lucasramos/pico-sdk`):** Compilado com sucesso (`harmonia_diag.elf` e `harmonia_diag.uf2` atualizados).
4. **Wokwi CLI (`wokwi-cli lint`):** `✓ No issues found` (após remover atributo `label` de `pot1`/`pot2`).
5. **Cenário ADC no Wokwi (`scenario_adc.yaml`):** **`Scenario completed successfully`** (exit `0`). Validou mínimo (`raw=0, midi7=0, val32=0`), intermediário e máximo (`raw=4095, midi7=127, val32=4294967295`) para `pot1` (`cc=7`) e `pot2` (`cc=11`).
6. **Regressão Digital no Wokwi (`scenario.yaml`, `task-319`):** Confirmou `[BOOT]`, `[GPIO]`, `[SCAN]`, `[HEARTBEAT]`, `K1..K4`, acordes `K1+K2`, `K1+K3`, `K2+K4`, anti-ghosting `D_3VERTICES` (sem `note=63`) e `D2_4KEYS` (`60..63` ON e OFF); o processo foi encerrado pelo guarda de 60s (`exit 124`) após `note=62 vel=0` sair 1 ms antes de `note=61 vel=0` no `D2_4KEYS_OFF`.

## 4. Riscos de Integração Elétrica e Pendências Físicas
- **Pedais comerciais:** Não conectar pedais de expressão comerciais diretamente aos GPIOs do RP2350 sem antes verificar pinagem (TRS tip/ring/sleeve), impedância do potenciômetro (tipicamente 10 kΩ–100 kΩ) e garantir divisão de tensão estritamente limitada a `0..3.3 V` (com proteção contra sobretensão/ESD).
- **Hardware físico:** Ruído térmico real de potenciômetros, precisão do `ADC_VREF` do Pico 2 e resposta real de pedais permanecem pendentes de medição em bancada física.

## 5. Recomendações para a Etapa 05 — Eventos Musicais e Codificação UMP
- Unificar `NoteEvent` e `ControlEvent` em um codificador portátil de pacotes **UMP (Universal MIDI Packet)** em `harmonia_core` (C++17), com *golden vectors* no Catch2 para Note On/Off, Control Change (32 bits) e fallback MIDI 1.0 (7 bits).

## 6. Atualização Pós-Etapa 04.1
- A regressão digital (`scenario.yaml` + `verify_wokwi_log.py`) foi corrigida e aprovada (`20/20` fases, `exit 0`), e o filtro EMA foi otimizado para `shift=2` ($\alpha = 1/4$) a `100 Hz/canal` (`310 ms` até `4095` no Wokwi). Consulte [ETAPA_04_1_REPORT.md](ETAPA_04_1_REPORT.md) para as métricas completas.
