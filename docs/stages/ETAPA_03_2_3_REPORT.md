# Relatório da Etapa 03.2.3 — Investigação e Simulação do Anti-Ghosting no Wokwi

## 1. Viabilidade Técnica da Wokwi Custom Chips API
- **Por que `drf5n/Wokwi-Chip-Diode@1.0.0` falhou:** O chip comunitário configura `pin_watch(ANODE, ...)` e nunca observa `CATHODE`. Na varredura ativo-baixo do HARMONIA (`ROW` ligado ao `CATHODE` indo para `LOW`, `COL` ligado ao `ANODE` em pull-up), o chip nunca dispara seu callback.
- **Modelo digital implementado (`hardware/simulation/wokwi/chips/diode_1n4148.chip.c`):**
  - `CATHODE` configurado como `INPUT_PULLUP` com `pin_watch(CATHODE, BOTH)`.
  - `ANODE` inicia como `INPUT` (alta impedância).
  - Quando `CATHODE == LOW`, `ANODE` passa para `OUTPUT_LOW`; quando `CATHODE == HIGH` (linha em `GPIO_IN`), `ANODE` volta para `INPUT` (alta impedância).
  - **Ausência de realimentação/oscilação:** Fluxo estritamente unidirecional (`CATHODE -> ANODE`), sem watch em `ANODE`.
  - **Build reproduzível:** Compilado com `wokwi-cli chip compile` (WASI-SDK 25) para `hardware/simulation/wokwi/chips/diode_1n4148.chip.wasm` (64.0 KB) e integrado via `wokwi.toml`.

## 2. Limitações do Modelo Digital
- Trata-se de uma **aproximação lógica digital** específica para varredura ativo-baixo com colunas em pull-up.
- **Não** simula queda de tensão direta ($V_f \approx 0.7\text{ V}$), corrente reversa de fuga, capacitância parasita ou proteção caso uma coluna fosse dirigida ativamente em `HIGH`. Não substitui validação física com diodos **1N4148** reais.

## 3. Resultados Comparativos (Circuito A vs. Circuito B)

| Cenário (`scenario.yaml`) | Circuito A — Sem Diodos (`diagram.no_diodes.json`) | Circuito B — Com `diode_1n4148` (`diagram.json`) |
| :--- | :--- | :--- |
| Boot, GPIO, Scan, Heartbeat | Aprovado | Aprovado |
| Teclas individuais (`K1`..`K4` -> `60`..`63`) | Aprovado | Aprovado |
| Acordes de 2 teclas (`K1+K2`, `K1+K3`, `K2+K4`) | Aprovado | Aprovado |
| **3 vértices (`K1+K2+K3`) — Anti-ghosting** | **Reprovado (controle negativo):** gerou `[NOTE_ON] note=63` e `[NOTE_OFF] note=63` fantasma | **Aprovado:** reconheceu apenas `60, 61, 62` sem `note=63` |
| 4 teclas simultâneas (`K1+K2+K3+K4`) | Aprovado | Aprovado |
| Estabilidade (hold 250 ms, zero duplicatas/notas presas) | Aprovado | Aprovado |
| **`verify_wokwi_log.py`** | `[FAIL]` (2 violações de anti-ghosting detectadas) | **`[PASS]` (`Fases=20/20`, zero violações)** |

## 4. Comandos Executados e Regressão
- `wokwi-cli chip compile hardware/simulation/wokwi/chips/diode_1n4148.chip.c -o hardware/simulation/wokwi/chips/diode_1n4148.chip.wasm` -> Sucesso (64.0 KB).
- `wokwi-cli lint hardware/simulation/wokwi` -> `✓ No issues found`.
- `wokwi-cli hardware/simulation/wokwi --scenario scenario.yaml --serial-log-file /tmp/harmonia_wokwi_diodes.log --timeout 10000` -> `Scenario completed successfully` (exit 0).
- `python3 scripts/verify_wokwi_log.py /tmp/harmonia_wokwi_diodes.log` -> **`[PASS]` (20/20 fases)**.
- `python3 -m unittest scripts/test_verify_wokwi_log.py -v` -> **8/8 testes aprovados**.
- `ctest --test-dir build --output-on-failure` -> **21/21 testes C++ aprovados**.

## 5. Pendências de Hardware Físico e Recomendação
- **Critério de encerramento atingido:** **Condição A — Sucesso** (modelo digital funcional nos cenários e anti-ghosting validado dentro das limitações da simulação).
- **Pendência física:** Validar matriz real com diodos 1N4148 em bancada (quedas de tensão $V_f$, tempo de estabilização e ruído de contato).
- **Próxima etapa recomendada:** **Etapa 04 — Aquisição Analógica e Filtragem** (aguardando autorização).
