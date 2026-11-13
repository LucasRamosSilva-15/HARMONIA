# Relatório da Etapa 03.0 — Validação do Ambiente Embarcado e Simulação Wokwi

## 1. Ambiente e Versões Verificadas
- **SO Host:** Fedora Linux 44 (KDE Plasma Desktop Edition)
- **Git:** `2.56.0`
- **CMake:** `4.3.0`
- **Ninja:** `1.13.2` (`/usr/bin/ninja`)
- **GCC (Host):** `16.2.1`
- **Ferramentas Embarcadas / Debug:** `arm-none-eabi-gcc`, `gdb-multiarch`, `openocd`, `picotool`, `wokwi-cli` e `PICO_SDK_PATH` **não instalados** no ambiente atual.
- **Bibliotecas de Sanitizer:** `libasan` e `libubsan` **não instalados** no sistema.
- **Privilégios:** `sudo -n true` falhou (`sudo: uma senha é necessária`), impedindo instalação automática de pacotes RPM sem interação do usuário.

## 2. Inspeção Realizada (Arquivos Preservados)
Foram inspecionados sem necessidade de alteração:
- `firmware/rp2350/CMakeLists.txt` (configurado para `PICO_BOARD=pico2`, `PICO_PLATFORM=rp2350`, `C++17`, `stdio_uart=1`, `stdio_usb=0`).
- `firmware/rp2350/src/main.cpp` (GPIO14 em `gpio_pull_up`, debounce 20 ms, `PICO_DEFAULT_LED_PIN`, heartbeat 1000 ms, compatível com núcleo único).
- `hardware/simulation/wokwi/diagram.json` (`board-pi-pico-2`, UART0 em GP0/GP1, botão `btn1` em GP14 + GND.4).
- `hardware/simulation/wokwi/wokwi.toml` (caminhos relativos `../../../firmware/rp2350/build/harmonia_diag.{uf2,elf}` conferidos a partir de `hardware/simulation/wokwi/`).

## 3. Comandos Executados e Resultados Reais

1. **Verificação dos testes no host:**
   ```bash
   ctest --test-dir build --output-on-failure
   ```
   - **Resultado:** `6/6` testes aprovados em `0.02 sec`.

2. **Análise de compatibilidade C++17 vs. C++23 em `harmonia_core`:**
   ```bash
   g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -I software/core/include -c software/core/src/midi_event.cpp -o /tmp/midi_event_17.o
   ```
   - **Resultado:** Falhou com erros esperados de C++20 em modo estrito C++17:
     - `midi_event.hpp:31`: `constexpr bool operator==(const NoteEvent&) const noexcept = default;` (requer `-std=c++20`).
     - `midi_event.cpp:19-22, 35-38`: *designated initializers* (`.type = ...`) requerem `-std=c++20`.
     - Além disso, `software/core/CMakeLists.txt` declara `target_compile_features(harmonia_core PUBLIC cxx_std_23)`.
   - **Estratégia sustentável proposta:**
     - Manter `harmonia_core` (ou ao menos o subconjunto compartilhado de aquisição, debounce e eventos UMP) compatível com **C++17** (`cxx_std_17` como requisito mínimo do target), evitando recursos exclusivos de C++20/23 nos headers e fontes compartilhados, enquanto testes e aplicação Qt no host continuam em **C++23**. Caso o `arm-none-eabi-gcc` instalado suporte C++20/23 com o Pico SDK 2.1.0, essa elevação poderá ser avaliada em etapa futura com teste real de compilação.

3. **Sanitizers (ASan/UBSan):**
   - Permanece bloqueado pela ausência dos pacotes `libasan` e `libubsan` no sistema e ausência de `sudo` sem senha.

## 4. Procedimento Manual Pendente (Requer Privilégios / Intervenção do Usuário)

1. **Instalar toolchain ARM, runtime de sanitizers e ferramentas de debug no Fedora 44:**
   ```bash
   sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib libasan libubsan openocd
   ```
2. **Clonar o Pico SDK (>= 2.0.0) localmente:**
   ```bash
   git clone -b 2.1.0 --depth 1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
   git -C ~/pico-sdk submodule update --init
   export PICO_SDK_PATH=~/pico-sdk
   ```
3. **Compilar o firmware `harmonia_diag` para `pico2` (com Ninja):**
   ```bash
   cmake -S firmware/rp2350 -B firmware/rp2350/build -G Ninja -DPICO_BOARD=pico2
   cmake --build firmware/rp2350/build
   ```
   Verificar geração de `firmware/rp2350/build/harmonia_diag.elf` e `harmonia_diag.uf2`.
4. **Executar simulação Wokwi no VS Code:**
   - Abrir a pasta `hardware/simulation/wokwi/` no VS Code com a extensão **Wokwi Simulator**.
   - Iniciar simulação (`F1 -> Wokwi: Start Simulator`).
   - Validar: mensagem inicial no Serial Monitor, `HEARTBEAT #N` periódico, `KEY_TEST PRESSED` / `KEY_TEST RELEASED` ao clicar em `btn1`, continuidade e reinicialização limpa.
5. **Revalidar Sanitizers no host:**
   ```bash
   cmake -B build-san -DHARMONIA_ENABLE_SANITIZERS=ON -DHARMONIA_WARNINGS_AS_ERRORS=ON
   cmake --build build-san -j
   ctest --test-dir build-san --output-on-failure
   ```

## 5. Critérios de Aprovação (Status Real)
- [x] Inspeção de arquitetura, firmware e configuração Wokwi concluída.
- [x] Testes unitários do host (`harmonia_core`) aprovados (`6/6`).
- [x] Análise de compatibilidade C++17 vs. C++23 em `harmonia_core` verificada com compilador.
- [ ] Compilação do firmware para `pico2` e geração de `.elf`/`.uf2` (**Pendente** — requer `arm-none-eabi-gcc` e `pico-sdk`).
- [ ] Execução e validação da simulação no Wokwi (**Pendente** — requer firmware compilado e extensão Wokwi no VS Code).
- [ ] Execução de testes com ASan/UBSan no host (**Pendente** — requer `libasan` e `libubsan`).

## 6. Recomendações para a Etapa 03.1
- Antes ou no início da **Etapa 03.1**, ajustar `harmonia_core` para compatibilidade C++17 (ou validar C++20 no `arm-none-eabi-gcc` instalado) para que a lógica de debounce e varredura matricial possa ser compilada tanto no host (com Catch2) quanto no firmware RP2350 sem duplicação de código.
