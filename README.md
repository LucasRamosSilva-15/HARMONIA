# HARMONIA — Teclado Musical Digital Autônomo (Open Source)

Projeto de um **teclado musical digital autônomo, modular e open source**, desenvolvido em etapas incrementais e verificáveis, sem dependência de computador externo, conexão com a internet ou software musical de terceiros.

## Arquitetura-Alvo

- **Processador 1 — Raspberry Pi Pico 2 (RP2350, C++ / Pico SDK):**
  - Varredura matricial determinística de teclas (com diodos anti-ghosting e debounce).
  - Leitura de velocidade (quando suportado pelo mecanismo) e entradas analógicas (pedais, potenciômetros, encoders).
  - Geração de eventos musicais em formato UMP (Universal MIDI Packet).
  - Comunicação interna dedicada com o processador principal e interface USB MIDI 2.0 (com fallback MIDI 1.0).
- **Processador 2 — Raspberry Pi 5 (Linux Embarcado, C++ / Qt 6 Quick & QML / FluidSynth):**
  - Inicialização direta na aplicação musical (sem ambiente desktop completo).
  - Motor de síntese polifônica baseado em SoundFont (FluidSynth inicialmente) com thread de áudio isolada.
  - Interface gráfica touchscreen (5" a 7") em Qt 6 Quick / QML.
  - Saída de áudio via DAC I²S de baixa latência.

## Estrutura Inicial do Repositório

```text
HARMONIA/
├── .gitignore
├── README.md
├── firmware/
│   └── rp2350/              # Firmware de diagnóstico RP2350 (Pico SDK)
├── hardware/
│   └── simulation/
│       └── wokwi/           # Simulação Wokwi (diagram.json + wokwi.toml)
└── docs/
    ├── architecture/
    │   ├── OVERVIEW.md      # Requisitos e arquitetura do sistema
    │   └── DECISIONS.md     # Architecture Decision Records (ADRs)
    ├── research/
    │   ├── FEASIBILITY.md   # Análise de viabilidade técnica e física
    │   └── DEPENDENCIES.md  # Pesquisa de dependências e versões
    └── stages/
        ├── ETAPA_01_REPORT.md       # Roteiro de etapas e relatório da Etapa 01
        └── ETAPA_01_8_WOKWI_REPORT.md # Integração Wokwi + Pico SDK
```

## Compilação e Testes (Host C++23)

```bash
cmake -B build -DHARMONIA_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Para habilitar sanitizers (ASan + UBSan) no host (requer `libasan` e `libubsan` instalados):

```bash
cmake -B build-san -DHARMONIA_ENABLE_SANITIZERS=ON -DHARMONIA_WARNINGS_AS_ERRORS=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

## Status Atual

- **Etapa atual:** Etapa 03.0 — Validação do Ambiente Embarcado e Simulação Wokwi (Host e compatibilidade C++17 verificados; compilação `pico2`, Wokwi e sanitizers pendentes de pacotes via `sudo dnf install` e clone do `pico-sdk`).
- **Próxima etapa:** Etapa 03.1 — Aquisição Digital (Matriz e Debounce) após conclusão dos itens pendentes ou autorização explícita.
