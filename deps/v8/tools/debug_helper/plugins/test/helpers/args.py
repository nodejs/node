# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assertions for `v8 args` and frame-relative `v8 inspect` integration tests."""

import re

from .backtrace import check_backtrace

# The innermost annotated frame: test_func_3(44, true, {inner: {...}}, "hello!").
_FUNC_FRAME_RE = re.compile(
    r"#(?P<frame>\d+)[^\n]*\[test_func_3 @ [^\]]+:15:21\]")

# Expected `v8 args` lines for the test_func_3 frame, one regex per line.
# Without static roots (no pointer compression) or space hints the library
# can only narrow known objects down to candidates, so `arg1` accepts both
# the exact brief and a `maybe ..., maybe TrueValue` candidate list.
_EXPECTED_ARG_LINE_RES = (
    r"this = 0x[0-9a-f]+ <JSGlobalProxy[^\n]*>",
    r"\[0\] = <Smi: 44>",
    r"\[1\] = 0x[0-9a-f]+ <Oddball:[^\n>]*TrueValue>",
    r"\[2\] = 0x[0-9a-f]+ <JSObject[^\n]*>",
    r"\[3\] = 0x[0-9a-f]+ <[^\n]*\"hello!\"[^\n]*>",
)


def _find_frame(bt, frame_re):
  """Return the frame number of the innermost frame annotated per `frame_re`."""
  m = frame_re.search(bt)
  if not m:
    raise AssertionError(
        f"no frame matching {frame_re.pattern} in backtrace:\n{bt}")
  return int(m.group("frame"))


def _find_func_frame(session):
  """Return the frame number of the annotated test_func_3 frame."""
  return _find_frame(session.run_command("bt"), _FUNC_FRAME_RE)


def _assert_matches(pattern, output):
  if not re.search(pattern, output):
    raise AssertionError(f"expected /{pattern}/ in output:\n{output}")


def check_args(session):
  """Check `v8 args` output, flags, and error handling."""
  frame = _find_func_frame(session)

  # Explicit frame number.
  output = session.run_command(f"v8 args {frame}")
  _assert_matches(rf"#{frame}  test_func_3 @ [^\n]+:15:21", output)
  for pattern in _EXPECTED_ARG_LINE_RES:
    _assert_matches(pattern, output)

  # Plain `v8 args` uses the debugger's selected frame.
  try:
    session.select_frame(frame)
    output = session.run_command("v8 args")
  finally:
    session.select_frame(0)
  for pattern in _EXPECTED_ARG_LINE_RES:
    _assert_matches(pattern, output)

  # Argument validation and non-JS frame handling.
  checks = (
      ("v8 args 9999", "cannot resolve frame 9999"),
      ("v8 args abc", "frame number must be a non-negative integer"),
      ("v8 args --frame 1", "unknown flag '--frame'"),
      # Frame 0 is the native abort frame, not a JS frame.
      ("v8 args 0", "is not a JS frame"),
  )
  for command, expected_error in checks:
    output = session.run_command(command)
    if expected_error not in output:
      raise AssertionError(
          f"`{command}` did not report {expected_error!r}, got:\n{output}")


# Expected annotations for the args-mismatch.js fixture. `under(a, b, c, d)`
# is called with one argument and `over(a)` with three. The trailer lists the
# call-site arguments. Padded formals of an under-applied frame are not
# listed, and all extra arguments of an over-applied frame are.
_MISMATCH_EXPECTED_ANNOTATIONS = (
    "[under @ <base>/args-mismatch.js:5:15] (this=<addr>, "
    '[0]=<addr> <SeqOneByteString: "only-one">)',
    "[over @ <base>/args-mismatch.js:9:14] (this=<addr>, [0]=<Smi: 1>, "
    "[1]=<Smi: 2>, [2]=<Smi: 3>)",
    "[<anonymous> @ <base>/args-mismatch.js:1:1] (this=<addr>, "
    "[0]=<addr> <FixedArray>)",
)

_UNDER_FRAME_RE = re.compile(r"#(?P<frame>\d+)[^\n]*\[under @ [^\]]+:5:15\]")
_OVER_FRAME_RE = re.compile(r"#(?P<frame>\d+)[^\n]*\[over @ [^\]]+:9:14\]")


def check_args_mismatch(session):
  """Check argument-count mismatches against the args-mismatch.js fixture."""
  bt = session.run_command("bt")
  failure = check_backtrace(bt, "args-mismatch", _MISMATCH_EXPECTED_ANNOTATIONS)
  if failure is not None:
    raise AssertionError(failure)

  # under(a, b, c, d) is called with one argument. Only [0] is listed, not
  # the undefined-padded slots of the remaining formals.
  output = session.run_command(f"v8 args {_find_frame(bt, _UNDER_FRAME_RE)}")
  _assert_matches(r"\[0\] = 0x[0-9a-f]+ <[^\n]*\"only-one\"[^\n]*>", output)
  if "[1]" in output:
    raise AssertionError(
        f"under-applied frame listed more than the passed argument:\n{output}")

  # over(a) is called with three arguments. The extras beyond the single
  # formal are all on the stack and listed.
  output = session.run_command(f"v8 args {_find_frame(bt, _OVER_FRAME_RE)}")
  for pattern in (r"\[0\] = <Smi: 1>", r"\[1\] = <Smi: 2>",
                  r"\[2\] = <Smi: 3>"):
    _assert_matches(pattern, output)
  if "[3]" in output:
    raise AssertionError(
        f"over-applied frame listed too many arguments:\n{output}")

  # Frame-relative names resolve against the call-site count too.
  try:
    session.select_frame(_find_frame(bt, _UNDER_FRAME_RE))
    output = session.run_command("v8 inspect [1]")
  finally:
    session.select_frame(0)
  if "has only 1 argument(s)" not in output:
    raise AssertionError(
        f"`v8 inspect [1]` on the under-applied frame did not report the "
        f"argument count, got:\n{output}")


def check_frame_relative_inspect(session):
  """Check `v8 inspect this|[N]` resolves against the selected frame."""
  frame = _find_func_frame(session)
  try:
    session.select_frame(frame)
    this_out = session.run_command("v8 inspect this")
    slot3_out = session.run_command("v8 inspect [3] --depth 0")
    slot9_out = session.run_command("v8 inspect [9]")
  finally:
    session.select_frame(0)
  _assert_matches(r"0x[0-9a-f]+ <JSGlobalProxy[^\n]*>", this_out)
  _assert_matches(r"0x[0-9a-f]+ <[^\n]*\"hello!\"[^\n]*>", slot3_out)
  if "has only 4 argument(s)" not in slot9_out:
    raise AssertionError(f"`v8 inspect [9]` did not report the argument "
                         f"count, got:\n{slot9_out}")

  # Outside a JS frame the slot resolution reports a clear error.
  output = session.run_command("v8 inspect [0]")
  if "is not a JS frame" not in output:
    raise AssertionError(
        f"`v8 inspect [0]` on a non-JS frame did not report an error, "
        f"got:\n{output}")
