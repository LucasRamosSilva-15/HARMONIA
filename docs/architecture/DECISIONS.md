# Architecture Decision Records (ADRs) — Etapa 01.4

## ADR-001: Arquitetura Dual-Processor (RP2350 + Raspberry Pi 5)
- **Status:** Aceito
- **Contexto:** Precisamos de varredura determinística de teclas (< 1 ms) e síntese polifônica + UI moderna em Qt 6.
- **Decisão:** Delegar aquisição física e USB MIDI 2.0 ao RP2350 (C++ bare-metal/Pico SDK) e síntese + UI ao Raspberry Pi 5 (Linux + C++ + QML).
- **Consequências:** Isola o determinismo elétrico do escalonamento do Linux, mas exige um protocolo de comunicação interna confiável entre as duas placas.

## ADR-002: Uso do Driver MIDI 2.0 Nativo do TinyUSB Oficial
- **Status:** Aceito
- **Contexto:** Avaliamos `midi2-dev/tusb_ump` vs. `hathach/tinyusb` oficial.
- **Decisão:** Adotar o driver MIDI 2.0 nativo (`CFG_TUD_MIDI2`) do TinyUSB oficial integrado ao Pico SDK.
- **Consequências:** Evita manter forks externos divergentes, mantém licença MIT e garante suporte a Dual Alternate Settings (MIDI 2.0 UMP + fallback MIDI 1.0).

## ADR-003: Comunicação Interna Independente de USB MIDI
- **Status:** Em investigação (Etapa 07)
- **Contexto:** O enlace RP2350 <-> RPi 5 não deve depender da pilha USB externa.
- **Decisão:** Avaliar SPI (alta velocidade, full-duplex, controle de clock pelo mestre) vs. UART (simplicidade, menos pinos) na Etapa 07, com framing explícito e CRC.

## ADR-004: Isolamento da Thread de Áudio (Lock-Free)
- **Status:** Aceito
- **Contexto:** Operações de QML, alocação de memória ou carregamento de SoundFonts causam glitches/XRUNs se executadas na thread de áudio.
- **Decisão:** A thread de áudio comunicará com o restante do sistema exclusivamente via filas lock-free / ring buffers, sem mutexes bloqueantes ou I/O.

## ADR-005: Motor de Síntese Inicial (FluidSynth) e Backend Gráfico (EGLFS)
- **Status:** Aceito
- **Contexto:** Construir um motor DSP próprio do zero atrasaria o primeiro som autônomo.
- **Decisão:** Usar FluidSynth (LGPL-2.1+) como motor inicial e Qt 6 Quick sobre EGLFS no Raspberry Pi OS Lite 64-bit.
