# Relatório da Etapa 05 — Eventos Musicais e Codificação UMP (MIDI 1.0 / MIDI 2.0)

## 1. Componentes Implementados
- **Núcleo portátil (`harmonia_core`, C++17, zero heap):**
  - `software/core/include/harmonia/core/ump.hpp`
  - `software/core/src/ump.cpp`
  - Estruturas e APIs:
    - `UmpPacket` (`std::array<std::uint32_t, 4>`, `word_count`, trivialmente copiável).
    - `ump_expected_word_count(mt)` conforme tabela normativa M2-104-UM (32, 64, 96 ou 128 bits).
    - `encode_midi2_note`, `encode_midi2_control` (`MT = 0x4`, 64 bits).
    - `encode_midi1_note`, `encode_midi1_control` (`MT = 0x2`, 32 bits).
    - `decode_ump_packet` com validação estrita de truncamento, `MT`, `status`, campos 7-bit e bytes reservados.
    - `ump_word_to_be_bytes` / `ump_word_from_be_bytes` (serialização independente da *endianness* da máquina).
    - `UmpPacketQueue<Capacity>` (fila circular estática) e `UmpRouter` (roteamento independente de transporte).
- **Verificador e cenário Wokwi:**
  - `hardware/simulation/wokwi/scenario_ump.yaml`
  - `scripts/verify_ump_log.py` e `scripts/test_verify_ump_log.py`
  - `software/tests/test_ump.cpp`

## 2. Campos dos Pacotes UMP (M2-104-UM)
- **MIDI 1.0 Channel Voice (`MT = 0x2`, 1 palavra / 32 bits):**
  - `word[0]`: `[31:28] MT=0x2 | [27:24] Group (0..15) | [23:20] Status (0x8/0x9/0xB) | [19:16] Channel (0..15) | [15:8] Index 7-bit (Note/CC, bit15=0) | [7:0] Data 7-bit (Vel/Value, bit7=0)`.
- **MIDI 2.0 Channel Voice (`MT = 0x4`, 2 palavras / 64 bits):**
  - **Note On (`0x9`) / Note Off (`0x8`):**
    - `word[0]`: `[31:28] MT=0x4 | [27:24] Group | [23:20] Status | [19:16] Channel | [15:8] Note (0..127) | [7:0] Attribute Type (0x00 = None)`.
    - `word[1]`: `[31:16] Velocity (16 bits) | [15:0] Attribute Data (0x0000)`.
  - **Control Change (`0xB`):**
    - `word[0]`: `[31:28] MT=0x4 | [27:24] Group | [23:20] Status=0xB | [19:16] Channel | [15:8] Controller (0..127) | [7:0] Reserved (0x00)`.
    - `word[1]`: `[31:0] Controller Value (32 bits)`.

## 3. Vetores de Teste (Golden Vectors Independentes)
Validados em `software/tests/test_ump.cpp`:
- Note On MIDI 2.0 (`G=0, Ch=0, Note=60, Vel=40000`): `40903C00 9C400000`
- Note Off MIDI 2.0 (`G=0, Ch=0, Note=60, Vel=0`): `40803C00 00000000`
- CC MIDI 2.0 (`G=0, Ch=0, CC=7, Val=0xFFFFFFFF`): `40B00700 FFFFFFFF`
- Note On MIDI 1.0 UMP (`G=0, Ch=0, Note=60, Vel7=78`): `20903C4E`
- Note Off MIDI 1.0 UMP (`G=0, Ch=0, Note=60, Vel7=0`): `20803C00`
- CC MIDI 1.0 UMP (`G=15, Ch=15, CC=11, Val7=127`): `2FBF0B7F`

## 4. Conversões MIDI 1.0 / MIDI 2.0
- **16 → 7 bits (`velocity_16_to_7`):** `vel16 >> 9` (`0 -> 0`, `0x8000 -> 64`, `0xFFFF -> 127`). Em `encode_midi1_note` para `NoteOn` com `velocity > 0` que quantizaria para `0` (`1..511`), satura em `1` para não virar `NoteOff` MIDI 1.0.
- **Note On vel=0 em MIDI 1.0:** Na decodificação de `MT=0x2, Status=0x9, Vel7=0`, converte semanticamente para `EventType::NoteOff` com `velocity = 0`.
- **32 → 7 bits (`to_midi1_7bit`):** `val32 >> 25` (`0 -> 0`, `0xFFFFFFFF -> 127`). Nota: um `ControlEvent` vindo de ADC de 12 bits é escalado para 32 bits, não possuindo 32 bits de precisão física real.
- **7 → 16 / 32 bits (`velocity_7_to_16`, `cc_7_to_32`):** Escalonamento normativo *min-center-max* com replicação de bits (`0 -> 0`, `64 -> centro`, `127 -> máximo`).

## 5. Integração com KeyScanner e AnalogBank
- Em `firmware/rp2350/src/main.cpp`:
  - `UmpRouter ump_router(0, UmpProtocolMode::Midi2)` converte os eventos emitidos por `KeyScanner` (`NoteEvent`) e `AnalogBank` (`ControlEvent`) em pacotes UMP de grupo `0`.
  - `UmpPacketQueue<32> ump_queue` desacopla a codificação da impressão diagnóstica UART0 (`[UMP] mt=4 group=0 words=2 data=XXXXXXXX YYYYYYYY`), drenando até 4 pacotes por iteração fora da janela crítica de leitura.
  - Todos os logs anteriores (`[BOOT]`, `[GPIO]`, `[ADC]`, `[SCAN]`, `[NOTE_ON]`, `[NOTE_OFF]`, `[ADC_CC]`, `[HEARTBEAT]`, `[METRICS]`) foram preservados.

## 6. Testes Executados e Resultados
1. **Compilação C++17 estrita (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `ump.cpp` compilado com **0 warnings / 0 erros**.
2. **Host C++23 + Sanitizers (ASan + UBSan) + CTest (`ctest --test-dir /tmp/harmonia_san_build --output-on-failure`):** **38/38 testes aprovados** em `0.61 sec` (31 anteriores + 7 novos casos UMP cobrindo todos os 17 requisitos normativos).
3. **Testes Python (`python3 -m unittest scripts/test_verify_wokwi_log.py scripts/test_verify_ump_log.py -v`):** **13/13 testes aprovados** em `0.001 s`.
4. **Build RP2350 (`pico2`, Ninja, `PICO_SDK_PATH=/home/lucasramos/pico-sdk`):** `harmonia_diag.elf` e `harmonia_diag.uf2` compilados sem erros.
5. **Wokwi Lint (`wokwi-cli lint hardware/simulation/wokwi`):** `✓ No issues found`.

## 7. Evidências do Wokwi
- **Cenário UMP (`scenario_ump.yaml`, `task-431`):** `Scenario completed successfully` (`exit 0`) e `verify_ump_log.py` retornou **`[PASS]` (`UMP Packets=44, NoteEvents=8, CCs=[7, 11]`)**, validando `K1` (`40903C00 9C400000` / `40803C00 00000000`), acorde `K1+K2+K3` sem ghosting e `CC7`/`CC11` nos extremos `00000000` e `FFFFFFFF`.
- **Cenário Analógico (`scenario_adc.yaml`, `task-435`):** `Scenario completed successfully` (`exit 0`).
- **Regressão Digital (`scenario.yaml`, `task-439`):** `Scenario completed successfully` (`exit 0`) e `verify_wokwi_log.py` retornou **`[PASS]` (`Fases=20/20`, `Heartbeats=1`)**, com métricas `[METRICS] [2000428 us] max_scan_dt=1000 max_scan_late=0 max_adc_proc=14 max_adc_late=0 adc_events=2 ump_drops=0`.

## 8. Custos de CPU / RAM
- `UmpPacket`: `20 bytes` (`4 × uint32_t` + `uint8_t` com alinhamento).
- `UmpPacketQueue<32>`: `~672 bytes` estáticos em RAM (zero heap).
- Codificação/decodificação: complexidade $O(1)$ apenas com deslocamentos e máscaras de bits; nenhum impacto mensurável no loop de `1 kHz` (`max_scan_dt=1000 us`, `max_scan_late=0 us`, `ump_drops=0`).

## 9. Limitações
- Apenas `Note On`, `Note Off` e `Control Change` (MIDI 1.0 e MIDI 2.0 Channel Voice) são suportados nesta etapa; outros tipos (`MT=0x0/0x3/0x5/0xF`, Pitch Bend, Program Change, SysEx) retornam erro explícito.
- O transporte físico USB MIDI 2.0 (TinyUSB) e o enlace binário RP2350 ↔ RPi 5 ainda não foram implementados.

## 10. Pendências de Hardware Físico
- Medir *throughput* real e ausência de contenção ao substituir o log hexadecimal UART0 pelo transporte USB MIDI 2.0 / enlace SPI/UART binário no Raspberry Pi Pico 2 físico.

## 11. Preparação para a Etapa 06
- O núcleo `harmonia_core` agora entrega pacotes `UmpPacket` prontos para envio direto ao driver USB MIDI 2.0 do TinyUSB (`MT=0x4` / fallback `MT=0x2`) na Etapa 06, preservando total separação entre codificação e transporte.
