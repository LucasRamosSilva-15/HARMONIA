#!/usr/bin/env python3
"""Analisador deterministico do log serial gerado pelo Wokwi CLI (--serial-log-file)."""

import re
import sys
from pathlib import Path


EVENT_RE = re.compile(r"^\[(NOTE_ON|NOTE_OFF)\]\s+\[(\d+)\s+us\]\s+ch=(\d+)\s+note=(\d+)\s+vel=(\d+)")


def analyze_log(log_path: Path) -> int:
    if not log_path.exists():
        print(f"[FAIL] Log nao encontrado: {log_path}")
        return 1

    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()

    has_boot = any("[BOOT]" in line for line in lines)
    has_gpio = any("[GPIO]" in line for line in lines)
    has_scan = any("[SCAN]" in line for line in lines)
    heartbeats = sum(1 for line in lines if "[HEARTBEAT]" in line)
    warns = [line for line in lines if "[WARN]" in line or "[ERROR]" in line]

    active_notes: set[int] = set()
    duplicates: list[str] = []
    orphan_offs: list[str] = []
    ghost_violations: list[str] = []
    counts = {60: [0, 0], 61: [0, 0], 62: [0, 0], 63: [0, 0]}

    for line in lines:
        m = EVENT_RE.match(line.strip())
        if not m:
            continue
        ev_type, _ts, _ch, note_s, _vel = m.groups()
        note = int(note_s)

        if ev_type == "NOTE_ON":
            if note in active_notes:
                duplicates.append(line)
            active_notes.add(note)
            if note in counts:
                counts[note][0] += 1
            # Verifica se em algum instante 4 notas estavam ativas antes de K4 esperado
            # (validacao fina depende da sequencia do scenario.yaml)
        else:
            if note not in active_notes:
                orphan_offs.append(line)
            active_notes.discard(note)
            if note in counts:
                counts[note][1] += 1

    print(f"Boot={has_boot}, GPIO={has_gpio}, Scan={has_scan}, Heartbeats={heartbeats}")
    print(f"Contagem por nota (ON, OFF): {counts}")

    ok = True
    if not (has_boot and has_gpio and has_scan and heartbeats >= 1):
        print("[FAIL] Mensagens iniciais ou heartbeat ausentes.")
        ok = False
    if warns:
        print(f"[FAIL] Warnings/Erros no firmware: {warns}")
        ok = False
    if duplicates:
        print(f"[FAIL] NOTE_ON duplicados detectados: {duplicates}")
        ok = False
    if orphan_offs:
        print(f"[FAIL] NOTE_OFF orfaos detectados: {orphan_offs}")
        ok = False
    if active_notes:
        print(f"[FAIL] Notas presas ao final da simulacao: {sorted(active_notes)}")
        ok = False
    if ghost_violations:
        print(f"[FAIL] Ghosting detectado: {ghost_violations}")
        ok = False

    for n, (on_c, off_c) in counts.items():
        if on_c == 0 or on_c != off_c:
            print(f"[FAIL] Nota {n} inconsistente: ON={on_c}, OFF={off_c}")
            ok = False

    if ok:
        print("[PASS] Log serial validado: zero duplicatas, zero notas presas, pares ON/OFF consistentes.")
        return 0
    return 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Uso: {sys.argv[0]} <caminho_para_serial.log>")
        sys.exit(2)
    sys.exit(analyze_log(Path(sys.argv[1])))
