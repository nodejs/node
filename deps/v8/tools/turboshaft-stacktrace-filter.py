#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
A filter tool that reads a stack trace or debug output from stdin and
simplifies overly long Turboshaft C++ templated type names.
Similar to c++filt but specifically targets Turboshaft.
"""

import sys
import argparse
import os

from turboshaft_type_formatter import TurboshaftTypeFormatter

_formatter = TurboshaftTypeFormatter(anchor_line_start=False)


def process_line(line):
  # Simplify absolute paths by replacing the home directory.
  # (Totally orthogonal to the Turboshaft filtering, but a nice quality of life improvement for shorter stacktraces regardless.)
  home_dir = os.path.expanduser('~')
  if home_dir:
    line = line.replace(home_dir, '~')

  line = _formatter.simplify_namespaces(line)

  pos = 0
  while pos < len(line):
    # Only simplify a line if there is some Turboshaft pattern in there.
    match = _formatter.combined_pattern.search(line, pos)
    if not match:
      break
    symbol_start_idx = match.start()

    to_format = line[symbol_start_idx:]
    to_format = _formatter.format_type(to_format)
    line = line[:symbol_start_idx] + to_format

    # Advance pos to avoid infinite loops, move past the start of the pattern.
    pos = symbol_start_idx + 1

  return line


def main():
  example_usage = """
Example usage:
  out/debug/d8 crashing-test.js 2>&1 | ./tools/turboshaft-stacktrace-filter.py
  # or using bash |&
  out/debug/d8 crashing-test.js |& ./tools/turboshaft-stacktrace-filter.py
"""
  parser = argparse.ArgumentParser(
      description="Filter and simplify Turboshaft stack traces. Reads from stdin.",
      epilog=example_usage,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.parse_args()

  try:
    for line in sys.stdin:
      sys.stdout.write(process_line(line))
      sys.stdout.flush()
  except KeyboardInterrupt:
    sys.exit(0)


if __name__ == "__main__":
  main()
