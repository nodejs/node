# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Suppressions for V8 correctness fuzzer failures.

We support three types of suppressions:
1. Ignore test case by pattern.
Map a regular expression to a bug entry. A new failure will be reported
when the pattern matches a JS test case.
Subsequent matches will be recoreded under the first failure.

2. Ignore test run by output pattern:
Map a regular expression to a bug entry. A new failure will be reported
when the pattern matches the output of a particular run.
Subsequent matches will be recoreded under the first failure.

3. Relax line-to-line comparisons with expressions of lines to ignore and
lines to be normalized (i.e. ignore only portions of lines).
These are not tied to bugs, be careful to not silently switch off this tool!

Alternatively, think about adding a behavior change to v8_suppressions.js
to silence a particular class of problems.
"""

import itertools
import re

try:
  # Python 3
  from itertools import zip_longest
  PYTHON3 = True
except ImportError:
  # Python 2
  from itertools import izip_longest as zip_longest
  PYTHON3 = False

# Max line length for regular experessions checking for lines to ignore.
MAX_LINE_LENGTH = 512

# For ignoring lines before carets and to ignore caret positions.
CARET_RE = re.compile(r'^\s*\^\s*$')

# Ignore by original source files. Map from bug->list of relative file paths,
# e.g. 'v8/test/mjsunit/d8-performance-now.js'. A test will be suppressed if
# one of the files below was used to mutate the test.
IGNORE_SOURCES = {
}

# Ignore by test case pattern. Map from bug->regexp.
# Bug is preferred to be a crbug.com/XYZ, but can be any short distinguishable
# label.
# Regular expressions are assumed to be compiled. We use regexp.search.
IGNORE_TEST_CASES = {
}

# Ignore by output pattern. Map from bug->regexp like above.
IGNORE_OUTPUT = {
  'crbug.com/689877':
      re.compile(r'^.*SyntaxError: .*Stack overflow$', re.M),
  '_fake_difference_':
      re.compile(r'^.*___fake_difference___$', re.M),
}

# Lines matching any of the following regular expressions will be ignored
# if appearing on both sides. The capturing groups need to match exactly.
# Use uncompiled regular expressions - they'll be compiled later.
ALLOWED_LINE_DIFFS = [
  # Ignore caret position in stack traces.
  r'^\s*\^\s*$',
]

# Lines matching any of the following regular expressions will be ignored.
# Use uncompiled regular expressions - they'll be compiled later.
IGNORE_LINES = [
  r'^Warning: .+ is deprecated.*$',
  r'^Try --help for options$',

  # crbug.com/705962
  r'^\s\[0x[0-9a-f]+\]$',
]


###############################################################################
# Implementation - you should not need to change anything below this point.

# Compile regular expressions.
ALLOWED_LINE_DIFFS = [re.compile(exp) for exp in ALLOWED_LINE_DIFFS]
IGNORE_LINES = [re.compile(exp) for exp in IGNORE_LINES]

ORIGINAL_SOURCE_PREFIX = 'v8-foozzie source: '


def get_output_capped(output1, output2):
  """Returns a pair of stdout byte arrays.

  The arrays are safely capped if at least one run has crashed.
  """

  # No length difference or no crash -> no capping.
  if (len(output1.stdout_bytes) == len(output2.stdout_bytes) or
      (not output1.HasCrashed() and not output2.HasCrashed())):
    return output1.stdout_bytes, output2.stdout_bytes

  # Both runs have crashed, cap by the shorter output.
  if output1.HasCrashed() and output2.HasCrashed():
    cap = min(len(output1.stdout_bytes), len(output2.stdout_bytes))
  # Only the first run has crashed, cap by its output length.
  elif output1.HasCrashed():
    cap = len(output1.stdout_bytes)
  # Similar if only the second run has crashed.
  else:
    cap = len(output2.stdout_bytes)

  return output1.stdout_bytes[0:cap], output2.stdout_bytes[0:cap]


def line_pairs(lines):
  return zip_longest(
      lines, itertools.islice(lines, 1, None), fillvalue=None)


def caret_match(line1, line2):
  if (not line1 or
      not line2 or
      len(line1) > MAX_LINE_LENGTH or
      len(line2) > MAX_LINE_LENGTH):
    return False
  return bool(CARET_RE.match(line1) and CARET_RE.match(line2))


def short_line_output(line):
  if len(line) <= MAX_LINE_LENGTH:
    # Avoid copying.
    return line
  return line[0:MAX_LINE_LENGTH] + '...'


def ignore_by_regexp(line1, line2, allowed):
  if len(line1) > MAX_LINE_LENGTH or len(line2) > MAX_LINE_LENGTH:
    return False
  for exp in allowed:
    match1 = exp.match(line1)
    match2 = exp.match(line2)
    if match1 and match2:
      # If there are groups in the regexp, ensure the groups matched the same
      # things.
      if match1.groups() == match2.groups():  # tuple comparison
        return True
  return False


def diff_output(output1, output2, allowed, ignore1, ignore2):
  """Returns a tuple (difference, source).

  The difference is None if there's no difference, otherwise a string
  with a readable diff.

  The source is the last source output within the test case, or None if no
  such output existed.
  """
  def useful_line(ignore):
    def fun(line):
      return all(not e.match(line) for e in ignore)
    return fun

  lines1 = list(filter(useful_line(ignore1), output1))
  lines2 = list(filter(useful_line(ignore2), output2))

  # This keeps track where we are in the original source file of the fuzz
  # test case.
  source = None

  for ((line1, lookahead1), (line2, lookahead2)) in zip_longest(
      line_pairs(lines1), line_pairs(lines2), fillvalue=(None, None)):

    # Only one of the two iterators should run out.
    assert not (line1 is None and line2 is None)

    # One iterator ends earlier.
    if line1 is None:
      return '+ %s' % short_line_output(line2), source
    if line2 is None:
      return '- %s' % short_line_output(line1), source

    # If lines are equal, no further checks are necessary.
    if line1 == line2:
      # Instrumented original-source-file output must be equal in both
      # versions. It only makes sense to update it here when both lines
      # are equal.
      if line1.startswith(ORIGINAL_SOURCE_PREFIX):
        source = line1[len(ORIGINAL_SOURCE_PREFIX):]
      continue

    # Look ahead. If next line is a caret, ignore this line.
    if caret_match(lookahead1, lookahead2):
      continue

    # Check if a regexp allows these lines to be different.
    if ignore_by_regexp(line1, line2, allowed):
      continue

    # Lines are different.
    return (
        '- %s\n+ %s' % (short_line_output(line1), short_line_output(line2)),
        source,
    )

  # No difference found.
  return None, source


def get_suppression(skip=False):
  return V8Suppression(skip)

def decode(output):
  if PYTHON3:
    try:
      return output.decode('utf-8')
    except UnicodeDecodeError:
      return output.decode('latin-1')
  return output

class V8Suppression(object):
  def __init__(self, skip):
    if skip:
      self.allowed_line_diffs = []
      self.ignore_output = {}
      self.ignore_sources = {}
    else:
      self.allowed_line_diffs = ALLOWED_LINE_DIFFS
      self.ignore_output = IGNORE_OUTPUT
      self.ignore_sources = IGNORE_SOURCES

  def diff(self, output1, output2):
    # Diff capped lines in the presence of crashes.
    return self.diff_lines(
        *map(str.splitlines, map(decode, get_output_capped(output1, output2))))

  def diff_lines(self, output1_lines, output2_lines):
    return diff_output(
        output1_lines,
        output2_lines,
        self.allowed_line_diffs,
        IGNORE_LINES,
        IGNORE_LINES,
    )

  def ignore_by_content(self, testcase):
    # Strip off test case preamble.
    try:
      lines = testcase.splitlines()
      lines = lines[lines.index(
          'print("js-mutation: start generated test case");'):]
      content = '\n'.join(lines)
    except ValueError:
      # Search the whole test case if preamble can't be found. E.g. older
      # already minimized test cases might have dropped the delimiter line.
      content = testcase
    for bug, exp in IGNORE_TEST_CASES.items():
      if exp.search(content):
        return bug
    return None

  def ignore_by_metadata(self, metadata):
    for bug, sources in self.ignore_sources.items():
      for source in sources:
        if source in metadata['sources']:
          return bug
    return None

  def ignore_by_output(self, output):
    def check(mapping):
      for bug, exp in mapping.items():
        if exp.search(output):
          return bug
      return None
    bug = check(self.ignore_output)
    if bug:
      return bug
    return None
