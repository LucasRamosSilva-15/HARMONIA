# Relatório da Etapa 01 e Roteiro das Etapas Futuras

## Parte A — Roteiro das Etapas Futuras (01.6)

| Etapa | Nome | Dependências | Entregáveis Principais | Testes Planejados | Prioridade |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **02** | Infraestrutura C++ e Testes | Etapa 01 | `CMakeLists.txt` raiz, framework de testes (Catch2/GTest), CI/build host | Compilação C++ e teste unitário dummy passando | Alta |
| **03** | Aquisição Digital no RP2350 | Etapa 02 | Varredura matricial, debounce, cálculo de velocity (single-core) | Testes unitários no host + teste em bancada RP2350 | Alta |
| **04** | Aquisição Analógica e Filtragem | Etapa 03 | Leitura ADC (pedais, pots), média móvel/histerese | Testes unitários de filtro + leitura ADC real | Média |
| **05** | Eventos Musicais e Codificação UMP | Etapa 02 | Codificador/decodificador UMP (Note On/Off, CC, Pitch Bend) | Golden vectors UMP + testes unitários | Alta |
| **06** | USB MIDI 2.0 | Etapas 03, 05 | Descritores USB MIDI 2.0 + TinyUSB `CFG_TUD_MIDI2` | Enumeração real em Linux (`amidi`/`aconnect`) | Alta |
| **07** | Comunicação Entre Processadores | Etapas 02, 05 | Protocolo SPI/UART com framing e CRC + driver RP2350/RPi 5 | Loopback, taxa de erro, latência medida | Alta |
| **08** | Fundação Linux e Qt 6 | Etapa 02 | Imagem RPi OS Lite, Qt 6 Quick (EGLFS), serviço `systemd` | Boot direto na aplicação | Alta |
| **09** | Motor de Áudio e Primeira Nota | Etapas 02, 08 | Wrapper FluidSynth + fila lock-free + saída ALSA/I²S | Primeira nota audível sem XRUNs | Alta |
| **10** | Interface QML e Teclado Virtual | Etapas 08, 09 | Tela principal + teclado virtual touchscreen | Teste de toque e disparo de notas | Alta |
| **11** | Instrumentos e SoundFonts | Etapas 09, 10 | Biblioteca de instrumentos e troca segura de presets | Troca de SoundFont sem travar áudio | Média |
| **12** | Efeitos e Configurações | Etapa 11 | Reverb, chorus, mixer, configurações de áudio/MIDI | Testes de parâmetros em tempo real | Média |
| **13** | Integração Física Completa | Etapas 04, 07, 12 | Montagem física RP2350 + RPi 5 + DAC + Tela + Teclas | Teste ponta a ponta físico | Alta |
| **14** | Otimização e Confiabilidade | Etapa 13 | Profiling CPU/RAM, watchdog, recuperação de falhas | Teste de estresse (polifonia + longa duração) | Média |
| **15** | Validação Acadêmica e Publicação | Etapa 14 | Benchmarks reais, documentação final, artigo/TCC | Evidências reproduzíveis | Média |

**Riscos Transversais:**
- Disponibilidade de hardware (teclado com duplo contato para velocity, DAC I²S, tela DSI).
- Indisponibilidade temporária de `arm-none-eabi-gcc` e `pico-sdk` no ambiente atual (a resolver antes da Etapa 03).

---

## Parte B — Relatório Final da Etapa 01 (01.7)

1. **O que foi analisado:**
   - Viabilidade da arquitetura dual-processor (RP2350 + Raspberry Pi 5), eletrônica (níveis 3.3 V, matriz com diodos), alimentação (5 V/5 A), orçamento de latência (< 10 ms alvo), sistema operacional (RPi OS Lite + EGLFS) e separação entre desenvolvimento no host vs. dependente de hardware.
   - Dependências principais: `pico-sdk` (>= 2.0.0), `tusb_ump` vs. TinyUSB oficial (`CFG_TUD_MIDI2`), Qt 6 Quick, FluidSynth (LGPL-2.1+) e ferramentas já presentes no host.
2. **O que foi criado:**
   - `.gitignore`
   - `README.md`
   - `docs/research/FEASIBILITY.md`
   - `docs/research/DEPENDENCIES.md`
   - `docs/architecture/OVERVIEW.md`
   - `docs/architecture/DECISIONS.md`
   - `docs/stages/ETAPA_01_REPORT.md`
3. **O que foi verificado:**
   - Diretório `/home/lucasramos/Documentos/HARMONIA` inicialmente vazio.
   - Ferramentas no host: `git 2.56.0`, `cmake 4.3.0`, `g++ 16.2.1`, `python 3.14.8`, `qmake6`/`qtpaths6` e `fluidsynth` disponíveis; `arm-none-eabi-gcc` e `PICO_SDK_PATH` ausentes no momento.
4. **O que permanece incerto:**
   - Escolha final entre SPI e UART para o enlace interno (será decidida com testes na Etapa 07).
   - Modelo exato do teclado físico (se terá duplo contato para velocity) e do DAC I²S / tela touchscreen.
5. **Decisões que precisam ser tomadas pelo usuário antes/durante as próximas etapas:**
   - Preferência de framework de testes unitários C++ na Etapa 02 (**Catch2 v3** ou **GoogleTest**).
   - Confirmação do hardware-alvo de teclas (quantas teclas, se possui duplo contato).
6. **Resultado da Etapa 01:**
   - **Concluída** conforme critérios de aceitação (documentação fundacional, requisitos, ADRs, análise de viabilidade e roteiro criados sem código prematuro).
7. **Proposta da Etapa 02:**
   - Configurar a estrutura inicial de build C++ com `CMakeLists.txt` no host, integrar o framework de testes unitários e validar a compilação e execução de um teste base reproduzível.
