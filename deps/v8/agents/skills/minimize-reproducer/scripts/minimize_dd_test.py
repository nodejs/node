#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for minimize_dd.py."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import minimize_dd

FAKE_D8 = """\
import sys

args = sys.argv[1:]
flags = [a for a in args if a.startswith("--")]
path = [a for a in args if not a.startswith("--")][-1]
with open(path, "r", encoding="utf-8") as f:
    src = f.read()

# Simulate a syntax error returning non-zero with a different message.
if "OPEN_PAREN(" in src and "CLOSE_PAREN)" not in src:
    print("SyntaxError: unexpected end of input")
    sys.exit(1)

if "--needed-flag" in flags and "TRIGGER_CRASH();" in src:
    print("Fatal error: DCHECK failed: target_invariant()")
    sys.exit(134)

sys.exit(0)
"""


class MinimizeDdTest(unittest.TestCase):

  def setUp(self):
    self.tmpdir = tempfile.TemporaryDirectory()
    self.fake_d8 = os.path.join(self.tmpdir.name, "fake_d8.py")
    with open(self.fake_d8, "w", encoding="utf-8") as f:
      f.write(FAKE_D8)

  def tearDown(self):
    self.tmpdir.cleanup()

  def test_build_command_substitutions(self):
    cmd = minimize_dd.build_command(
        "d8 {flags} --expose-gc {file}",
        ["--a", "--b"],
        "/tmp/repro.js",
    )
    self.assertEqual(cmd, ["d8", "--a", "--b", "--expose-gc", "/tmp/repro.js"])

    cmd_no_placeholder = minimize_dd.build_command("d8 --expose-gc", ["--a"],
                                                   "/tmp/repro.js")
    self.assertEqual(cmd_no_placeholder,
                     ["d8", "--expose-gc", "--a", "/tmp/repro.js"])

  def test_minimizes_flags_and_lines_with_signature_match(self):
    repro = os.path.join(self.tmpdir.name, "repro.js")
    out = os.path.join(self.tmpdir.name, "min.js")
    lines = [
        "// fuzzing boilerplate\n",
        "\n",
        "const noise1 = 1;\n",
        "const noise2 = 2;\n",
        "OPEN_PAREN(\n",
        "  extra_arg\n",
        "CLOSE_PAREN)\n",
        "const noise3 = 3;\n",
        "TRIGGER_CRASH();\n",
        "const noise4 = 4;\n",
    ]
    with open(repro, "w", encoding="utf-8") as f:
      f.writelines(lines)

    min_flags, min_lines, stats = minimize_dd.minimize_reproducer(
        cmd_template=f"{sys.executable} {self.fake_d8} {{flags}} {{file}}",
        input_path=repro,
        output_path=out,
        flags=["--noise-a", "--needed-flag", "--noise-b", "--fuzzing"],
        timeout=5.0,
        match_pattern=r"DCHECK failed: target_invariant",
    )

    self.assertEqual(min_flags, ["--needed-flag"])
    self.assertEqual(min_lines, ["TRIGGER_CRASH();\n"])
    with open(out, "r", encoding="utf-8") as f:
      self.assertEqual(f.read(), "TRIGGER_CRASH();\n")
    self.assertLess(stats.runs, 40)

  def test_baseline_failure_raises(self):
    repro = os.path.join(self.tmpdir.name, "clean.js")
    out = os.path.join(self.tmpdir.name, "min.js")
    with open(repro, "w", encoding="utf-8") as f:
      f.write("const x = 1;\n")

    with self.assertRaises(RuntimeError):
      minimize_dd.minimize_reproducer(
          cmd_template=f"{sys.executable} {self.fake_d8} {{flags}} {{file}}",
          input_path=repro,
          output_path=out,
          flags=["--needed-flag"],
          timeout=5.0,
          match_pattern=r"DCHECK failed",
      )

  def test_parse_single_flag_and_main_execution(self):
    ns_two_token = minimize_dd.parse_args([
        "--cmd", "d8", "--file", "a.js", "--out", "b.js", "--flags",
        "--expose-gc"
    ])
    self.assertEqual(ns_two_token.flags, "--expose-gc")

    ns_equals = minimize_dd.parse_args([
        "--cmd", "d8", "--file", "a.js", "--out", "b.js", "--flags=--expose-gc"
    ])
    self.assertEqual(ns_equals.flags, "--expose-gc")

    ns_empty = minimize_dd.parse_args(
        ["--cmd", "d8", "--file", "a.js", "--out", "b.js", "--flags", ""])
    self.assertEqual(ns_empty.flags, "")

    with self.assertRaises(SystemExit):
      minimize_dd.parse_args(
          ["--cmd", "d8", "--file", "a.js", "--out", "b.js", "--flags"])

    repro = os.path.join(self.tmpdir.name, "single_flag_repro.js")
    out = os.path.join(self.tmpdir.name, "single_flag_min.js")
    with open(repro, "w", encoding="utf-8") as f:
      f.write("const noise = 1;\nTRIGGER_CRASH();\n")

    rc = minimize_dd.main([
        "--cmd",
        f"{sys.executable} {self.fake_d8} {{flags}} {{file}}",
        "--flags",
        "--needed-flag",
        "--file",
        repro,
        "--out",
        out,
        "--timeout",
        "5.0",
    ])
    self.assertEqual(rc, 0)
    with open(out, "r", encoding="utf-8") as f:
      self.assertEqual(f.read(), "TRIGGER_CRASH();\n")


if __name__ == "__main__":
  unittest.main()
