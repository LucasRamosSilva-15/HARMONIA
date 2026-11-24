# Relatório da Etapa 06 — Transporte USB MIDI 2.0 Nativo (TinyUSB) e Fallback MIDI 1.0 no RP2350

## 1. Arquitetura do Transporte USB MIDI e Integração com `UmpRouter`
- **Camada A — Núcleo Portátil (`harmonia_core`, C++17, zero heap):**
  - `software/core/include/harmonia/core/usb_midi_bridge.hpp`
  - `software/core/src/usb_midi_bridge.cpp`
  - O componente `UsbMidiBridge` recebe `UmpPacket` canônicos (`MT = 0x4`) gerados pelo `UmpRouter` e seleciona o formato de saída conforme `UsbAltSetting`:
    - `Midi2Native` (`1`): preserva o pacote `UmpPacket` (`MT = 0x4`, 2 palavras / 64 bits) intacto.
    - `Midi1Fallback` (`0`): converte deterministicamente para `UmpPacket` (`MT = 0x2`, 1 palavra / 32 bits) e gera o quadro USB-MIDI 1.0 de 4 bytes (`[(cable << 4) | CIN, status_ch, byte2, byte3]`) compatível com `tud_midi_packet_write()`.
    - `Disconnected` (`-1`): retorna `std::nullopt` e incrementa `drop_count_` sem bloquear.
  - Suporta também decodificação de retorno do host via `receive_ump()` e `receive_midi1_usb_packet()`.
- **Camada B — Driver USB Device no RP2350 (`firmware/rp2350`):**
  - `firmware/rp2350/include/tusb_config.h`
  - `firmware/rp2350/src/usb_descriptors.cpp`
  - `firmware/rp2350/src/main.cpp`: inicializa `tud_init(BOARD_TUD_RHPORT)` mantendo `pico_stdio_uart` (`UART0` GP0/GP1) ativo e `pico_stdio_usb` desabilitado (`0`). Executa `tud_task()` e leitura/escrita não bloqueantes no loop principal.

## 2. Versão do TinyUSB Inspecionada no Pico SDK e Descritores USB
- **Achado crítico na inspeção de `$PICO_SDK_PATH/lib/tinyusb` (`/home/lucasramos/pico-sdk/lib/tinyusb`):**
  - A versão oficial do TinyUSB empacotada no Pico SDK local é **`0.17.0`** (`TUSB_VERSION_STRING = "0.17.0"`).
  - Nesta versão `0.17.0`, as macros e APIs de MIDI 2.0 nativo (`CFG_TUD_MIDI2`, `TUD_MIDI2_DESC_LEN`, `TUD_MIDI2_DESCRIPTOR`, `tud_midi2_mounted`, `tud_midi2_ump_write`) **ainda não existem** no código-fonte da biblioteca. Apenas a classe USB MIDI 1.0 (`CFG_TUD_MIDI`, `TUD_MIDI_DESCRIPTOR`, `tud_midi_mounted`, `tud_midi_packet_write`, `tud_midi_packet_read`) está disponível.
- **Conformidade com a ADR-002 e restrições:**
  - Mantido exclusivamente o TinyUSB oficial do Pico SDK (sem forks externos como `tusb_ump`).
  - Os descritores em `usb_descriptors.cpp` configuram o dispositivo de protótipo (`VID=0x2E8A`, `PID=0x000A`, fabricante `"HARMONIA"`, produto `"HARMONIA MIDI 2.0 Controller"`, serial via `pico_unique_id`) utilizando `TUD_CONFIG_DESCRIPTOR` e `TUD_MIDI_DESCRIPTOR` (Audio/MIDI Streaming Alt 0, endpoints Bulk `0x01` OUT / `0x81` IN de 64 bytes).
  - Quando uma versão futura do Pico SDK trouxer TinyUSB com `CFG_TUD_MIDI2`, a Camada A (`UsbMidiBridge`) já possui suporte pronto para `UsbAltSetting::Midi2Native` (`MT = 0x4`) sem alterar o núcleo de aquisição nem o `UmpRouter`.

## 3. Mecanismo de Fallback MIDI 1.0 vs. MIDI 2.0 Nativo
- **Alt Setting 1 (`Midi2Native`, implementado na Camada A):** mantém resolução total (`velocity` 16 bits, `CC` 32 bits, `MT = 0x4`).
- **Alt Setting 0 (`Midi1Fallback`, ativo no RP2350 com TinyUSB 0.17.0):**
  - `Note On`: `velocity` 16→7 bits (`vel16 >> 9`), com saturação mínima em `1` sempre que `vel16 > 0` para evitar interpretação como `Note Off` MIDI 1.0 (`vel7 = 0`).
  - `Note Off`: `0x80 | ch`, `note`, `vel7`.
  - `Control Change`: `0xB0 | ch`, `cc_number`, `val32 >> 25`, preservando extremos exatos `0` e `127`.

## 4. Resultados dos Testes Unitários Catch2 e Sanitizers
- **Compilação estrita C++17 (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `usb_midi_bridge.cpp` compilado com **0 warnings / 0 erros**.
- **Host C++23 (`ctest --test-dir build --output-on-failure`):** **42/42 testes aprovados** (38 anteriores + 4 novos casos em `test_usb_midi_bridge.cpp`).
- **Host com Sanitizers (`ASan` + `UBSan`, `ctest --test-dir /tmp/harmonia_san_build --output-on-failure`):** **42/42 testes aprovados**, zero violações de memória ou comportamento indefinido.

## 5. Resultados do Build RP2350
- **Alvo:** `pico2` (`rp2350-arm-s`), Ninja, `PICO_SDK_PATH=/home/lucasramos/pico-sdk`.
- **Bibliotecas linkadas:** `pico_stdlib`, `pico_unique_id`, `hardware_adc`, `tinyusb_device`, `tinyusb_board`, `harmonia::core`.
- **Tamanho do binário (`arm-none-eabi-size firmware/rp2350/build/harmonia_diag.elf`):**
  - `text`: `91224` bytes (~`89.1 KB`)
  - `data`: `0` bytes
  - `bss`: `3472` bytes (~`3.39 KB`)
  - `total (dec)`: `94696` bytes (`0x171e8`)

## 6. Regressão Completa no Wokwi CLI e Métricas Temporais
1. **`wokwi-cli lint hardware/simulation/wokwi`:** `✓ No issues found`.
2. **`scenario_ump.yaml` + `verify_ump_log.py` (`task-491`):** `Scenario completed successfully` (`exit 0`), **`[PASS]` (`UMP Packets=44, NoteEvents=8, CCs=[7, 11]`)**.
3. **`scenario_adc.yaml` (`task-497`):** `Scenario completed successfully` (`exit 0`).
4. **`scenario.yaml` + `verify_wokwi_log.py` (`task-497`):** `Scenario completed successfully` (`exit 0`), **`[PASS]` (`Boot=True, GPIO=True, Scan=True, Heartbeats=1, Fases=20/20`)**.
5. **Métricas observadas em `[METRICS]` (`[2000581 us]`):**
   - `max_scan_dt = 1001 us` (varredura de `1 kHz` preservada com jitter de apenas `1 µs`).
   - `max_scan_late = 2 us`
   - `max_adc_proc = 14 us`
   - `max_adc_late = 1 us`
   - `ump_drops = 0`
   - `usb_mounted = 1`, `usb_alt = 0` (`Midi1Fallback`), `usb_tx_ok = 0`, `usb_drops = 2` (referentes aos 2 eventos iniciais de ADC ocorridos antes da conclusão da montagem USB em `1080 us` / `6081 us`, sem qualquer bloqueio do loop).

## 7. Limitações do Simulador Wokwi e do SDK Atual
- O Wokwi CLI não conecta um host USB MIDI externo (`ALSA`/`snd-usb-midi2`) para consumir pacotes do endpoint IN ou validar enumeração em sistema operacional real.
- O TinyUSB `0.17.0` presente no Pico SDK atual não inclui `CFG_TUD_MIDI2` / Group Terminal Blocks, limitando o transporte ativo no RP2350 ao modo `Alt 0` (USB MIDI 1.0) até atualização do TinyUSB no SDK.

## 8. Checklist de Validação Física em Bancada (Linux)
Quando o Raspberry Pi Pico 2 físico estiver conectado via USB ao host Linux / Raspberry Pi 5:
- [ ] `lsusb -d 2e8a:000a -v`: confirmar strings `"HARMONIA"` / `"HARMONIA MIDI 2.0 Controller"` e descritores Audio/MIDI Streaming.
- [ ] `dmesg | tail -n 30`: confirmar enumeração limpa sem resets de endpoint.
- [ ] `amidi -l` e `aseqdump -l`: confirmar porta MIDI reconhecida pelo ALSA (`snd-usb-audio` / futuro `snd-usb-midi2`).
- [ ] `aseqdump -p <client:port>`: pressionar K1..K4, acordes e girar potenciômetros, validando ausência de notas presas e estabilidade contínua.

## 9. Preparação para a Etapa 07 (Enlace Interno RP2350 ↔ Raspberry Pi 5)
- Com `KeyScanner`, `AnalogBank`, `UmpRouter` e `UsbMidiBridge` desacoplados em `harmonia_core`, a Etapa 07 implementará o protocolo de framing com cabeçalho, tamanho e **CRC** sobre SPI ou UART binária (ADR-003) para enviar os mesmos `UmpPacket` (`MT = 0x4`) ao Raspberry Pi 5 com `0%` de perda silenciosa.
