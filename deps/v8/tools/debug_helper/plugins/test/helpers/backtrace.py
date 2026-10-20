# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for validating rendered debugger backtrace annotations."""

from collections import Counter
import os
import re

_BACKTRACE_ANNOTATION_LINE_RE = re.compile(
    r"(?P<annotation>\[(?:[^\]]+ @ .+?(?::\d+:\d+)?|<anonymous>)\])"
    r"(?P<trailer>\s*\(this=[^\)]*\))?(?:\s*\([^\)]*\))?\s*$")
_SCRIPT_ANNOTATION_RE = re.compile(
    r"^\[(?P<function>[^\]]+?) @ (?P<script>.+?):(?P<line>\d+):(?P<column>\d+)\]$"
)
_SCRIPT_ONLY_ANNOTATION_RE = re.compile(
    r"^\[(?P<function>[^\]]+?) @ (?P<script>.+)\]$")
# Receiver pointer in `(this=0xADDR, argc=N)`.
_ADDR_RE = re.compile(r"0x[0-9a-fA-F]+")

_DEFAULT_EXPECTED_ANNOTATIONS = (
    "[test_func_3 @ <base>/throw.js:15:21] (this=<addr>, argc=4)",
    "[<anonymous> @ <base>/throw.js:10:19] (this=<addr>, argc=4)",
    "[test_func_2 @ <base>/throw.js:9:21] (this=<addr>, argc=4)",
    "[test_func_1 @ <base>/throw.js:5:21] (this=<addr>, argc=4)",
    "[<anonymous> @ <base>/throw.js:1:1] (this=<addr>, argc=1)",
)


def _normalize_annotation(annotation, trailer):
  """Reduce debugger-specific paths to a stable, basename-only annotation."""
  annotation = annotation.strip()
  # Mask the receiver pointer so the trailer compares stably across runs.
  suffix = "" if not trailer else " " + _ADDR_RE.sub("<addr>", trailer.strip())
  match = _SCRIPT_ANNOTATION_RE.match(annotation)
  if match:
    return (f"[{match.group('function').strip()} @ <base>/"
            f"{os.path.basename(match.group('script'))}:"
            f"{match.group('line')}:{match.group('column')}]{suffix}")
  match = _SCRIPT_ONLY_ANNOTATION_RE.match(annotation)
  if not match:
    return annotation + suffix
  return (f"[{match.group('function').strip()} @ <base>/"
          f"{os.path.basename(match.group('script'))}]{suffix}")


def extract_backtrace_annotations(output):
  """Collect rendered annotations from raw debugger backtrace text."""
  annotations = []
  for line in output.splitlines():
    match = _BACKTRACE_ANNOTATION_LINE_RE.search(line)
    if not match:
      continue
    annotations.append(
        _normalize_annotation(
            match.group("annotation"), match.group("trailer")))
  return annotations


def check_backtrace(output, label, expected_annotations=None):
  """Check normalized backtrace annotations against the expected rendering."""
  expected = (
      _DEFAULT_EXPECTED_ANNOTATIONS
      if expected_annotations is None else expected_annotations)
  found = extract_backtrace_annotations(output)
  expected_counter = Counter(expected)
  found_counter = Counter(found)
  missing = list((expected_counter - found_counter).elements())
  extras = list((found_counter - expected_counter).elements())
  if not missing and not extras:
    return None

  lines = [output, "\nFound normalized backtrace annotations:\n"]
  for annotation in found:
    lines.append(f"  {annotation}\n")
  if missing:
    lines.append(f"\nMissing {label} backtrace annotations:\n")
    for annotation in missing:
      lines.append(f"  {annotation}\n")
  if extras:
    lines.append(f"\nUnexpected {label} backtrace annotations:\n")
    for annotation in extras:
      lines.append(f"  {annotation}\n")
  return "".join(lines)
