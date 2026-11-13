# Relatório da Etapa 04.1 — Correção da Regressão Digital e Otimização da Resposta Analógica

## 1. Causa do Timeout Digital
- Na execução anterior (`task-319`), todos os quatro eventos `NOTE_OFF` (`note=60..63 vel=0`) de `D2_4KEYS_OFF` **foram efetivamente emitidos** pelo firmware sem perda de UART e sem notas presas.
- O timeout (`exit code 124`) ocorreu porque o `scenario.yaml` utilizava quatro diretivas `wait-serial` sequenciais em ordem fixa (`60 -> 61 -> 62 -> 63`), e a liberação escalonada de 2 ms aliada à cadência do scanner (`1 kHz`) e ao jitter da simulação fez `note=62 vel=0` ser impresso 1 ms antes de `note=61 vel=0`.
- Como o `wait-serial` do Wokwi CLI consome o fluxo serial estritamente em ordem, a espera por `note=61 vel=0` descartou a linha `note=62 vel=0`, bloqueando o passo seguinte (`wait-serial: 'ch=0 note=62 vel=0'`) até o timeout.
- **Conclusão:** não houve interferência do ADC nem bug no `KeyScanner`; a causa foi expectativa sequencial rígida no YAML para eventos de acorde cuja ordem relativa não é requisito funcional.

## 2. Alterações Realizadas
- **`hardware/simulation/wokwi/scenario.yaml`:** Substituídas as sequências rígidas de `wait-serial` nas fases de acordes (Testes C, D e D2) por janelas `delay: 45ms` após o último `set-control` de cada fase (margem suficiente para `20 ms` de debounce + escalonamento + varredura), mantendo `wait-serial` determinístico nos testes de inicialização (Teste A), teclas individuais (Teste B) e estabilidade final (Teste E).
- **`scripts/test_verify_wokwi_log.py`:** Adicionado o teste `test_reordered_chord_note_off_passes`, comprovando que `scripts/verify_wokwi_log.py` valida conjuntos completos de eventos por fase independentemente da ordem interna do acorde, mantendo rejeição estrita de ghosting, duplicatas, órfãos, canais incorretos e notas presas.
- **`firmware/rp2350/src/main.cpp`:**
  - Ajustado o subsistema analógico para `kAdcPollIntervalUs = 5000` (**`100 Hz/canal`** alternado) e `kAdcFilterShift = 2` (**$\alpha = 1/4 = 0.25$**), mantendo `kAdcHysteresisRaw = 24` e defasagem de `+500 µs` em relação ao scan digital de `1 kHz`.
  - Adicionada instrumentação não intrusiva de temporização (`max_scan_dt`, `max_scan_late`, `max_adc_proc`, `max_adc_late`, `adc_events`), impressa apenas uma vez a cada `2 s` junto ao `[HEARTBEAT]`, sem logs no caminho crítico.
- **`software/tests/test_analog_control.cpp`:** Adicionado o benchmark reproduzível `[benchmark]` comparando quatro configurações de filtro em degrau ascendente/descendente, rampa gradual, ruído pequeno e oscilação próxima à histerese.

## 3. Resultados dos Testes
1. **Compilação estrita C++17 (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `analog_control.cpp` compilado com **0 warnings** e **0 erros**.
2. **Host C++23 + CTest (`ctest --test-dir build --output-on-failure`):** **31/31 testes aprovados** em `0.07 sec`.
3. **Testes Python (`python3 -m unittest scripts/test_verify_wokwi_log.py -v`):** **9/9 testes aprovados** em `0.001 s`.
4. **Build RP2350 (`pico2`, Ninja, `PICO_SDK_PATH=/home/lucasramos/pico-sdk`):** `harmonia_diag.elf` e `harmonia_diag.uf2` compilados com sucesso.
5. **Wokwi Lint (`wokwi-cli lint hardware/simulation/wokwi`):** `✓ No issues found`.
6. **Cenário ADC no Wokwi (`scenario_adc.yaml`, `task-381`):** **`Scenario completed successfully`** (`exit 0`). `pot1` (`cc=7`) e `pot2` (`cc=11`) atingiram `raw=4095 midi7=127 val32=4294967295` em `310 ms` após o degrau.
7. **Cenário Digital + Anti-Ghosting no Wokwi (`scenario.yaml`, `task-385`):** **`Scenario completed successfully`** (`exit 0`) e `verify_wokwi_log.py` retornou **`[PASS]` (`Boot=True, GPIO=True, Scan=True, Heartbeats=1, Fases=20/20`)** sem ghosting, duplicatas, órfãos ou notas presas.

## 4. Comparação dos Filtros (Benchmark Reproduzível em Catch2)
Medições com `hysteresis_raw = 24` e escala 12 bits (`0..4095`):

| Configuração | 1º Evento (0→100%) | 50% (≥2048) | 90% (≥3686) | 99% (≥4054) | 100% (4095) | 100% Descida (4095→0) | Rampa 2s (eventos) | Ruído ±8 | Histerese ±23 |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| `shift=3` ($\alpha=1/8$) @ `50 Hz` | 20 ms | 120 ms | 360 ms | 700 ms | 1360 ms | 1360 ms | 96 | 0 | 0 |
| `shift=2` ($\alpha=1/4$) @ `50 Hz` | 20 ms | 60 ms | 180 ms | 320 ms | 640 ms | 640 ms | 98 | 0 | 0 |
| **`shift=2` ($\alpha=1/4$) @ `100 Hz` (escolhida)** | **10 ms** | **30 ms** | **90 ms** | **160 ms** | **320 ms** | **320 ms** | **99** | **0** | **0** |
| `shift=1` ($\alpha=1/2$) @ `50 Hz` | 20 ms | 20 ms | 80 ms | 140 ms | 260 ms | 280 ms | 99 | 0 | 0 |

**Justificativa da escolha (`shift=2 @ 100 Hz`):**
- Reduz o tempo de acomodação total de `1360 ms` para **`320 ms`** (e `310 ms` observados no Wokwi), atingindo `90%` em **`90 ms`** e o primeiro evento musical em **`10 ms`**, oferecendo resposta expressiva imediata para volume/expressão.
- Mantém atenuação de **75%** (fator 4) sobre picos espúrios de amostra única (superior aos 50% de `shift=1`), com **zero** eventos espúrios para ruído de `±8` counts e oscilação próxima à histerese (`±23` counts).
- O custo adicional no loop (`100 Hz` por canal = 1 leitura a cada `5000 µs`) consome ~`14 µs` por leitura (`< 0.3%` do orçamento temporal), sem impactar a varredura digital de `1 kHz`.

## 5. Métricas Temporais Observadas no Wokwi
Registro capturado em `task-385` (`[METRICS] [2000424 us]`):
- **Maior intervalo entre varreduras digitais (`max_scan_dt`):** `1000 µs` (exato para `1 kHz`).
- **Atraso máximo de agendamento digital (`max_scan_late`):** `1 µs`.
- **Tempo máximo de processamento ADC (`max_adc_proc`):** `14 µs` (inclui `10 µs` de estabilização do mux + `adc_read()` + `AnalogBank::update_channel()`).
- **Atraso máximo de agendamento ADC (`max_adc_late`):** `0 µs`.
- **Eventos analógicos emitidos:** `2` no cenário digital em repouso (apenas inicialização de `cc=7` e `cc=11`; zero oscilações posteriores); `17` eventos por canal durante o degrau `0 -> 4095` no cenário ADC (`task-381`).

## 6. Limitações do Wokwi
- A simulação em nuvem executa mais lenta que o tempo real em *wall-clock* (~10–15×), embora os contadores internos (`time_us_32()`) reflitam o tempo simulado do RP2350.
- `wokwi-potentiometer` gera valores ideais sem ruído térmico, folga mecânica ou impedância de saída variável.
- `diode_1n4148` é um modelo lógico digital para varredura ativo-baixo (não modela queda $V_f$ real).
- Portanto, as métricas temporais e de ruído no Wokwi validam o determinismo lógico do firmware, **não** substituindo medições em hardware físico.

## 7. Pendências de Hardware Físico
- Medir ruído real de potenciômetros e pedais de expressão (10 kΩ–100 kΩ) alimentados em `ADC_VREF` (3,3 V) no Raspberry Pi Pico 2 físico.
- Validar proteção contra sobretensão/ESD e pinagem TRS antes de conectar pedais comerciais.
- Validar matriz real com diodos 1N4148 físicos e osciloscópio/analisador lógico para confirmar margens reais de `ADC` e `GPIO`.

## 8. Condições Necessárias para Iniciar a Etapa 05
- Regressão digital (`scenario.yaml` + `verify_wokwi_log.py`) e analógica (`scenario_adc.yaml`) aprovadas com `exit 0` (cumprido).
- Núcleo `harmonia_core` (`KeyScanner` + `AnalogBank`) estável, sem alocação dinâmica e validado por CTest em C++17/C++23 (cumprido).
- Autorização explícita do usuário para iniciar a **Etapa 05 (Eventos Musicais e Codificação UMP)**.
