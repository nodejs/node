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
except ImportError:
  # Python 2
  from itertools import izip_longest as zip_longest

# Max line length for regular experessions checking for lines to ignore.
MAX_LINE_LENGTH = 512

# For ignoring lines before carets and to ignore caret positions.
CARET_RE = re.compile(r'^\s*\^\s*$')

# Ignore by original source files. Map from bug->list of relative file paths in
# V8, e.g. '/v8/test/mjsunit/d8-performance-now.js' including /v8/. A test will
# be suppressed if one of the files below was used to mutate the test.
IGNORE_SOURCES = {
}

# Ignore by test case pattern. Map from config->bug->regexp. Config '' is used
# to match all configurations. Otherwise use either a compiler configuration,
# e.g. ignition or validate_asm or an architecture, e.g. x64 or ia32.
# Bug is preferred to be a crbug.com/XYZ, but can be any short distinguishable
# label.
# Regular expressions are assumed to be compiled. We use regexp.search.
IGNORE_TEST_CASES = {
}

# Ignore by output pattern. Map from config->bug->regexp. See IGNORE_TEST_CASES
# on how to specify config keys.
# Bug is preferred to be a crbug.com/XYZ, but can be any short distinguishable
# label.
# Regular expressions are assumed to be compiled. We use regexp.search.
IGNORE_OUTPUT = {
  '': {
    'crbug.com/689877':
        re.compile(r'^.*SyntaxError: .*Stack overflow$', re.M),
  },
}

# Lines matching any of the following regular expressions will be ignored
# if appearing on both sides. The capturing groups need to match exactly.
# Use uncompiled regular expressions - they'll be compiled later.
ALLOWED_LINE_DIFFS = [
  # Ignore caret position in stack traces.
  r'^\s*\^\s*$',

  # Ignore some stack trace headers as messages might not match.
  r'^(.*)TypeError: .* is not a function$',
  r'^(.*)TypeError: .* is not a constructor$',
  r'^(.*)TypeError: (.*) is not .*$',
  r'^(.*):\d+: TypeError: Message suppressed for fuzzers.*$',
  r'^(.*)ReferenceError: .* is not defined$',
  r'^(.*):\d+: ReferenceError: .* is not defined$',

  # These are rarely needed. It includes some cases above.
  r'^\w*Error: .* is not .*$',
  r'^(.*) \w*Error: .* is not .*$',
  r'^(.*):\d+: \w*Error: .* is not .*$',

  # Some test cases just print the message.
  r'^.* is not a function(.*)$',
  r'^(.*) is not a .*$',

  # crbug.com/680064. This subsumes one of the above expressions.
  r'^(.*)TypeError: .* function$',
]

# Lines matching any of the following regular expressions will be ignored.
# Use uncompiled regular expressions - they'll be compiled later.
IGNORE_LINES = [
  r'^Warning: unknown flag .*$',
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
  """Returns a pair of stdout strings.

  The strings are safely capped if at least one run has crashed.
  """

  # No length difference or no crash -> no capping.
  if (len(output1.stdout) == len(output2.stdout) or
      (not output1.HasCrashed() and not output2.HasCrashed())):
    return output1.stdout, output2.stdout

  # Both runs have crashed, cap by the shorter output.
  if output1.HasCrashed() and output2.HasCrashed():
    cap = min(len(output1.stdout), len(output2.stdout))
  # Only the first run has crashed, cap by its output length.
  elif output1.HasCrashed():
    cap = len(output1.stdout)
  # Similar if only the second run has crashed.
  else:
    cap = len(output2.stdout)

  return output1.stdout[0:cap], output2.stdout[0:cap]


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


def get_suppression(arch1, config1, arch2, config2, skip=False):
  return V8Suppression(arch1, config1, arch2, config2, skip)


class Suppression(object):
  def diff(self, output1, output2):
    return None

  def ignore_by_metadata(self, metadata):
    return None

  def ignore_by_content(self, testcase):
    return None

  def ignore_by_output1(self, output):
    return None

  def ignore_by_output2(self, output):
    return None


class V8Suppression(Suppression):
  def __init__(self, arch1, config1, arch2, config2, skip):
    self.arch1 = arch1
    self.config1 = config1
    self.arch2 = arch2
    self.config2 = config2
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
        *map(str.splitlines, get_output_capped(output1, output2)))

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
    for key in ['', self.arch1, self.arch2, self.config1, self.config2]:
      for bug, exp in IGNORE_TEST_CASES.get(key, {}).items():
        if exp.search(content):
          return bug
    return None

  def ignore_by_metadata(self, metadata):
    for bug, sources in self.ignore_sources.items():
      for source in sources:
        if source in metadata['sources']:
          return bug
    return None

  def ignore_by_output1(self, output):
    return self.ignore_by_output(output, self.arch1, self.config1)

  def ignore_by_output2(self, output):
    return self.ignore_by_output(output, self.arch2, self.config2)

  def ignore_by_output(self, output, arch, config):
    def check(mapping):
      for bug, exp in mapping.items():
        if exp.search(output):
          return bug
      return None
    for key in ['', arch, config]:
      bug = check(self.ignore_output.get(key, {}))
      if bug:
        return bug
    return None
