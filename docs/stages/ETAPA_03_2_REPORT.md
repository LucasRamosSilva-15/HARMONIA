# Relatório da Etapa 03.2 — Integração da Aquisição Digital ao RP2350 e Wokwi

## 1. Resumo e Integridade de Eventos (`KeyScanner`)
- **Problema identificado:** Quando `max_events` do chamador era menor que o número de transições simultâneas, `KeyScanner::scan` descartava silenciosamente eventos excedentes após atualizar o `KeyDebouncer`. Além disso, mudanças no `KeyMapper` enquanto uma tecla permanecia pressionada fariam o `NoteOff` ser enviado para a nova nota em vez da nota original.
- **Correções implementadas em `software/core/include/harmonia/core/key_scanner.hpp`:**
  - Adicionada fila circular estática interna (`kQueueCapacity = kMaxKeys * 2`, sem alocação dinâmica) e métodos `scan_with_status(...)` / `drain(...)`.
  - Buffers pequenos do chamador não perdem eventos: os excedentes permanecem na fila e são drenados nas chamadas seguintes.
  - Em saturação extrema da fila interna, `ScanResult::overflow` é sinalizado e `dropped_note_on` é contabilizado; `NoteOff` tem prioridade e nunca é descartado silenciosamente.
  - Rastreamento de `active_notes_` por tecla garante que `NoteOff` sempre desligue exatamente o canal e nota do `NoteOn` original.
- **Limitação explícita:** Em saturação extrema contínua (fila cheia e `max_events == 0`), `NoteOn` excedentes são descartados de forma controlada e explícita (sem promessa irrealista de perda zero infinita).

## 2. Estratégia de Debounce e Latência
- Mantido `kDebounceUs = 20000` (20 ms) para diagnóstico inicial e estabilidade em botões mecânicos/simulados.
- **Incompatibilidade documentada:** 20 ms de debounce já excede sozinho a meta final ponta a ponta de `< 10 ms`. Em etapas futuras com hardware definitivo (chaves de teclado musical de duplo contato), o debounce será reduzido ou substituído por detecção de borda no primeiro contato com janela de estabilização de ~1–3 ms, preservando a medição de $\Delta t$ para velocity.

## 3. Circuito 2×2 no Wokwi e Orientação dos Diodos
- **Arquivo atualizado:** `hardware/simulation/wokwi/diagram.json`
- **Pinagem:**
  - `UART0`: `GP0` (TX) -> `$serialMonitor:RX`, `GP1` (RX) -> `$serialMonitor:TX`
  - `ROW0`: `GP10`, `ROW1`: `GP11`
  - `COL0`: `GP14` (pull-up), `COL1`: `GP15` (pull-up)
- **Sentido da corrente e diodos (`chip-diode`):**
  - O Wokwi não possui um diodo de sinal nativo (`wokwi-diode` gera `unknown-part-type`), portanto foi adotado o chip comunitário `"chip-diode": "github:drf5n/Wokwi-Chip-Diode@1.0.0"` com pinos `ANODE` e `CATHODE`.
  - Colunas em pull-up (3.3 V) e linha selecionada em nível LOW (0 V).
  - Corrente flui de `COL_c -> Botão k_i -> ANODE -> CATHODE do diodo d_i -> ROW_r`.
  - Mapeamento: `k1` (N60) em `(ROW0, COL0)`, `k2` (N61) em `(ROW0, COL1)`, `k3` (N62) em `(ROW1, COL0)`, `k4` (N63) em `(ROW1, COL1)`.

## 4. Driver GPIO RP2350
- **Arquivos criados/modificados:**
  - `firmware/rp2350/include/gpio_matrix_driver.hpp` (criado)
  - `firmware/rp2350/src/gpio_matrix_driver.cpp` (criado)
  - `firmware/rp2350/CMakeLists.txt` (modificado — linka `harmonia::core` em C++17)
  - `firmware/rp2350/src/main.cpp` (modificado — varredura 1 kHz, logs `[BOOT]`, `[GPIO]`, `[SCAN]`, `[NOTE_ON]`, `[NOTE_OFF]`, `[WARN]`, `[ERROR]`, `[HEARTBEAT]`)
- **Proteção elétrica:** Apenas a linha ativa é colocada em `GPIO_OUT` `LOW`; as demais linhas permanecem em alta impedância (`GPIO_IN` sem pull), evitando conflito elétrico entre linhas. Tempo de estabilização de `5 µs` via `busy_wait_us_32(5)`.

## 5. Resultados Reais Executados
1. **Regressão CTest no host (`ctest --test-dir build --output-on-failure`):**
   - **Resultado:** **21/21 testes aprovados** em `0.05 sec` (18 anteriores + 3 novos testes de integridade/overflow/remapeamento).
2. **Compilação RP2350 (`PICO_SDK_PATH=/home/lucasramos/pico-sdk`, `pico2`, Ninja):**
   - **Resultado:** Compilado com sucesso (`/usr/bin/arm-none-eabi-gcc`, `rp2350-arm-s`), linkando `libharmonia_core.a` (C++17).
   - **Artefatos gerados:** `firmware/rp2350/build/harmonia_diag.elf` (414 KB) e `firmware/rp2350/build/harmonia_diag.uf2` (37 KB).
3. **Execução no Wokwi (`wokwi-cli v0.29.1`):**
   - **Boot + Heartbeat:** **Aprovado** (`[BOOT]`, `[GPIO]`, `[SCAN]` e `[HEARTBEAT] [2000334 us] #1` observados na UART0).
   - **Problema encontrado e corrigido:** `wokwi-diode` foi rejeitado pelo Wokwi (`unknown-part-type`); `diagram.json` foi corrigido para usar `chip-diode` (`github:drf5n/Wokwi-Chip-Diode@1.0.0`).
   - **Testes de pressionamento/acordes no Wokwi:** **Pendente** de reexecução com `diagram.json` atualizado (`chip-diode`) e automação de cliques/cenário.

## 6. Procedimento de Validação Manual no Wokwi (VS Code)
1. Abrir `hardware/simulation/wokwi/` no VS Code com a extensão Wokwi Simulator.
2. Iniciar a simulação (`F1 -> Wokwi: Start Simulator`).
3. Verificar `[BOOT]`, `[GPIO]`, `[SCAN]` e `[HEARTBEAT]` no Monitor Serial.
4. Testar individualmente `k1`..`k4` (`[NOTE_ON]`/`[NOTE_OFF]` para notas `60..63`), acordes de 2 e 3 teclas, 4 teclas simultâneas, ausência de notas fantasmas e reinicialização limpa.

## 7. Limitações e Próxima Etapa
- **Limitações:** Testes de aquisição no host validam lógica/overflow; compilação RP2350 valida toolchain e linkage; simulação eletrônica no Wokwi e testes em hardware físico continuam pendentes de validação interativa/bancada.
- **Próxima etapa recomendada:** **Etapa 04 — Aquisição Analógica e Filtragem** (após validação manual da matriz 2×2 no Wokwi).
