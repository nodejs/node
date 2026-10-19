# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Emit and parse self-contained regexp-fuzzer testcases.

Local and ClusterFuzz drivers use this format:

  // <header lines, if any>
  const cases = [
  ["<pattern>","<flags>","<subject>",<lastIndex>],
  ...
  ];
  <harness.js, comment-only and blank lines stripped>

Each case occupies one line and ends with a comma for line-based minimization.
json.dumps escapes non-ASCII characters and lone surrogates.

ClusterFuzz writes thousands of these files per fuzz task to a small
partition, so a case stores only what the engine needs: no JSON whitespace,
and the foozzie source tag is derived from the pattern by harness.js at run
time rather than stored next to each case.  The harness is embedded without
its comments for the same reason.
"""

import json
from pathlib import Path

HARNESS_PATH = Path(__file__).resolve().parent / "harness.js"

_CASES_OPEN = "const cases = ["
_CASES_CLOSE = "];"


def _compact_harness():
  """harness.js with comment-only and blank lines removed.

  Only whole lines starting with `//` are dropped, so a `//` inside a string
  or regexp literal cannot be touched.  harness.js has no block comments.
  """
  lines = HARNESS_PATH.read_text(encoding="utf-8").splitlines()
  return "\n".join(
      l for l in lines if l.strip() and not l.lstrip().startswith("//"))


def emit_js(cases, header=()):
  """Emit cases as a self-contained d8 script."""
  lines = ["// " + h for h in header]
  lines.append(_CASES_OPEN)
  for pattern, flags, subject, last_index in cases:
    lines.append(
        json.dumps([pattern, flags, subject, last_index], separators=(",",
                                                                      ":")) +
        ",")
  lines.append(_CASES_CLOSE)
  lines.append(_compact_harness())
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
