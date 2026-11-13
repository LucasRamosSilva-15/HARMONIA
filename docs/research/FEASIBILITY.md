# Análise de Viabilidade Técnica — Etapa 01.1

## 1. Visão Geral da Arquitetura Dual-Processor (RP2350 + Raspberry Pi 5)

A separação entre **aquisição física determinística** (RP2350) e **síntese sonora + interface gráfica** (Raspberry Pi 5) resolve um problema clássico de instrumentos digitais baseados em Linux: o sistema operacional de propósito geral (mesmo com kernel preemptivo) não garante determinismo de sub-milissegundo para varredura matricial de dezenas de teclas e leitura de sensores sem risco de jitter ou perda de bordas.

- **RP2350 (Cortex-M33 @ 150 MHz, 520 KB SRAM):** Excelente custo-benefício para varredura matricial, debounce, medição de tempo entre contatos (velocity) e geração de pacotes UMP com latência previsível.
- **Raspberry Pi 5 (BCM2712 Quad-core Cortex-A76 @ 2.4 GHz, 4 GB ou 8 GB RAM):** Capacidade de sobra para executar FluidSynth com polifonia elevada (64+ vozes) e interface Qt 6 Quick/QML a 60 FPS simultaneamente, além de suporte nativo a PCIe/NVMe (boot rápido) e I²S para DAC externo.

## 2. Análise por Domínio

### 2.1 Eletrônica e Aquisição Física
- **Matriz de teclas:** Para 61 teclas (ou 88), uma matriz com diodos anti-ghosting (1N4148) evita teclas fantasmas quando múltiplos acordes são pressionados.
- **Sensibilidade à velocidade (Velocity):** Requer dois contatos por tecla (make/break) para medir o intervalo $\Delta t$ entre o acionamento do primeiro e do segundo contato. Sem hardware de teclado com duplo contato, o sistema deve operar com velocity fixa configurável ou simulada apenas para testes.
- **Níveis lógicos:** Ambos RP2350 e Raspberry Pi 5 operam em **3.3 V** nos GPIOs. Isso simplifica a interconexão direta (UART/SPI) sem conversores de nível, desde que compartilhem o mesmo plano de terra (GND) e tenham proteção contra ESD/ruído.

### 2.2 Alimentação
- **Raspberry Pi 5:** Exige fonte estável de **5 V / 5 A** (USB-C PD) para operar sem throttling sob carga de áudio + GPU.
- **RP2350:** Pode ser alimentado via regulador 3.3 V ou 5 V (VSYS), consumindo tipicamente < 100 mA.
- **Áudio (DAC + Amplificador):** Amplificadores de potência (ex.: alto-falantes internos) devem ter estágio de alimentação isolado ou bem filtrado para evitar ruído digital (ground loop / switching noise) no DAC I²S.

### 2.3 Latência e Áudio
- **Orçamento de latência alvo (ponta a ponta):** < 10 ms (idealmente 4–8 ms) entre o pressionamento físico da tecla e o início do som no DAC.
  - Varredura RP2350: ~0.5–1.0 ms (taxa >= 1 kHz).
  - Enlace RP2350 -> RPi 5: < 0.2 ms (SPI/UART em alta taxa).
  - Buffer ALSA/FluidSynth: 64 a 128 frames @ 48 kHz = 1.33 ms a 2.67 ms por período (2 períodos = 2.67–5.33 ms).
- **Risco:** Underruns de áudio (XRUNs) se a thread de áudio competir com I/O de disco (carregamento de SoundFont) ou renderização QML pesada. Mitigação: prioridade `SCHED_FIFO` na thread de áudio e carregamento de SoundFonts fora da thread de áudio.

### 2.4 Sistema Operacional e Interface Gráfica
- **Raspberry Pi OS Lite (64-bit):** Base enxuta sem X11/Wayland desktop completo.
- **EGLFS vs Wayland:** `EGLFS` permite que a aplicação Qt 6 Quick renderize diretamente sobre DRM/KMS sem compositor intermediário, reduzindo overhead de memória e latência gráfica. Wayland (ex.: Cage/Weston) é alternativa caso seja necessário exibir múltiplas janelas ou utilitários externos.

### 2.5 Custos Estimados (Ordem de Grandeza)
- Raspberry Pi 5 (4 GB): ~USD 60
- Raspberry Pi Pico 2 (RP2350): ~USD 5
- Tela Touchscreen DSI/HDMI (5"–7"): ~USD 40–70
- DAC I²S (ex.: PCM5102A / HiFiBerry): ~USD 10–35
- Fonte 5V 5A + componentes (diodos, cabos, protoboard): ~USD 25–40
- *Nota:* A migração para Compute Module 5 (CM5) só será avaliada após validação do protótipo em RPi 5.

## 3. O que Pode Ser Desenvolvido Sem Hardware vs. Com Hardware

| Componente | Sem Hardware (Host Linux x86_64) | Depende de Hardware Físico |
| :--- | :--- | :--- |
| Lógica de varredura, debounce e velocity | Sim (testes unitários em C++ com matriz simulada) | Validação elétrica, ruído de contato e timing real |
| Codificação/decodificação UMP (MIDI 2.0) | Sim (testes unitários e golden vectors) | Enumeração USB MIDI 2.0 real em host/device |
| Protocolo de comunicação interna (framing/CRC) | Sim (testes em memória / PTY / loopback) | Integridade elétrica em SPI/UART sob carga |
| Motor de áudio (FluidSynth + thread-safety) | Sim (compilação e execução no Linux host) | Medição real de XRUNs e latência no RPi 5 + DAC I²S |
| Interface Qt 6 Quick / QML | Sim (desenvolvimento e testes no host) | Responsividade ao toque na tela 5"–7" via EGLFS |
