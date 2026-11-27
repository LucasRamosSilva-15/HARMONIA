# Relatório da Etapa 07 — Protocolo de Enlace Interno (RP2350 ↔ Raspberry Pi 5) com Framing e CRC (ADR-003)

## 1. Decisão Arquitetural da ADR-003 (UART1 921.600 bps vs. SPI)
- **Decisão adotada:** **UART1 (`GP4` TX / `GP5` RX, `921.600` bps 8N1, 3,3 V)** dedicada ao enlace interno RP2350 ↔ Raspberry Pi 5.
- **Comparação técnica:**
  - **Largura de banda e latência:** Um pacote UMP MIDI 2.0 (8 bytes) encapsulado com preâmbulo (`0xAA 0x55`), cabeçalho (4 bytes), CRC-16 (2 bytes), overhead COBS (1 byte) e delimitador `0x00` ocupa **18 bytes** na linha. A `921.600 bps` (`92.160 B/s` úteis em 8N1), o tempo de transmissão é **`~195 µs`** (`0,195 ms`), atendendo à meta de `< 0,2 ms` em `FEASIBILITY.md` e suportando `~5.120` pacotes UMP MIDI 2.0/s (ou `~6.580` pacotes MIDI 1.0/s).
  - **Simplicidade elétrica:** Requer apenas `TX`, `RX` e `GND` em 3,3 V direto, eliminando complexidade de SPI Slave no RP2350, linha extra de `IRQ` e jitter de *polling* no Linux.
  - **Preservação de pinos:** `UART0` (`GP0`/`GP1`) permanece 100% livre para diagnóstico e analisadores Wokwi; matriz (`GP10`, `GP11`, `GP14`, `GP15`) e ADCs (`GP26`, `GP27`) permanecem intactos.

## 2. Especificação do Formato de Frame Binário
- **Enquadramento (Framing):** **COBS** (*Consistent Overhead Byte Stuffing*) terminado pelo delimitador único `0x00`. Garante ausência de `0x00` no corpo do quadro e ressincronização imediata após ruído ou *hot-plug*.
- **Layout interno (pré-COBS, Big-Endian):**
  1. `magic` (2 bytes): `0xAA`, `0x55`.
  2. `version` (1 byte): `0x01`.
  3. `msg_type` (1 byte): `0x01` (`UmpData`), `0x02` (`Heartbeat`), `0x03` (`Ack`).
  4. `seq` (1 byte): contador monotônico `0..255`.
  5. `payload_len` (1 byte): `0..16` bytes (`0` para `Ack`, `4` para `Heartbeat`, `4/8/12/16` para `UmpData`).
  6. `payload` (`0..16` bytes): palavras UMP serializadas via `ump_word_to_be_bytes`.
  7. `crc16` (2 bytes, Big-Endian): **CRC-16/CCITT-FALSE** (polinômio `0x1021`, inicial `0xFFFF`, sem reflexão) sobre cabeçalho + payload.
- **Códigos de diagnóstico do `LinkFrameDecoder`:** `frames_ok`, `crc_errors`, `framing_errors`, `invalid_ump_errors`, `seq_gaps`.

## 3. Implementação do `LinkFrameDecoder` e do `SpscRingBuffer` Lock-Free
- `software/core/include/harmonia/core/link_protocol.hpp` e `src/link_protocol.cpp`: codificação/decodificação incremental byte-a-byte em C++17 sem alocação dinâmica.
- `software/core/include/harmonia/core/spsc_ring_buffer.hpp`: fila circular estática lock-free *Single-Producer, Single-Consumer* (`std::atomic<std::size_t>` com `memory_order_acquire`/`release`, capacidade potência de 2), preparando a ADR-004 para a thread de I/O → thread de áudio no Raspberry Pi 5.
- `software/tools/harmonia_link_receiver.cpp`: receptor/validador POSIX para Linux que abre dispositivo serial/PTY em modo raw (`921600` bps), alimenta o `LinkFrameDecoder`, enfileira no `SpscRingBuffer` e exibe eventos e estatísticas.

## 4. Resultados dos Testes Unitários, PTY Loopback e Sanitizers
1. **Compilação estrita C++17 (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `link_protocol.cpp` compilado com **0 warnings / 0 erros**.
2. **Host C++23 (`ctest --test-dir build --output-on-failure`):** **48/48 testes aprovados** (42 anteriores + 6 novos casos em `test_link_protocol.cpp`, incluindo vetor padrão CRC-16 `0x29B1`, transparência COBS para `0x00/0xAA/0x55/0xFF`, alimentação byte-a-byte e fragmentada, rejeição de CRC/UMP inválido, detecção de `seq_gaps`, transferência concorrente de 20.000 itens no `SpscRingBuffer` e **loopback POSIX real via `openpty()`** com 20/20 frames recuperados e 1 frame corrompido rejeitado).
3. **Host com Sanitizers (`ASan` + `UBSan`):** **48/48 testes aprovados** em `0.81 sec`.
4. **ThreadSanitizer (`TSan`):** Não executado neste host porque `libtsan.so.2.0.0` não está instalada no Fedora 44; concorrência do `SpscRingBuffer` verificada no host padrão e com `ASan + UBSan`.

## 5. Resultados do Build RP2350 e Regressão no Wokwi CLI
- **Build RP2350 (`pico2`, Ninja, `hardware_uart` integrado):** `harmonia_diag.elf` e `harmonia_diag.uf2` compilados sem erros (`text=93416`, `data=0`, `bss=3472`, `dec=96888`).
- **Wokwi Lint:** `✓ No issues found`.
- **Cenário UMP (`scenario_ump.yaml` + `verify_ump_log.py`):** `Scenario completed successfully` (`exit 0`), **`[PASS]`**.
- **Cenário ADC (`scenario_adc.yaml`):** `Scenario completed successfully` (`exit 0`).
- **Cenário Digital (`scenario.yaml` + `verify_wokwi_log.py`):** `Scenario completed successfully` (`exit 0`), **`[PASS]` (`Fases=20/20`, `Heartbeats=1`)**.
- **Métricas observadas (`task-550`):**
  - `[LINK] UART1 GP4(TX)/GP5(RX) @ 921658 bps | COBS + CRC-16`
  - `[METRICS] [2000676 us] max_scan_dt=1002 max_scan_late=2 max_adc_proc=15 max_adc_late=1 adc_events=2 ump_drops=0 usb_mounted=1 usb_alt=0 usb_tx_ok=0 usb_drops=2 link_tx_ok=3 link_drops=0 link_crc_err=0`

## 6. Checklist de Conexão Física e Bancada (RP2350 ↔ Raspberry Pi 5)
- [ ] Conectar `RP2350 GP4 (UART1 TX)` → `RPi 5 GPIO15 (RXD0 / UART RX)` (3,3 V direto).
- [ ] Conectar `RP2350 GP5 (UART1 RX)` ← `RPi 5 GPIO14 (TXD0 / UART TX)` (3,3 V direto).
- [ ] Conectar `GND` comum entre RP2350 e Raspberry Pi 5.
- [ ] Habilitar UART de hardware no RPi 5 (`/dev/ttyAMA0` / `/dev/serial0`) sem console serial do Linux disputando a porta.
- [ ] Executar `./harmonia_link_receiver /dev/serial0` no RPi 5 e validar recebimento contínuo de `HEARTBEAT`, `NOTE_ON`, `NOTE_OFF` e `CC` com `crc_err=0` e `seq_gaps=0`.

## 7. Preparação para a Etapa 08 (Motor de Áudio FluidSynth + Thread-Safety Lock-Free)
- Com o `SpscRingBuffer` e o `LinkFrameDecoder` prontos em `harmonia_core`, a Etapa 08 conectará o fluxo de `DecodedUmpMessage` ao motor **FluidSynth** no Linux (`RPi 5` / host Fedora 44) com thread de áudio isolada (ADR-004 e ADR-005).
