#!/usr/bin/env python3
"""Verificador independente dos pacotes UMP hexadecimais no log serial do Wokwi."""

from dataclasses import dataclass
from pathlib import Path
import re
import sys
from typing import Iterable, List, Optional, Set, Tuple

UMP_RE = re.compile(
    r"\[UMP\]\s+mt=(\d+)\s+group=(\d+)\s+words=(\d+)\s+data=([0-9A-Fa-f]{8})(?:\s+([0-9A-Fa-f]{8}))?"
)


@dataclass(frozen=True)
class DecodedUmp:
    mt: int
    group: int
    status: int
    channel: int
    index: int
    value: int
    words: Tuple[int, ...]


@dataclass
class UmpAnalysisResult:
    passed: bool
    packet_count: int
    note_events: List[Tuple[str, int, int, int]]  # (ON/OFF, ch, note, vel16)
    cc_values: dict
    stuck_notes: List[int]
    errors: List[str]


def decode_ump_line(line: str) -> Optional[DecodedUmp]:
    m = UMP_RE.search(line)
    if not m:
        return None
    mt = int(m.group(1))
    group = int(m.group(2))
    word_count = int(m.group(3))
    w0 = int(m.group(4), 16)
    w1_str = m.group(5)

    if word_count == 2:
        if w1_str is None:
            raise ValueError(f"Pacote UMP truncado (esperava 2 palavras): {line}")
        words = (w0, int(w1_str, 16))
    elif word_count == 1:
        words = (w0,)
    else:
        raise ValueError(f"Contagem de palavras nao suportada ({word_count}): {line}")

    w0_mt = (w0 >> 28) & 0xF
    w0_grp = (w0 >> 24) & 0xF
    status = (w0 >> 20) & 0xF
    channel = (w0 >> 16) & 0xF
    index = (w0 >> 8) & 0xFF
    byte3 = w0 & 0xFF

    if w0_mt != mt or w0_grp != group:
        raise ValueError(f"Cabecalho UMP inconsistente com log: {line}")
    if index > 127:
        raise ValueError(f"Campo de 7 bits invalido (index={index}): {line}")

    if mt == 4:
        if word_count != 2:
            raise ValueError(f"MIDI 2.0 UMP deve ter 2 palavras: {line}")
        w1 = words[1]
        if status in (0x8, 0x9):
            attr_data = w1 & 0xFFFF
            if byte3 != 0 or attr_data != 0:
                raise ValueError(f"Campos reservados invalidos em Note MIDI 2.0: {line}")
            value = (w1 >> 16) & 0xFFFF
        elif status == 0xB:
            if byte3 != 0:
                raise ValueError(f"Campo reservado invalido em CC MIDI 2.0: {line}")
            value = w1
        else:
            raise ValueError(f"Status nao suportado (0x{status:X}): {line}")
    elif mt == 2:
        if word_count != 1 or byte3 > 127:
            raise ValueError(f"MIDI 1.0 UMP invalido: {line}")
        value = byte3
    else:
        raise ValueError(f"MT UMP nao suportado ({mt}): {line}")

    return DecodedUmp(mt=mt, group=group, status=status, channel=channel, index=index, value=value, words=words)


def analyze_ump_lines(lines: Iterable[str]) -> UmpAnalysisResult:
    packets: List[DecodedUmp] = []
    errors: List[str] = []
    note_events: List[Tuple[str, int, int, int]] = []
    cc_values: dict[int, List[int]] = {}
    active_notes: Set[int] = set()

    for raw in lines:
        ln = raw.strip()
        if "[UMP]" not in ln:
            continue
        try:
            pkt = decode_ump_line(ln)
        except ValueError as exc:
            errors.append(str(exc))
            continue
        if pkt is None:
            errors.append(f"Linha [UMP] malformada: {ln}")
            continue

        packets.append(pkt)
        if pkt.status == 0x9 and (pkt.mt == 4 or pkt.value > 0):
            note_events.append(("ON", pkt.channel, pkt.index, pkt.value))
            if pkt.index in active_notes:
                errors.append(f"NoteOn duplicado para nota {pkt.index}")
            active_notes.add(pkt.index)
        elif pkt.status == 0x8 or (pkt.mt == 2 and pkt.status == 0x9 and pkt.value == 0):
            note_events.append(("OFF", pkt.channel, pkt.index, pkt.value))
            if pkt.index not in active_notes:
                errors.append(f"NoteOff orfao para nota {pkt.index}")
            active_notes.discard(pkt.index)
        elif pkt.status == 0xB:
            cc_values.setdefault(pkt.index, []).append(pkt.value)

    if not packets:
        errors.append("Nenhum pacote [UMP] encontrado no log.")

    # Verifica K1 individual + acorde K1+K2+K3 (sem nota 63)
    expected_sequence = [
        {("ON", 0, 60, 40000)},
        {("OFF", 0, 60, 0)},
        {("ON", 0, 60, 40000), ("ON", 0, 61, 40000), ("ON", 0, 62, 40000)},
        {("OFF", 0, 60, 0), ("OFF", 0, 61, 0), ("OFF", 0, 62, 0)},
    ]
    idx = 0
    for phase_set in expected_sequence:
        rem = set(phase_set)
        while rem and idx < len(note_events):
            ev = note_events[idx]
            idx += 1
            if ev not in rem:
                errors.append(f"Evento de nota UMP inesperado: {ev} (esperado {sorted(rem)})")
                break
            rem.remove(ev)
        if rem:
            errors.append(f"Faltaram eventos de nota UMP na fase: {sorted(rem)}")

    if idx < len(note_events):
        errors.append(f"Eventos de nota UMP extras apos o cenario: {note_events[idx:]}")

    # Verifica CC7 e CC11 minimo (0) e maximo (0xFFFFFFFF)
    for cc in (7, 11):
        vals = cc_values.get(cc, [])
        if 0 not in vals:
            errors.append(f"CC{cc} nao registrou valor minimo 0x00000000.")
        if 0xFFFFFFFF not in vals:
            errors.append(f"CC{cc} nao registrou valor maximo 0xFFFFFFFF.")

    if active_notes:
        errors.append(f"Notas presas ao final: {sorted(active_notes)}")

    return UmpAnalysisResult(
        passed=len(errors) == 0,
        packet_count=len(packets),
        note_events=note_events,
        cc_values=cc_values,
        stuck_notes=sorted(active_notes),
        errors=errors,
    )


def main(argv: Optional[List[str]] = None) -> int:
    args = argv if argv is not None else sys.argv[1:]
    if len(args) != 1:
        print("Uso: verify_ump_log.py <caminho_para_serial.log>")
        return 2

    path = Path(args[0])
    if not path.exists():
        print(f"[FAIL] Arquivo nao encontrado: {path}")
        return 1

    res = analyze_ump_lines(path.read_text(encoding="utf-8", errors="replace").splitlines())
    print(f"UMP Packets={res.packet_count}, NoteEvents={len(res.note_events)}, CCs={sorted(res.cc_values.keys())}")
    if res.passed:
        print("[PASS] Pacotes UMP MIDI 2.0 validados com sucesso (K1, acorde K1+K2+K3, CC7/CC11 min/max).")
        return 0
    for err in res.errors:
        print(f"[FAIL] {err}")
    return 1


if __name__ == "__main__":
    sys.exit(main())