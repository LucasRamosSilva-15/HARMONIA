#!/usr/bin/env python3
"""Testes unitarios para o analisador scripts/verify_wokwi_log.py."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_wokwi_log as vwl  # noqa: E402


def build_valid_log() -> list[str]:
    lines = [
        "[BOOT] HARMONIA RP2350 - Etapa 03.2 (Matriz 2x2)",
        "[GPIO] Rows: GP10, GP11 (Hi-Z / active-low) | Cols: GP14, GP15 (pull-up)",
        "[SCAN] Interval: 1000 us | Debounce: 20000 us | Notes: 60..63 | Vel: 40000",
        "[HEARTBEAT] [2000334 us] #1",
    ]
    ts = 2100000
    for phase in vwl.EXPECTED_PHASES:
        for ev_type, ch, note, vel in phase.expected_events:
            tag = "[NOTE_ON] " if ev_type == "NOTE_ON" else "[NOTE_OFF]"
            lines.append(f"{tag} [{ts} us] ch={ch} note={note} vel={vel}")
            ts += 1000
    return lines


class VerifyWokwiLogTests(unittest.TestCase):
    def test_valid_complete_log_passes(self) -> None:
        res = vwl.analyze_lines(build_valid_log())
        self.assertTrue(res.passed, msg=f"Errors: {res.errors}")
        self.assertEqual(res.phases_completed, len(vwl.EXPECTED_PHASES))
        self.assertEqual(res.ghost_violations, [])
        self.assertEqual(res.stuck_notes, [])

    def test_empty_log_fails(self) -> None:
        res = vwl.analyze_lines([])
        self.assertFalse(res.passed)

    def test_incomplete_log_fails(self) -> None:
        lines = build_valid_log()[:10]
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertLess(res.phases_completed, len(vwl.EXPECTED_PHASES))

    def test_ghosting_note_63_during_3_vertices_is_detected(self) -> None:
        lines = build_valid_log()
        # Localiza o segundo bloco com note=62 vel=40000 (que pertence a D_3VERTICES_ON)
        matches = [i for i, ln in enumerate(lines) if "note=62 vel=40000" in ln]
        self.assertGreaterEqual(len(matches), 3)
        idx = matches[2]
        # Injeta NOTE_ON note=63 indevido ANTES de note=62 completar D_3VERTICES_ON
        lines.insert(idx, "[NOTE_ON]  [2500000 us] ch=0 note=63 vel=40000")
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertTrue(len(res.ghost_violations) >= 1)

    def test_stuck_note_fails(self) -> None:
        lines = build_valid_log()
        # Remove o ultimo NOTE_OFF (note=60)
        lines.pop()
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertEqual(res.stuck_notes, [60])

    def test_duplicate_note_on_fails(self) -> None:
        lines = build_valid_log()
        idx = next(i for i, ln in enumerate(lines) if "note=60 vel=40000" in ln)
        lines.insert(idx + 1, "[NOTE_ON]  [2100500 us] ch=0 note=60 vel=40000")
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertEqual(len(res.duplicates), 1)

    def test_orphan_note_off_fails(self) -> None:
        lines = build_valid_log()
        lines.insert(4, "[NOTE_OFF] [2050000 us] ch=0 note=61 vel=0")
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertEqual(len(res.orphan_offs), 1)

    def test_wrong_channel_fails(self) -> None:
        lines = build_valid_log()
        for i, ln in enumerate(lines):
            if "ch=0 note=60 vel=40000" in ln:
                lines[i] = ln.replace("ch=0", "ch=1")
                break
        res = vwl.analyze_lines(lines)
        self.assertFalse(res.passed)
        self.assertTrue(len(res.unexpected_events) >= 1)

    def test_reordered_chord_note_off_passes(self) -> None:
        lines = build_valid_log()
        # Troca a ordem de note=61 e note=62 no D2_4KEYS_OFF (exatamente como ocorreu no Wokwi)
        idx_61 = max(i for i, ln in enumerate(lines) if "note=61 vel=0" in ln)
        idx_62 = max(i for i, ln in enumerate(lines) if "note=62 vel=0" in ln)
        lines[idx_61], lines[idx_62] = lines[idx_62], lines[idx_61]
        res = vwl.analyze_lines(lines)
        self.assertTrue(res.passed, msg=f"Errors: {res.errors}")
        self.assertEqual(res.phases_completed, len(vwl.EXPECTED_PHASES))


if __name__ == "__main__":
    unittest.main()
