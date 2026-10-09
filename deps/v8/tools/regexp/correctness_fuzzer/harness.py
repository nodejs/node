# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Emit and parse self-contained regexp-fuzzer testcases.

Local and ClusterFuzz drivers use this format:

  // <header lines, if any>
  const cases = [
  ["<pattern>", "<flags>", "<subject>", <lastIndex>, "<tag>"],
  ...
  ];
  <harness.js verbatim>

Each case occupies one line and ends with a comma for line-based minimization.
json.dumps escapes non-ASCII characters and lone surrogates.
"""

import json
from pathlib import Path

HARNESS_PATH = Path(__file__).resolve().parent / "harness.js"

_CASES_OPEN = "const cases = ["
_CASES_CLOSE = "];"

# Characters kept when reducing a pattern to its shape for the dedup tag.
_STRUCTURAL = set("()[]{}|*+?^$.-&!=<>:,")
_TAG_MAX = 40


def source_tag(pattern, flags):
  """Return a bounded source tag that groups patterns by structure."""
  out = []
  i = 0
  while i < len(pattern) and len(out) < _TAG_MAX:
    c = pattern[i]
    if c == "\\":
      nxt = pattern[i + 1] if i + 1 < len(pattern) else ""
      # Keep ASCII alphanumeric escapes; other escaped characters are literals.
      if nxt.isascii() and nxt.isalnum():
        out.append(c + nxt)
      i += 2
      continue
    if c in _STRUCTURAL:
      out.append(c)
    i += 1
  return "".join(sorted(flags)) + "/" + "".join(out)


def emit_js(cases, header=()):
  """Emit cases as a self-contained d8 script."""
  lines = ["// " + h for h in header]
  lines.append(_CASES_OPEN)
  for pattern, flags, subject, last_index in cases:
    lines.append(
        json.dumps(
            [pattern, flags, subject, last_index,
             source_tag(pattern, flags)]) + ",")
  lines.append(_CASES_CLOSE)
  lines.append(HARNESS_PATH.read_text(encoding="utf-8"))
  return "\n".join(lines)


def parse_testcase(text):
  """Parse cases, accepting missing trailing commas and optional fields."""
  cases = []
  in_cases = False
  for line in text.splitlines():
    stripped = line.strip()
    if not in_cases:
      in_cases = stripped == _CASES_OPEN
      continue
    if stripped == _CASES_CLOSE:
      break
    stripped = stripped.rstrip(",")
    if not stripped:
      continue
    fields = json.loads(stripped)
    pattern = fields[0]
    flags = fields[1] if len(fields) > 1 else ""
    subject = fields[2] if len(fields) > 2 else ""
    last_index = fields[3] if len(fields) > 3 else 0
    cases.append([pattern, flags, subject, last_index])
  if not in_cases:
    raise ValueError("no '%s' block found; not a file emitted by emit_js()" %
                     _CASES_OPEN)
  return cases
