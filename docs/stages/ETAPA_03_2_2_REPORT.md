# Relatório da Etapa 03.2.2 — Automação e Validação da Matriz 2×2 no Wokwi CLI

## 1. Arquivos Criados
- `hardware/simulation/wokwi/scenario.yaml` (cenário automatizado cobrindo Testes A, B, C, D e E)
- `scripts/verify_wokwi_log.py` (analisador determinístico de log serial para detectar duplicatas, `NOTE_OFF` órfãos, notas presas e inconsistências de pares ON/OFF)
- `docs/stages/ETAPA_03_2_2_REPORT.md` (este relatório)

## 2. Versão e Verificação do Wokwi CLI
- **Executável:** `/home/lucasramos/bin/wokwi-cli`
- **Versão:** `0.29.1 (0aa185144b92)`
- **Lint do diagrama (`wokwi-cli lint hardware/simulation/wokwi`):** `✓ No issues found` (validando `diagram.json` com `chip-diode`).
- **Token CI (`WOKWI_CLI_TOKEN`):** `UNSET` no ambiente do processo do agente (o usuário exportou apenas em sua sessão interativa de terminal, sem persistir em `~/.bashrc` / ambiente global). Nenhuma credencial foi gravada em arquivos ou logs.

## 3. Estrutura dos Cenários (`scenario.yaml`)
- **Teste A (Inicialização):** Verifica `[BOOT]`, `[GPIO]`, `[SCAN]` e `[HEARTBEAT]`.
- **Teste B (Teclas individuais):** Pressiona e solta `k1`..`k4` por `50 ms` (> `20 ms` de debounce), verificando `note=60`..`63` em `[NOTE_ON]` e `[NOTE_OFF]`.
- **Teste C (Acordes):** Testa `k1+k2`, `k1+k3`, `k2+k4`, `k1+k2+k3` e `k1+k2+k3+k4`.
- **Teste D (Anti-ghosting):** Pressiona 3 vértices (`k1+k2+k3`) antes de completar `k4`, combinado à verificação de pares no log.
- **Teste E (Estabilidade):** Pressionamento prolongado (`250 ms`) e soltura limpa.

## 4. Comandos Executados e Resultados Reais
1. `wokwi-cli lint hardware/simulation/wokwi` -> **`✓ No issues found`**.
2. `cmake --build build -j && ctest --test-dir build --output-on-failure` -> **`21/21` testes aprovados** em `0.05 sec`.
3. `cmake --build firmware/rp2350/build` -> **`ninja: no work to do`** (ELF/UF2 já atualizados para `pico2` / `rp2350-arm-s`).
4. **Execução do `scenario.yaml`:** **Pendente** no shell do agente porque `WOKWI_CLI_TOKEN` está `UNSET` neste processo.

## 5. Como Executar a Validação Completa no Terminal com Token
No terminal onde `WOKWI_CLI_TOKEN` está exportado (ou adicionando-o ao seu ambiente):

```bash
/home/lucasramos/bin/wokwi-cli hardware/simulation/wokwi \
  --scenario hardware/simulation/wokwi/scenario.yaml \
  --serial-log-file /tmp/harmonia_wokwi.log \
  --timeout 15000

python3 scripts/verify_wokwi_log.py /tmp/harmonia_wokwi.log
```

## 6. Limitações e Critérios Pendentes
- **Execução do cenário 2×2:** Aguarda execução com `WOKWI_CLI_TOKEN` ativo para comprovar na prática o comportamento elétrico de `github:drf5n/Wokwi-Chip-Diode@1.0.0`, as 4 teclas individuais, acordes, ausência de ghosting e zero notas presas.
- **Hardware físico:** A validação no Wokwi (simulador) não substitui testes em bancada física com RP2350 real, diodos 1N4148 reais e osciloscópio/analisador lógico.
