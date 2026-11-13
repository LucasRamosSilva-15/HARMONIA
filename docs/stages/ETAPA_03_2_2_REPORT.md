# Relatório da Etapa 03.2.2 — Automação e Validação da Matriz 2×2 no Wokwi CLI

## 1. Arquivos Criados e Modificados
- `hardware/simulation/wokwi/scenario.yaml` (atualizado — `wait-serial` com substrings completas `ch=0 note=X vel=40000` / `vel=0` e separação explícita da fase de 3 vértices `k1+k2+k3` antes de `k1+k2+k3+k4`)
- `scripts/verify_wokwi_log.py` (atualizado — verificação determinística por fase, detecção explícita de `ghost_violations`, eventos extras, canal/velocity incorretos, duplicatas, `NOTE_OFF` órfãos, notas presas e rejeição de logs vazios/incompletos)
- `scripts/test_verify_wokwi_log.py` (criado — 8 testes unitários para o analisador Python)
- `docs/stages/ETAPA_03_2_2_REPORT.md` (atualizado)

## 2. Versão e Verificação do Wokwi CLI
- **Executável:** `/home/lucasramos/bin/wokwi-cli`
- **Versão:** `0.29.1 (0aa185144b92)`
- **Lint do diagrama (`wokwi-cli lint hardware/simulation/wokwi`):** `✓ No issues found`.
- **Token CI (`WOKWI_CLI_TOKEN`):** `UNSET` no ambiente deste processo. Nenhuma credencial foi gravada em arquivos ou logs.

## 3. Comandos Executados e Resultados Reais
1. **`python3 -m unittest scripts/test_verify_wokwi_log.py -v`:** **8/8 testes aprovados** em `0.001s` (cobrindo log válido completo, log vazio, log incompleto, ghosting `note=63` em `D_3VERTICES_ON`, nota presa, `NOTE_ON` duplicado, `NOTE_OFF` órfão e canal incorreto).
2. **`ctest --test-dir build --output-on-failure`:** **21/21 testes C++ aprovados** em `0.04s`.
3. **`/home/lucasramos/bin/wokwi-cli lint hardware/simulation/wokwi`:** `✓ No issues found`.
4. **Execução do `scenario.yaml` no Wokwi CLI:** **Pendente** neste processo porque `WOKWI_CLI_TOKEN` está `UNSET` no ambiente do agente (os cenários **não** foram declarados aprovados).

## 4. Comando de Execução no Terminal com Token
No terminal onde `WOKWI_CLI_TOKEN` estiver exportado:

```bash
/home/lucasramos/bin/wokwi-cli hardware/simulation/wokwi \
  --scenario hardware/simulation/wokwi/scenario.yaml \
  --serial-log-file /tmp/harmonia_wokwi.log \
  --timeout 15000

python3 scripts/verify_wokwi_log.py /tmp/harmonia_wokwi.log
```

## 5. Limitações e Critérios Pendentes
- A execução real do `scenario.yaml` (comprovação de `chip-diode`, 4 teclas, acordes e anti-ghosting no simulador) permanece pendente de execução com `WOKWI_CLI_TOKEN` ativo.
- A validação em simulador (Wokwi) continua distinta da validação em hardware físico (RP2350 + diodos 1N4148 reais).
