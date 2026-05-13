# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helpers for validating rendered debugger backtrace annotations."""

from collections import Counter
import os
import re

_BACKTRACE_ANNOTATION_LINE_RE = re.compile(
    r"(?P<annotation>\[(?:[^\]]+ @ .+?(?::\d+:\d+)?|<anonymous>)\])(?:\s*\([^\)]*\))?\s*$"
)
_SCRIPT_ANNOTATION_RE = re.compile(
    r"^\[(?P<function>[^\]]+?) @ (?P<script>.+?):(?P<line>\d+):(?P<column>\d+)\]$"
)
_SCRIPT_ONLY_ANNOTATION_RE = re.compile(
    r"^\[(?P<function>[^\]]+?) @ (?P<script>.+)\]$")

_DEFAULT_EXPECTED_ANNOTATIONS = (
    "[test_func_3 @ <base>/throw.js:15:21]",
    "[<anonymous> @ <base>/throw.js:10:19]",
    "[test_func_2 @ <base>/throw.js:9:21]",
    "[test_func_1 @ <base>/throw.js:5:21]",
    "[<anonymous> @ <base>/throw.js:1:1]",
)


def _normalize_annotation(annotation):
  """Reduce debugger-specific paths to a stable, basename-only annotation."""
  annotation = annotation.strip()
  match = _SCRIPT_ANNOTATION_RE.match(annotation)
  if match:
    return (f"[{match.group('function').strip()} @ <base>/"
            f"{os.path.basename(match.group('script'))}:"
            f"{match.group('line')}:{match.group('column')}]")
  match = _SCRIPT_ONLY_ANNOTATION_RE.match(annotation)
  if not match:
    return annotation
  return (f"[{match.group('function').strip()} @ <base>/"
          f"{os.path.basename(match.group('script'))}]")


def extract_backtrace_annotations(output):
  """Collect rendered annotations from raw debugger backtrace text."""
  annotations = []
  for line in output.splitlines():
    match = _BACKTRACE_ANNOTATION_LINE_RE.search(line)
    if not match:
      continue
    annotations.append(_normalize_annotation(match.group("annotation")))
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
