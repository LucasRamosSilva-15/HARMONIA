#!/usr/bin/env python3
"""Testes unitarios para scripts/verify_ump_log.py."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_ump_log as vul  # noqa: E402


def build_valid_ump_log() -> list[str]:
    return [
        "[UMP] mt=4 group=0 words=2 data=40B00700 00000000",
        "[UMP] mt=4 group=0 words=2 data=40B00B00 00000000",
        "[UMP] mt=4 group=0 words=2 data=40903C00 9C400000",
        "[UMP] mt=4 group=0 words=2 data=40803C00 00000000",
        "[UMP] mt=4 group=0 words=2 data=40903E00 9C400000",
        "[UMP] mt=4 group=0 words=2 data=40903C00 9C400000",
        "[UMP] mt=4 group=0 words=2 data=40903D00 9C400000",
        "[UMP] mt=4 group=0 words=2 data=40803D00 00000000",
        "[UMP] mt=4 group=0 words=2 data=40803C00 00000000",
        "[UMP] mt=4 group=0 words=2 data=40803E00 00000000",
        "[UMP] mt=4 group=0 words=2 data=40B00700 FFFFFFFF",
        "[UMP] mt=4 group=0 words=2 data=40B00B00 FFFFFFFF",
    ]


class VerifyUmpLogTests(unittest.TestCase):
    def test_valid_ump_log_passes(self) -> None:
        res = vul.analyze_ump_lines(build_valid_ump_log())
        self.assertTrue(res.passed, msg=f"Errors: {res.errors}")
        self.assertEqual(res.packet_count, 12)

    def test_empty_log_fails(self) -> None:
        res = vul.analyze_ump_lines([])
        self.assertFalse(res.passed)

    def test_ghost_note_63_fails(self) -> None:
        lines = build_valid_ump_log()
        lines.insert(6, "[UMP] mt=4 group=0 words=2 data=40903F00 9C400000")
        res = vul.analyze_ump_lines(lines)
        self.assertFalse(res.passed)

    def test_reserved_byte_nonzero_fails(self) -> None:
        lines = build_valid_ump_log()
        lines[0] = "[UMP] mt=4 group=0 words=2 data=40B00701 00000000"
        res = vul.analyze_ump_lines(lines)
        self.assertFalse(res.passed)


if __name__ == "__main__":
    unittest.main()