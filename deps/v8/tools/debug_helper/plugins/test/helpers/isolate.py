# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assertions for `v8 isolate` integration tests."""

import re

_ISOLATE_LINE_RE = re.compile(r"^isolate = 0x([0-9a-f]+)\s*$", re.MULTILINE)

_NONE_LINE_RE = re.compile(r"^isolate = <none>\s*$", re.MULTILINE)

# Use `Runtime_Throw` as a stable anchor to find the `isolate` argument on the stack.
_THROW_FRAME_RE = re.compile(r"#(\d+):?\s+.*?v8::internal::Runtime_Throw")

_VARIABLE_VALUE_RE = re.compile(r"=\s*(0x[0-9a-f]+)")

# Upper bound when probing for an idle worker thread.
_MAX_PROBED_THREADS = 64


def check_isolate_command(session):
  """Check that `v8 isolate` resolves the current Isolate.
  If `Runtime_Throw` is on the stack, also check that the resolved address
  matches the `isolate` argument of that frame.
  """
  output = session.run_command("v8 isolate")
  m = _ISOLATE_LINE_RE.search(output)
  if not m:
    raise AssertionError(
        f"no `isolate = 0x...` line in `v8 isolate` output:\n{output}")
  isolate = int(m.group(1), 16)
  throw_frame = _THROW_FRAME_RE.search(session.run_command("bt"))
  if not throw_frame:
    return
  try:
    session.select_frame(int(throw_frame.group(1)))
    value = _VARIABLE_VALUE_RE.search(session.frame_variable("isolate"))
  finally:
    session.select_frame(0)
  if not value:
    # Skip if the argument is optimized out.
    return
  expected = int(value.group(1), 16)
  if isolate != expected:
    raise AssertionError(
        f"`v8 isolate` reported {isolate:#x} but the `isolate` argument of "
        f"the Runtime_Throw frame is {expected:#x}")


def check_isolate_none_on_idle_thread(session):
  """Check that `v8 isolate` reports `<none>` on at least one thread e.g.
  an idle worker pool thread.
  """
  try:
    for index in range(2, _MAX_PROBED_THREADS + 1):
      if not session.select_thread(index):
        continue
      output = session.run_command("v8 isolate")
      if _NONE_LINE_RE.search(output):
        return
    raise AssertionError("no probed thread reported `isolate = <none>`")
  finally:
    session.select_thread(1)
