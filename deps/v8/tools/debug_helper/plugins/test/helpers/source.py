# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assertions for `v8 source` integration tests against the throw.js fixture."""

import re

_FUNC_FRAME_RE = re.compile(
    r"#(?P<frame>\d+)[^\n]*\[test_func_3 @ (?P<script>[^\]]+):15:21\]"
    r" \(this=(?P<this>0x[0-9a-f]+), argc=(?P<argc>\d+)\)")
_TOP_LEVEL_FRAME_RE = re.compile(
    r"#(?P<frame>\d+)[^\n]*\[<anonymous> @ (?P<script>[^\]]*throw\.js):1:1\]"
    r" \(this=(?P<this>0x[0-9a-f]+), argc=(?P<argc>\d+)\)")


def _match_frame(bt, frame_re):
  """Return the bt match for the innermost frame annotated per `frame_re`."""
  m = frame_re.search(bt)
  if not m:
    raise AssertionError(
        f"no frame matching {frame_re.pattern} in backtrace:\n{bt}")
  return m


def _header(m, function_name, position):
  """The `v8 source` first line, reproducing the frame's bt annotation."""
  return (f"#{m['frame']}  {function_name} @ {m['script']}:{position}"
          f" (this={m['this']}, argc={m['argc']})\n\n")


def _expected_func_span(m):
  """The `v8 source` snippet for the test_func_3 frame (throw.js 15-17)."""
  rule = f"+---- frame {m['frame']}: test_func_3 -----"
  return (_header(m, "test_func_3", "15:21") + "  14\n"
          f"     {rule}\n"
          "  15 | function test_func_3(n, b, o, s) {\n"
          '  16 |   throw new Error("v8dbg bridge test: " + s);\n'
          "  17 | }\n"
          f"     {rule}\n")


def _expected_max_lines_span(m):
  """The `v8 source --max-lines 2` snippet for the test_func_3 frame."""
  rule = f"+---- frame {m['frame']}: test_func_3 -----"
  return (_header(m, "test_func_3", "15:21") + "  14\n"
          f"     {rule}\n"
          "  15 | function test_func_3(n, b, o, s) {\n"
          '  16 |   throw new Error("v8dbg bridge test: " + s);\n'
          "     |  ... (1 more lines)\n")


def _expected_top_level_span(m):
  """The `v8 source` snippet for the top-level frame, truncated at 10 lines."""
  rule = f"+---- frame {m['frame']}: <anonymous> -----"
  return (
      _header(m, "<anonymous>", "1:1") + f"     {rule}\n"
      "   1 | // Copyright 2026 the V8 project authors. "
      "All rights reserved.\n"
      "   2 | // Use of this source code is governed by a BSD-style license "
      "that can be\n"
      "   3 | // found in the LICENSE file.\n"
      "   4 |\n"
      "   5 | function test_func_1(n, b, o, s) {\n"
      "   6 |   return test_func_2(n + 1, !b, o, s);\n"
      "   7 | }\n"
      "   8 |\n"
      "   9 | function test_func_2(n, b, o, s) {\n"
      "  10 |   return (function(n, b, o, s) {\n"
      "     |  ... (9 more lines)\n")


def _assert_in_output(expected, output):
  """Assert `expected` appears in the debugger output, modulo CRLF endings."""
  if expected not in output.replace("\r\n", "\n"):
    raise AssertionError(f"expected `v8 source` snippet:\n{expected}\n"
                         f"not found in output:\n{output}")


def check_source(session):
  """Check `v8 source` rendering, flags, and error handling end-to-end."""
  bt = session.run_command("bt")
  func = _match_frame(bt, _FUNC_FRAME_RE)
  top = _match_frame(bt, _TOP_LEVEL_FRAME_RE)
  expected = _expected_func_span(func)

  # Explicit frame number.
  _assert_in_output(expected, session.run_command(f"v8 source {func['frame']}"))

  # Plain `v8 source` uses the debugger's selected frame.
  try:
    session.select_frame(int(func["frame"]))
    output = session.run_command("v8 source")
  finally:
    session.select_frame(0)
  _assert_in_output(expected, output)

  # `--max-lines N` truncates the span after N lines, with no end rule.
  _assert_in_output(
      _expected_max_lines_span(func),
      session.run_command(f"v8 source {func['frame']} --max-lines 2"))

  # The top-level frame's span is the whole script, truncated at the cap.
  _assert_in_output(
      _expected_top_level_span(top),
      session.run_command(f"v8 source {top['frame']}"))

  # Argument validation and non-JS frame handling.
  checks = (
      ("v8 source 9999", "cannot resolve frame 9999"),
      ("v8 source --context 1", "unknown flag '--context'"),
      ("v8 source abc", "frame number must be a non-negative integer"),
      ("v8 source --max-lines 0", "--max-lines must be a positive integer"),
      ("v8 source --max-lines abc", "--max-lines must be a positive integer"),
      # Frame 0 is the native abort frame, not a JS frame.
      ("v8 source 0", "is not a JS frame"),
  )
  for command, expected_error in checks:
    output = session.run_command(command)
    if expected_error not in output:
      raise AssertionError(
          f"`{command}` did not report {expected_error!r}, got:\n{output}")
