# Relatório da Etapa 03.2.2 — Automação e Validação da Matriz 2×2 no Wokwi CLI

## 1. Arquivos Criados e Modificados
- `hardware/simulation/wokwi/diagram.json` (atualizado durante diagnóstico: testados `chip-diode`, `wokwi-led` e ligação direta sem diodos para isolar o comportamento do simulador)
- `hardware/simulation/wokwi/scenario.yaml` (atualizado — `wait-serial` com substrings completas `ch=0 note=X vel=40000`/`vel=0` e escalonamento `delay: 2ms` nos acordes para ordem determinística)
- `scripts/verify_wokwi_log.py` (atualizado — `EVENT_RE.finditer` para lidar com linhas coladas da UART0 no `--serial-log-file`, verificação determinística de 20 fases e detecção de `ghost_violations`)
- `scripts/test_verify_wokwi_log.py` (criado — 8 testes unitários para o analisador Python)
- `docs/stages/ETAPA_03_2_2_REPORT.md` (atualizado com resultados reais)

## 2. Versão e Execução do Wokwi CLI
- **Executável:** `/home/lucasramos/bin/wokwi-cli` (`0.29.1`)
- **Token:** Carregado via `$HOME/.config/harmonia/wokwi.env` exclusivamente no processo do `wokwi-cli` (não exibido nem gravado).
- **Comando executado:**
  ```bash
  /home/lucasramos/bin/wokwi-cli hardware/simulation/wokwi \
    --scenario scenario.yaml \
    --serial-log-file /tmp/harmonia_wokwi.log \
    --timeout 10000
  ```
  - **Resultado do `wokwi-cli`:** `Scenario completed successfully` (exit code `0`).

## 3. Investigação dos Diodos no Wokwi (Achados Reais)
1. `wokwi-diode`: rejeitado pelo Wokwi CLI (`unknown-part-type`).
2. `github:drf5n/Wokwi-Chip-Diode@1.0.0`: implementa `pin_watch(ANODE)` apenas no pino `ANODE`, não reagindo quando o `CATHODE` é puxado para `LOW` pela varredura de linha (`ROW`).
3. `wokwi-led`: queda de tensão direta ($V_f \approx 1.8\text{ V}$) impede que a coluna em pull-up (3.3 V) atinja nível lógico `LOW` ($V_{IL}$) no RP2350.
4. **Ligação direta (sem diodos):** Confirmou 100% da lógica do firmware RP2350, `GpioMatrixDriver2x2` e `KeyScanner` nas 20 fases (`Boot`, `GPIO`, `Scan`, `Heartbeat`, `K1..K4`, acordes `K1+K2`, `K1+K3`, `K2+K4`, `K1+K2+K3+K4` e hold prolongado), **e reproduziu exatamente o ghosting esperado** em `note=63` durante `K1+K2+K3` (sem diodos).

## 4. Resultados Reais dos Testes
- **`python3 -m unittest scripts/test_verify_wokwi_log.py -v`:** **8/8 testes aprovados** (`0.001s`).
- **`ctest --test-dir build --output-on-failure`:** **21/21 testes C++ aprovados** (`0.04s`).
- **`python3 scripts/verify_wokwi_log.py /tmp/harmonia_wokwi.log`:**
  - `Boot=True, GPIO=True, Scan=True, Heartbeats=1, Fases=20/20`
  - **Falha esperada e comprovada (sem diodos no simulador):**
    `[FAIL] Violacoes de anti-ghosting: ['[D_3VERTICES_OFF] nota fantasma 63: [NOTE_ON]  [2414334 us] ch=0 note=63 vel=40000', '[D_3VERTICES_OFF] nota fantasma 63: [NOTE_OFF] [2445334 us] ch=0 note=63 vel=0']`
  - Zero duplicatas, zero `NOTE_OFF` órfãos, zero notas presas.

## 5. Limitações e Critérios Pendentes
- **Anti-ghosting no Wokwi:** **Não declarado aprovado**, pois o Wokwi não possui diodo de sinal nativo e o chip comunitário testado (`drf5n/Wokwi-Chip-Diode@1.0.0`) não suporta varredura com `CATHODE` em `LOW`. A validação definitiva de anti-ghosting requer **diodos físicos 1N4148 em bancada** (ou desenvolvimento futuro de um custom chip Wokwi próprio em WebAssembly).
- **Hardware físico:** Testes em RP2350 real com matriz física e diodos 1N4148 permanecem pendentes.
