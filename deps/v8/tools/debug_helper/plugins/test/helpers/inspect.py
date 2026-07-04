# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Assertions and fixture-aware checks for `v8 inspect` integration tests."""

import re

_FIXTURE_JS_FRAME = "greetInner"
_FIXTURE_EXPECTED_ARGC = 3

_HEAD_RE = re.compile(r"^0x[0-9a-f]+\s+<", re.MULTILINE)


def check_inspect_receiver(session):
  """Inspect the Greeter receiver and assert basic shape + `.map`."""
  receiver, argc = session.frame_trailer(_FIXTURE_JS_FRAME)
  if argc != _FIXTURE_EXPECTED_ARGC:
    raise AssertionError(
        f"expected argc={_FIXTURE_EXPECTED_ARGC} for {_FIXTURE_JS_FRAME!r}, "
        f"got argc={argc}")
  output = session.v8_inspect(receiver)
  # The head is a line starting with `0x<hex> <`.
  if not _HEAD_RE.search(output):
    raise AssertionError(
        f"no v8 inspect head (0xADDR <Type...>) found in output:\n{output}")
  if not re.search(r"^\s+\.map=", output, re.MULTILINE):
    raise AssertionError(f"property .map= not found in output:\n{output}")


def check_inspect_recurses_into_map(session):
  """Inspecting the receiver renders its `.map=...` brief as a Map."""
  receiver = session.frame_receiver(_FIXTURE_JS_FRAME)
  output = session.v8_inspect(receiver)
  if not re.search(r"\.map=0x[0-9a-f]+\s+<Map[:>\s]", output):
    raise AssertionError(
        f"`.map=ADDR <Map...>` not found in receiver inspect:\n{output}")
