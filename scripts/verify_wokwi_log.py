#!/usr/bin/env python3
"""Analisador deterministico por fase do log serial gerado pelo Wokwi CLI."""

from dataclasses import dataclass
import re
import sys
from pathlib import Path
from typing import Iterable, List, Optional, Set, Tuple

EVENT_RE = re.compile(
    r"^\[(NOTE_ON|NOTE_OFF)\]\s+\[(\d+)\s+us\]\s+ch=(\d+)\s+note=(\d+)\s+vel=(\d+)$"
)

MidiTuple = Tuple[str, int, int, int]  # (type, ch, note, vel)


@dataclass(frozen=True)
class PhaseSpec:
    name: str
    expected_events: Tuple[MidiTuple, ...]
    forbidden_notes: Tuple[int, ...] = ()


def on(note: int, ch: int = 0, vel: int = 40000) -> MidiTuple:
    return ("NOTE_ON", ch, note, vel)


def off(note: int, ch: int = 0, vel: int = 0) -> MidiTuple:
    return ("NOTE_OFF", ch, note, vel)


EXPECTED_PHASES: Tuple[PhaseSpec, ...] = (
    # Teste B - Individuais
    PhaseSpec("B_K1_ON", (on(60),)),
    PhaseSpec("B_K1_OFF", (off(60),)),
    PhaseSpec("B_K2_ON", (on(61),)),
    PhaseSpec("B_K2_OFF", (off(61),)),
    PhaseSpec("B_K3_ON", (on(62),)),
    PhaseSpec("B_K3_OFF", (off(62),)),
    PhaseSpec("B_K4_ON", (on(63),)),
    PhaseSpec("B_K4_OFF", (off(63),)),
    # Teste C - Acordes 2 teclas
    PhaseSpec("C_K1_K2_ON", (on(60), on(61))),
    PhaseSpec("C_K1_K2_OFF", (off(60), off(61))),
    PhaseSpec("C_K1_K3_ON", (on(60), on(62))),
    PhaseSpec("C_K1_K3_OFF", (off(60), off(62))),
    PhaseSpec("C_K2_K4_ON", (on(61), on(63))),
    PhaseSpec("C_K2_K4_OFF", (off(61), off(63))),
    # Teste D - Anti-ghosting 3 vertices (K1+K2+K3; nota 63 proibida!)
    PhaseSpec("D_3VERTICES_ON", (on(60), on(61), on(62)), forbidden_notes=(63,)),
    PhaseSpec("D_3VERTICES_OFF", (off(60), off(61), off(62)), forbidden_notes=(63,)),
    # Teste D2 - 4 teclas simultaneas
    PhaseSpec("D2_4KEYS_ON", (on(60), on(61), on(62), on(63))),
    PhaseSpec("D2_4KEYS_OFF", (off(60), off(61), off(62), off(63))),
    # Teste E - Estabilidade (hold prolongado)
    PhaseSpec("E_HOLD_ON", (on(60),)),
    PhaseSpec("E_HOLD_OFF", (off(60),)),
)


@dataclass
class AnalysisResult:
    passed: bool
    has_boot: bool
    has_gpio: bool
    has_scan: bool
    heartbeats: int
    phases_completed: int
    total_phases: int
    warnings: List[str]
    duplicates: List[str]
    orphan_offs: List[str]
    ghost_violations: List[str]
    unexpected_events: List[str]
    stuck_notes: List[int]
    errors: List[str]


def analyze_lines(lines: Iterable[str]) -> AnalysisResult:
    raw_lines = [ln.strip() for ln in lines]

    has_boot = any("[BOOT]" in ln for ln in raw_lines)
    has_gpio = any("[GPIO]" in ln for ln in raw_lines)
    has_scan = any("[SCAN]" in ln for ln in raw_lines)
    heartbeats = sum(1 for ln in raw_lines if "[HEARTBEAT]" in ln)
    warnings = [ln for ln in raw_lines if "[WARN]" in ln or "[ERROR]" in ln]

    active_notes: Set[int] = set()
    duplicates: List[str] = []
    orphan_offs: List[str] = []
    ghost_violations: List[str] = []
    unexpected_events: List[str] = []
    errors: List[str] = []

    parsed_events: List[Tuple[MidiTuple, str]] = []
    for ln in raw_lines:
        m = EVENT_RE.match(ln)
        if not m:
            continue
        ev_type, _ts, ch_s, note_s, vel_s = m.groups()
        ev = (ev_type, int(ch_s), int(note_s), int(vel_s))
        parsed_events.append((ev, ln))

        note = ev[2]
        if ev_type == "NOTE_ON":
            if note in active_notes:
                duplicates.append(ln)
            active_notes.add(note)
        else:
            if note not in active_notes:
                orphan_offs.append(ln)
            active_notes.discard(note)

    # Verificacao deterministica por fase
    phase_idx = 0
    remaining: List[MidiTuple] = list(EXPECTED_PHASES[0].expected_events)

    for ev, ln in parsed_events:
        if phase_idx >= len(EXPECTED_PHASES):
            unexpected_events.append(f"apos fim do cenario: {ln}")
            continue

        phase = EXPECTED_PHASES[phase_idx]
        if ev[2] in phase.forbidden_notes:
            ghost_violations.append(f"[{phase.name}] nota fantasma {ev[2]}: {ln}")
            continue

        if ev in remaining:
            remaining.remove(ev)
            if not remaining:
                phase_idx += 1
                if phase_idx < len(EXPECTED_PHASES):
                    remaining = list(EXPECTED_PHASES[phase_idx].expected_events)
        else:
            unexpected_events.append(f"[{phase.name}] inesperado: {ln}")

    if not raw_lines:
        errors.append("Log serial vazio.")
    if not (has_boot and has_gpio and has_scan):
        errors.append("Mensagens obrigatorias de inicializacao ([BOOT]/[GPIO]/[SCAN]) ausentes.")
    if heartbeats < 1:
        errors.append("Nenhum [HEARTBEAT] observado.")
    if phase_idx < len(EXPECTED_PHASES):
        errors.append(
            f"Cenario incompleto: concluiu {phase_idx}/{len(EXPECTED_PHASES)} fases "
            f"(faltou em {EXPECTED_PHASES[phase_idx].name}: {remaining})."
        )
    if warnings:
        errors.append(f"Warnings/Erros no firmware: {warnings}")
    if duplicates:
        errors.append(f"NOTE_ON duplicados: {duplicates}")
    if orphan_offs:
        errors.append(f"NOTE_OFF orfaos: {orphan_offs}")
    if ghost_violations:
        errors.append(f"Violacoes de anti-ghosting: {ghost_violations}")
    if unexpected_events:
        errors.append(f"Eventos inesperados fora da fase: {unexpected_events}")
    if active_notes:
        errors.append(f"Notas presas ao final: {sorted(active_notes)}")

    return AnalysisResult(
        passed=len(errors) == 0,
        has_boot=has_boot,
        has_gpio=has_gpio,
        has_scan=has_scan,
        heartbeats=heartbeats,
        phases_completed=phase_idx,
        total_phases=len(EXPECTED_PHASES),
        warnings=warnings,
        duplicates=duplicates,
        orphan_offs=orphan_offs,
        ghost_violations=ghost_violations,
        unexpected_events=unexpected_events,
        stuck_notes=sorted(active_notes),
        errors=errors,
    )


def analyze_log(log_path: Path) -> AnalysisResult:
    if not log_path.exists():
        return AnalysisResult(
            passed=False,
            has_boot=False,
            has_gpio=False,
            has_scan=False,
            heartbeats=0,
            phases_completed=0,
            total_phases=len(EXPECTED_PHASES),
            warnings=[],
            duplicates=[],
            orphan_offs=[],
            ghost_violations=[],
            unexpected_events=[],
            stuck_notes=[],
            errors=[f"Arquivo de log nao encontrado: {log_path}"],
        )
    return analyze_lines(log_path.read_text(encoding="utf-8", errors="replace").splitlines())


def main(argv: Optional[List[str]] = None) -> int:
    args = argv if argv is not None else sys.argv[1:]
    if len(args) != 1:
        print("Uso: verify_wokwi_log.py <caminho_para_serial.log>")
        return 2

    res = analyze_log(Path(args[0]))
    print(
        f"Boot={res.has_boot}, GPIO={res.has_gpio}, Scan={res.has_scan}, "
        f"Heartbeats={res.heartbeats}, Fases={res.phases_completed}/{res.total_phases}"
    )
    if res.passed:
        print("[PASS] Todas as fases validadas sem ghosting, duplicatas, orfaos ou notas presas.")
        return 0

    for err in res.errors:
        print(f"[FAIL] {err}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
