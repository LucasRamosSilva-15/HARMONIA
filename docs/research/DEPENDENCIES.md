# Pesquisa de Dependências e Ambiente — Etapa 01.3

## 1. Inventário do Ambiente Local (Verificado)

Inspeção realizada no host de desenvolvimento em `2026-11-13`:

- **Git:** `2.56.0`
- **CMake:** `4.3.0`
- **Compilador C++ (Host):** `g++ (GCC) 16.2.1`
- **Python:** `3.14.8`
- **Qt 6:** `qmake6` e `qtpaths6` presentes em `/usr/lib64/qt6/bin/`
- **FluidSynth:** `/usr/bin/fluidsynth` presente no sistema
- **Toolchain ARM (`arm-none-eabi-gcc`) / `PICO_SDK_PATH`:** Não instalados no PATH atual (serão configurados na Etapa 02/03).

## 2. Análise das Dependências Principais

### 2.1 Pico SDK (RP2350)
- **Fonte:** `https://github.com/raspberrypi/pico-sdk`
- **Versão mínima necessária:** `>= 2.0.0` (primeira versão com suporte oficial ao RP2350 / Cortex-M33 e Hazard3).
- **Licença:** BSD-3-Clause.
- **Observação:** Suporta CMake nativamente e inclui `pico_stdlib`, `hardware_spi`, `hardware_uart`, `hardware_adc`, `hardware_pio` e integração com TinyUSB.

### 2.2 USB MIDI 2.0: `tusb_ump` vs. TinyUSB Oficial
- **`midi2-dev/tusb_ump`:** Projeto pioneiro (licença MIT) que implementou driver USB MIDI 2.0 com UMP e fallback MIDI 1.0 sobre TinyUSB.
- **`hathach/tinyusb` (Oficial):** O trabalho do `tusb_ump` foi **integrado ao TinyUSB oficial** como driver nativo de classe MIDI 2.0 (`CFG_TUD_MIDI2 = 1`).
  - Suporta Dual Alternate Settings (Alt 0 = MIDI 1.0 fallback; Alt 1 = MIDI 2.0 UMP nativo).
  - Gerencia Endpoint Discovery, Function Block Discovery e Group Terminal Blocks.
  - API principal: `tud_midi2_mounted()`, `tud_midi2_available()`, `tud_midi2_alt_setting()`, `tud_midi2_ump_read()`, `tud_midi2_ump_write()`.
  - Licença: MIT.
- **Recomendação Inicial:** Utilizar o **driver MIDI 2.0 nativo do TinyUSB oficial** (empacotado junto ao Pico SDK), evitando misturar forks externos (`tusb_ump`) a menos que uma lacuna específica seja encontrada nos testes de enumeração da Etapa 06.

### 2.3 Qt 6 Quick / QML
- **Módulos necessários:** `Qt6::Core`, `Qt6::Gui`, `Qt6::Qml`, `Qt6::Quick`, `Qt6::QuickControls2`.
- **Licença:** LGPL-3.0 / GPL-3.0 (compatível com projeto open source).
- **Backend alvo no RPi 5:** `EGLFS` (DRM/KMS) para inicialização direta sem desktop environment.

### 2.4 FluidSynth
- **Fonte:** `https://github.com/FluidSynth/fluidsynth`
- **Licença:** LGPL-2.1+.
- **Papel:** Motor de síntese inicial baseado em SoundFont (`.sf2` / `.sf3`), com suporte a polifonia, reverb, chorus, sustain e múltiplos canais.
- **Requisito arquitetural:** Toda chamada à API do FluidSynth (`fluid_synth_noteon`, `fluid_synth_cc`, etc.) deve ser encapsulada em um módulo C++ isolado, sem bloqueios na thread de áudio.

### 2.5 Bibliotecas de Testes (C++)
- **Candidato principal:** **Catch2** (v3) ou **GoogleTest (GTest)**.
  - *Catch2 v3* ou *GoogleTest* integram-se facilmente via CMake `FetchContent` ou pacote do sistema, sem dependências pesadas, permitindo testes unitários rápidos no host x86_64 para lógica pura (debounce, UMP, framing, ring buffers).
- **Decisão:** Será formalizada na Etapa 02 ao configurar o `CMakeLists.txt` de testes.

### 2.6 Sistema Operacional Embarcado (Raspberry Pi 5)
- **Candidato principal:** **Raspberry Pi OS Lite (64-bit, Debian-based)**.
- **Justificativa:** Suporte oficial ao kernel do RPi 5, drivers DRM/KMS (EGLFS), ALSA, `snd-usb-midi2` e pacotes Qt 6 / FluidSynth disponíveis via `apt` ou compilação cruzada/nativa.
