# Relatório da Etapa 02 — Infraestrutura C++23, CMake e Testes Automatizados

## 1. Arquivos Criados e Modificados
- `CMakeLists.txt` (criado — raiz do projeto, C++23 no host, separação host vs. firmware)
- `cmake/CompilerWarnings.cmake` (criado — `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` e `-Werror` opcional)
- `cmake/Sanitizers.cmake` (criado — ASan/UBSan apenas para builds de host)
- `software/core/CMakeLists.txt` (criado — target `harmonia_core` / `harmonia::core`)
- `software/core/include/harmonia/core/midi_event.hpp` (criado)
- `software/core/src/midi_event.cpp` (criado)
- `software/tests/CMakeLists.txt` (criado — Catch2 `v3.7.1` via `FetchContent` + `catch_discover_tests`)
- `software/tests/test_midi_event.cpp` (criado — 6 casos de teste)
- `README.md` (atualizado — instruções de build e CTest)
- `docs/stages/ETAPA_02_REPORT.md` (criado)

## 2. Arquitetura da Biblioteca (`harmonia_core`)
- Biblioteca estática C++23 sem dependências externas (independente de TinyUSB, Qt e FluidSynth) e sem alocação dinâmica.
- Define `EventType`, `NoteEvent` (com `velocity` de 16 bits para suportar resolução MIDI 2.0 e escalonamento MIDI 1.0), constantes de limite (`kMaxChannel = 15`, `kMaxNote = 127`) e funções de validação/construção (`is_valid_channel`, `is_valid_note`, `is_valid_note_event`, `make_note_on`, `make_note_off`).

## 3. Comandos Executados e Resultados Reais
1. **Configuração e build padrão (com `-Werror`):**
   ```bash
   cmake -B build -DHARMONIA_WARNINGS_AS_ERRORS=ON
   cmake --build build -j
   ```
   - **Resultado:** Compilado com sucesso (`GCC 16.2.1`, C++23), **0 warnings**, **0 erros**.
2. **Execução do CTest:**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
   - **Resultado:** **6/6 testes aprovados** em `0.01 sec` (`100% tests passed, 0 tests failed out of 6`).
3. **Verificação de Sanitizers (ASan + UBSan):**
   ```bash
   cmake -B build-san -DHARMONIA_ENABLE_SANITIZERS=ON -DHARMONIA_WARNINGS_AS_ERRORS=ON
   cmake --build build-san --target harmonia_core_tests -j
   ```
   - **Resultado:** Compilação dos objetos concluída, mas **linkagem falhou** porque `/usr/lib64/libasan.so.8.0.0` não está instalado no sistema (`libasan` e `libubsan` ausentes no Fedora 44).

## 4. Limitações e Riscos
- **Sanitizers no host:** Requer instalação de `libasan` e `libubsan` (`sudo dnf install libasan libubsan`) para execução completa.
- **Wokwi (Etapa 01.8):** Continua pendente de `arm-none-eabi-gcc` e `pico-sdk` para validação de compilação/simulação.

## 5. Próxima Etapa Recomendada
- **Etapa 03 — Aquisição Digital no RP2350:** implementar lógica de varredura matricial, anti-ghosting e debounce no `harmonia_core` (testável no host) e integrar ao firmware RP2350.
