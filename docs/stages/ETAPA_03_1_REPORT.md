# Relatório da Etapa 03.1 — Motor de Aquisição Digital: Matriz e Debounce em C++ Portável

## 1. Arquitetura Implementada
A aquisição digital foi dividida em camadas desacopladas em `harmonia_core`, totalmente compatíveis com **C++17** (mínimo compartilhado) e **C++23** (host/testes), sem alocação dinâmica, sem threads e sem dependências de hardware:

- **Matriz Lógica (`KeyMatrixState<MaxRows, MaxCols>`):** armazena estados booleanos em `std::array<bool, MaxRows * MaxCols>` com dimensões configuráveis em tempo de execução, conversão bidirecional `(row, col) <-> index` e verificação de limites. Permite injeção direta de estados para simulação lógica no host.
- **Debouncer (`KeyDebouncer<MaxKeys>`):** máquina de estados por tecla baseada em timestamp `uint32_t` em microssegundos (`now_us`).
- **Mapeador Musical (`KeyMapper<MaxKeys>`):** converte `KeyTransition` (`Pressed`/`Released`) em `NoteEvent` com canal (`0..15`), nota MIDI (`0..127` ou `kUnmappedNote`) e velocity fixa configurável.
- **Orquestrador (`KeyScanner<MaxRows, MaxCols>`):** percorre as teclas ativas da matriz, aplica debounce e mapeamento, escrevendo os eventos em um buffer pré-alocado pelo chamador.

## 2. Arquivos Criados e Modificados
- `software/core/CMakeLists.txt` (modificado — padrão mínimo ajustado de `cxx_std_23` para `cxx_std_17`)
- `software/core/include/harmonia/core/midi_event.hpp` (modificado — `operator==` e `operator!=` explícitos compatíveis com C++17)
- `software/core/src/midi_event.cpp` (modificado — inicialização posicional de `NoteEvent` compatível com C++17)
- `software/core/include/harmonia/core/key_matrix.hpp` (criado)
- `software/core/include/harmonia/core/key_debouncer.hpp` (criado)
- `software/core/include/harmonia/core/key_mapper.hpp` (criado)
- `software/core/include/harmonia/core/key_scanner.hpp` (criado)
- `software/tests/CMakeLists.txt` (modificado — adicionado `test_key_acquisition.cpp`)
- `software/tests/test_key_acquisition.cpp` (criado — 12 novos casos de teste)
- `docs/stages/ETAPA_03_1_REPORT.md` (criado)
- `README.md` (atualizado)

## 3. Algoritmo de Debounce e Desempenho
- **Algoritmo:** Cada tecla mantém `{last_raw, stable, last_change_us, pending_change}`. Quando `raw != last_raw`, o timer reinicia em `now_us`. Se `pending_change` permanecer ativo por `static_cast<uint32_t>(now_us - last_change_us) >= debounce_us`, o estado `stable` é atualizado e uma única transição (`Pressed` ou `Released`) é emitida.
- **Independência temporal:** Não usa `sleep()` nem contagem fixa de ticks; funciona com intervalos de amostragem irregulares e lida com wrap-around de `uint32_t` (~71,5 minutos em µs) via aritmética modular unsigned.
- **Complexidade:** Tempo $O(R \times C)$ por varredura completa, memória $O(R \times C)$ estática (8 bytes por slot de debounce + 1 byte por nota mapeada + 1 byte por estado da matriz; para 64 teclas, < 700 bytes de RAM no RP2350), zero alocações no heap.

## 4. Resultados Reais da Validação
1. **Build Host C++23 (`cmake --build build -j` com `-Werror`):** 0 warnings, 0 erros.
2. **CTest (`ctest --test-dir build --output-on-failure`):** **18/18 testes aprovados** em `0.04 sec` (6 testes anteriores de `midi_event` + 12 novos testes de aquisição em matriz 2×2).
3. **Compilação estrita C++17 (`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror`):** `midi_event.cpp` e `key_scanner.hpp` compilados com **0 warnings** e **0 erros**.
4. **Sanitizers (ASan/UBSan):** Não executados (mesma limitação anterior: `libasan` e `libubsan` ausentes no host).

## 5. Limitações e Pendências
- **Anti-ghosting elétrico:** Os testes lógicos validam o algoritmo de matriz e múltiplos pressionamentos simultâneos, mas **não comprovam ausência de ghosting no hardware**; a prevenção elétrica depende exclusivamente do circuito físico e da orientação correta dos diodos em cada tecla.
- **Velocity:** Utiliza velocity fixa configurável; cálculo de velocity por duplo contato será tratado em etapa futura.
- **Pendências anteriores mantidas:** Instalação de `arm-none-eabi-gcc`, `pico-sdk`, `libasan`/`libubsan`, compilação `pico2` e validação Wokwi da Etapa 03.0 continuam pendentes.

## 6. Critérios para a Etapa 03.2
- Toolchain ARM (`arm-none-eabi-gcc`) e `pico-sdk` configurados no ambiente para integrar `KeyScanner` aos pinos GPIO reais do RP2350 (varredura física de linhas/colunas).
