# Relatório da Etapa 01.8 — Integração da Simulação Wokwi com Pico SDK

## 1. Resumo das Alterações
Foi preparada a fundação para simulação eletrônica gratuita no VS Code utilizando **Wokwi** com **Raspberry Pi Pico 2 (`board-pi-pico-2`)** e **Pico SDK**:
- Diagrama eletrônico minimalista com UART0 para monitor serial, botão digital com pull-up interno e LED onboard de diagnóstico.
- Firmware C++ mínimo (`harmonia_diag`) com debounce básico (20 ms), log de pressionamento/soltura e heartbeat periódico (1000 ms).
- Configuração CMake (`PICO_BOARD=pico2`, `PICO_PLATFORM=rp2350`) e `wokwi.toml` com caminhos relativos validados.

## 2. Arquivos Criados e Modificados
- `hardware/simulation/wokwi/diagram.json` (criado)
- `hardware/simulation/wokwi/wokwi.toml` (criado)
- `firmware/rp2350/CMakeLists.txt` (criado)
- `firmware/rp2350/pico_sdk_import.cmake` (criado)
- `firmware/rp2350/src/main.cpp` (criado)
- `docs/stages/ETAPA_01_8_WOKWI_REPORT.md` (criado)
- `README.md` (atualizado)
- `docs/stages/ETAPA_01_REPORT.md` (atualizado)

## 3. Conexões do Diagrama (`diagram.json`)
- `pico:GP0` -> `$serialMonitor:RX` (UART0 TX)
- `pico:GP1` -> `$serialMonitor:TX` (UART0 RX)
- `btn1:1.r` -> `pico:GP14` (entrada digital com `gpio_pull_up(14)`, ativo-baixo)
- `btn1:2.r` -> `pico:GND.4`
- LED de diagnóstico: `PICO_DEFAULT_LED_PIN` (onboard do Pico 2).

## 4. Verificação do Ambiente e Limitações Encontradas
- **OS Host:** Fedora Linux 44.
- **Ferramentas ausentes no ambiente atual:** `arm-none-eabi-gcc`, `picotool`, `PICO_SDK_PATH` e `wokwi-cli` não estão instalados no `PATH`.
- **Impacto:** Os critérios de compilação (`cmake` + `make` para `pico2`) e execução automática no Wokwi **não puderam ser executados** neste ambiente e, portanto, **não são declarados como aprovados**.

## 5. Procedimento de Validação Manual (Quando Toolchain e Wokwi Estiverem Disponíveis)

1. Instalar o toolchain ARM e clonar o Pico SDK (>= 2.0.0) no Fedora:
   ```bash
   sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib
   git clone -b 2.1.0 --depth 1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
   git -C ~/pico-sdk submodule update --init
   export PICO_SDK_PATH=~/pico-sdk
   ```
2. Compilar o firmware de diagnóstico:
   ```bash
   cd firmware/rp2350
   cmake -B build -DPICO_BOARD=pico2
   cmake --build build -j
   ```
3. Abrir o diretório `hardware/simulation/wokwi/` no VS Code com a extensão **Wokwi Simulator** instalada e iniciar a simulação (`F1 -> Wokwi: Start Simulator`).
4. Verificar no Monitor Serial:
   - Mensagem de inicialização `[HARMONIA] RP2350 Diagnostic Firmware`.
   - Eventos `KEY_TEST PRESSED` e `KEY_TEST RELEASED` ao clicar no botão `btn1`.
   - `HEARTBEAT #N` a cada 1000 ms e piscar do LED onboard.
   - Reinicialização da simulação sem falhas.

## 6. Limitações Obrigatórias do Wokwi
O Wokwi **não** deve ser tratado como validação definitiva de:
- Execução dual-core do RP2350.
- USB MIDI 2.0 em hardware físico.
- Latência elétrica real.
- Propriedades analógicas não modeladas pelo simulador.
- Comunicação física com a Raspberry Pi 5.
