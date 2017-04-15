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

# Max line length for regular experessions checking for lines to ignore.
MAX_LINE_LENGTH = 512

# For ignoring lines before carets and to ignore caret positions.
CARET_RE = re.compile(r'^\s*\^\s*$')

# Ignore by original source files. Map from bug->relative file paths in V8,
# e.g. '/v8/test/mjsunit/d8-performance-now.js' including /v8/. A test will
# be suppressed if one of the files below was used to mutate the test.
IGNORE_SOURCES = {
  # This contains a usage of f.arguments that often fires.
  'crbug.com/662424': '/v8/test/mjsunit/regress/regress-2989.js',

  # crbug.com/681088
  'crbug.com/681088': '/v8/test/mjsunit/asm/asm-validation.js',
  'crbug.com/681088': '/v8/test/mjsunit/asm/b5528-comma.js',
  'crbug.com/681088': '/v8/test/mjsunit/asm/pointer-masking.js',
  'crbug.com/681088': '/v8/test/mjsunit/compiler/regress-443744.js',
  'crbug.com/681088': '/v8/test/mjsunit/regress/regress-599719.js',
  'crbug.com/681088': '/v8/test/mjsunit/regress/wasm/regression-647649.js',
  'crbug.com/681088': '/v8/test/mjsunit/wasm/asm-wasm.js',
  'crbug.com/681088': '/v8/test/mjsunit/wasm/asm-wasm-deopt.js',
  'crbug.com/681088': '/v8/test/mjsunit/wasm/asm-wasm-heap.js',
  'crbug.com/681088': '/v8/test/mjsunit/wasm/asm-wasm-literals.js',
  'crbug.com/681088': '/v8/test/mjsunit/wasm/asm-wasm-stack.js',

  # crbug.com/681236
  'crbug.com/681236': '/v8/test/mjsunit/wasm/asm-wasm-switch.js',
}

# Ignore by test case pattern. Map from bug->regexp.
# Regular expressions are assumed to be compiled. We use regexp.match.
# Make sure the code doesn't match in the preamble portion of the test case
# (i.e. in the modified inlined mjsunit.js). You can reference the comment
# between the two parts like so:
#  'crbug.com/666308':
#      re.compile(r'.*End stripped down and modified version.*'
#                 r'\.prototype.*instanceof.*.*', re.S)
# TODO(machenbach): Insert a JS sentinel between the two parts, because
# comments are stripped during minimization.
IGNORE_TEST_CASES = {
  'crbug.com/679957':
      re.compile(r'.*performance\.now.*', re.S),
}

# Ignore by output pattern. Map from config->bug->regexp. Config '' is used
# to match all configurations. Otherwise use either a compiler configuration,
# e.g. fullcode or validate_asm or an architecture, e.g. x64 or ia32 or a
# comma-separated combination, e.g. x64,fullcode, for more specific
# suppressions.
# Bug is preferred to be a crbug.com/XYZ, but can be any short distinguishable
# label.
# Regular expressions are assumed to be compiled. We use regexp.search.
IGNORE_OUTPUT = {
  '': {
    'crbug.com/664068':
        re.compile(r'RangeError', re.S),
    'crbug.com/667678':
        re.compile(r'\[native code\]', re.S),
    'crbug.com/681806':
        re.compile(r'\[object WebAssembly\.Instance\]', re.S),
  },
  'validate_asm': {
    'validate_asm':
        re.compile(r'TypeError'),
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
  r'^(.*)ReferenceError: .* is not defined$',
  r'^(.*):\d+: ReferenceError: .* is not defined$',

  # These are rarely needed. It includes some cases above.
  r'^\w*Error: .* is not .*$',
  r'^(.*) \w*Error: .* is not .*$',
  r'^(.*):\d+: \w*Error: .* is not .*$',

  # Some test cases just print the message.
  r'^.* is not a function(.*)$',
  r'^(.*) is not a .*$',

  # Ignore lines of stack traces as character positions might not match.
  r'^    at (?:new )?([^:]*):\d+:\d+(.*)$',
  r'^(.*):\d+:(.*)$',

  # crbug.com/662840
  r"^.*(?:Trying to access ')?(\w*)(?:(?:' through proxy)|"
  r"(?: is not defined))$",

  # crbug.com/680064. This subsumes one of the above expressions.
  r'^(.*)TypeError: .* function$',

  # crbug.com/681326
  r'^(.*<anonymous>):\d+:\d+(.*)$',
]

# Lines matching any of the following regular expressions will be ignored.
# Use uncompiled regular expressions - they'll be compiled later.
IGNORE_LINES = [
  r'^Validation of asm\.js module failed: .+$',
  r'^.*:\d+: Invalid asm.js: .*$',
  r'^Warning: unknown flag .*$',
  r'^Warning: .+ is deprecated.*$',
  r'^Try --help for options$',

  # crbug.com/677032
  r'^.*:\d+:.*asm\.js.*: success$',

  # crbug.com/680064
  r'^\s*at .* \(<anonymous>\)$',
]


###############################################################################
# Implementation - you should not need to change anything below this point.

# Compile regular expressions.
ALLOWED_LINE_DIFFS = [re.compile(exp) for exp in ALLOWED_LINE_DIFFS]
IGNORE_LINES = [re.compile(exp) for exp in IGNORE_LINES]

ORIGINAL_SOURCE_PREFIX = 'v8-foozzie source: '

def line_pairs(lines):
  return itertools.izip_longest(
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

  lines1 = filter(useful_line(ignore1), output1)
  lines2 = filter(useful_line(ignore2), output2)

  # This keeps track where we are in the original source file of the fuzz
  # test case.
  source = None

  for ((line1, lookahead1), (line2, lookahead2)) in itertools.izip_longest(
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


def get_suppression(arch1, config1, arch2, config2):
  return V8Suppression(arch1, config1, arch2, config2)


class Suppression(object):
  def diff(self, output1, output2):
    return None

  def ignore_by_metadata(self, metadata):
    return False

  def ignore_by_content(self, testcase):
    return False

  def ignore_by_output1(self, output):
    return False

  def ignore_by_output2(self, output):
    return False


class V8Suppression(Suppression):
  def __init__(self, arch1, config1, arch2, config2):
    self.arch1 = arch1
    self.config1 = config1
    self.arch2 = arch2
    self.config2 = config2

  def diff(self, output1, output2):
    return diff_output(
        output1.splitlines(),
        output2.splitlines(),
        ALLOWED_LINE_DIFFS,
        IGNORE_LINES,
        IGNORE_LINES,
    )

  def ignore_by_content(self, testcase):
    for bug, exp in IGNORE_TEST_CASES.iteritems():
      if exp.match(testcase):
        return bug
    return False

  def ignore_by_metadata(self, metadata):
    for bug, source in IGNORE_SOURCES.iteritems():
      if source in metadata['sources']:
        return bug
    return False

  def ignore_by_output1(self, output):
    return self.ignore_by_output(output, self.arch1, self.config1)

  def ignore_by_output2(self, output):
    return self.ignore_by_output(output, self.arch2, self.config2)

  def ignore_by_output(self, output, arch, config):
    def check(mapping):
      for bug, exp in mapping.iteritems():
        if exp.search(output):
          return bug
      return None
    bug = check(IGNORE_OUTPUT.get('', {}))
    if bug:
      return bug
    bug = check(IGNORE_OUTPUT.get(arch, {}))
    if bug:
      return bug
    bug = check(IGNORE_OUTPUT.get(config, {}))
    if bug:
      return bug
    bug = check(IGNORE_OUTPUT.get('%s,%s' % (arch, config), {}))
    if bug:
      return bug
    return None
