#! /usr/bin/python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import re
import argparse


def get_coverage(file, lines):
  content = file.read()

  results = []
  coverage_map = {}

  # Format 1: Chromium/Fuzzilli coverage dashboard (<tr id="123-row">)
  row_pattern = re.compile(r'<tr id="(\d+)-row".*?>(.*?)</tr>', re.DOTALL)
  for match in row_pattern.finditer(content):
    line_num = int(match.group(1))
    row_content = match.group(2)
    if 'class="uncovered-count"' in row_content:
      coverage_map[line_num] = "0"
    elif 'class="covered-count"' in row_content:
      count_match = re.search(r'class="covered-count" title="(\d+)"',
                              row_content)
      coverage_map[line_num] = count_match.group(
          1) if count_match else "covered"
    else:
      coverage_map[line_num] = None

  # Format 2: V8 CI llvm-cov HTML report (<td class='line-number'><a name='L123'>)
  if not coverage_map:
    llvm_row_pattern = re.compile(
        r"<td\s+class=['\"]line-number['\"][^>]*>\s*<a\s+name=['\"]L(\d+)['\"][^>]*>"
        r"\s*<pre>\s*\d+\s*</pre>\s*</a>\s*</td>\s*"
        r"<td\s+class=['\"](covered-line|uncovered-line|skipped-line)['\"][^>]*>"
        r"\s*(?:<pre>([^<]*)</pre>)?\s*</td>")
    for match in llvm_row_pattern.finditer(content):
      line_num = int(match.group(1))
      line_class = match.group(2)
      count_text = (match.group(3) or "").strip()
      if line_class == "covered-line":
        coverage_map[line_num] = count_text if count_text else "covered"
      elif line_class == "uncovered-line":
        if coverage_map.get(line_num) in (None, "0"):
          coverage_map[line_num] = "0"
      else:
        if line_num not in coverage_map:
          coverage_map[line_num] = None

  # Resolve requested lines from the map
  for line_range in lines:
    if '-' in line_range:
      start, end = map(int, line_range.split('-'))
      line_nums = range(start, end + 1)
    else:
      line_nums = [int(line_range)]

    for line_num in line_nums:
      results.append((line_num, coverage_map.get(line_num, "Not found")))
  return results


if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Extract line coverage from a Fuzzilli coverage report')
  parser.add_argument(
      'file',
      type=argparse.FileType('r'),
      help='the downloaded HTML file containing the coverage report')
  parser.add_argument(
      'line',
      nargs='+',
      help='either a line number or a block of lines denoted as <begin>-<end>')
  args = parser.parse_args()
  coverage = get_coverage(args.file, args.line)
  for line_num, status in coverage:
    print(f"{line_num}: {status}")
