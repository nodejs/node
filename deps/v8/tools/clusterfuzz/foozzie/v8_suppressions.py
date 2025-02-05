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

import re

from itertools import zip_longest

# Max line length for regular experessions checking for lines to ignore.
MAX_LINE_LENGTH = 512

# Ignore by output pattern. Map from bug->regexp like above.
IGNORE_OUTPUT = {
  '_fake_difference_':
      re.compile(r'^.*___fake_difference___$', re.M),
}

# Avoid cross-arch comparisons if we find this string in the output.
# We also need to ignore this output in the diffs as this string might
# only be printed in the baseline run.
AVOID_CROSS_ARCH_COMPARISON_RE = (
    r'^Warning: This run cannot be compared across architectures\.$')

# Lines matching any of the following regular expressions will be ignored.
# Use uncompiled regular expressions - they'll be compiled later.
IGNORE_LINES = [
  r'^Warning: .+ is deprecated.*$',
  r'^Try --help for options$',
  AVOID_CROSS_ARCH_COMPARISON_RE,
]

# List of pairs (<flag string>, <regexp string>). If the regexp matches the
# test content, the flag is dropped from the command line of any run.
# If the flag is part of the third run, the entire run is dropped, as it
# in many cases would just be redundant to the second default run.
DROP_FLAGS_ON_CONTENT = [
    ('--jitless', r'\%Wasm(Struct|Array|GenerateRandomModule)\('),
]

###############################################################################
# Implementation - you should not need to change anything below this point.

# Compile regular expressions.
IGNORE_LINES = [re.compile(exp) for exp in IGNORE_LINES]

ORIGINAL_SOURCE_PREFIX = 'v8-foozzie source: '

SMOKE_TEST_SOURCE = 'foozzie smoke test'

SMOKE_TEST_END_TOKEN = '___foozzie___smoke_test_end___'

SMOKE_TEST_OUTPUT_RE = f'(.*?){SMOKE_TEST_END_TOKEN}'

SMOKE_TEST_INVERSE_OUTPUT_RE = f'.*?{SMOKE_TEST_END_TOKEN}\\s(.*)'


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


def short_line_output(line):
  if len(line) <= MAX_LINE_LENGTH:
    # Avoid copying.
    return line
  return line[0:MAX_LINE_LENGTH] + '...'


def diff_output(output1, output2, ignore1, ignore2):
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
  # test case. We always start with the smoke-test part.
  source = SMOKE_TEST_SOURCE

  for line1, line2 in zip_longest(lines1, lines2, fillvalue=None):

    # Only one of the two iterators should run out.
    assert not (line1 is None and line2 is None)

    # One iterator ends earlier.
    if line1 is None:
      return f'+ {short_line_output(line2)}', source
    if line2 is None:
      return f'- {short_line_output(line1)}', source

    # Lines are equal.
    if line1 == line2:
      # Instrumented original-source-file output must be equal in both
      # versions. It only makes sense to update it here when both lines
      # are equal.
      if source == SMOKE_TEST_SOURCE and line1 == SMOKE_TEST_END_TOKEN:
        source = None
      elif line1.startswith(ORIGINAL_SOURCE_PREFIX):
        source = line1[len(ORIGINAL_SOURCE_PREFIX):]
      continue

    # Lines are different.
    short_line1 = short_line_output(line1)
    short_line2 = short_line_output(line2)
    return f'- {short_line1}\n+ {short_line2}', source

  # No difference found.
  return None, source


def get_suppression(skip=False):
  return V8Suppression(skip)


def decode(output):
  try:
    return output.decode('utf-8')
  except UnicodeDecodeError:
    return output.decode('latin-1')


class V8Suppression(object):
  def __init__(self, skip):
    if skip:
      self.ignore_output = {}
      self.drop_flags_on_content = []
    else:
      self.ignore_output = IGNORE_OUTPUT
      self.drop_flags_on_content = DROP_FLAGS_ON_CONTENT

  def diff(self, output1, output2):
    # Diff capped lines in the presence of crashes.
    return self.diff_lines(
        *map(str.splitlines, map(decode, get_output_capped(output1, output2))))

  def diff_lines(self, output1_lines, output2_lines):
    return diff_output(
        output1_lines,
        output2_lines,
        IGNORE_LINES,
        IGNORE_LINES,
    )

  def reduced_output(self, output, source):
    """Return output reduced by the source's origin, either only smoke-test
    output or only non-smoke-test output.
    """
    if source == SMOKE_TEST_SOURCE:
      exp = SMOKE_TEST_OUTPUT_RE
    else:
      exp = SMOKE_TEST_INVERSE_OUTPUT_RE
    match = re.search(exp, output, re.S)
    if match:
      return match.group(1)
    return output

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

  def _remove_config_flag(self, config, flag):
    if config.has_config_flag(flag):
      config.remove_config_flag(flag)
      return [
          f'Dropped {flag} from {config.label} config based on content rule.'
      ]
    return []

  def adjust_configs_by_content(self, execution_configs, testcase):
    """Modifies the execution configs if the testcase content matches a
    regular expression defined in DROP_FLAGS_ON_CONTENT above.

    The specified flag is dropped from the first two configs. Further
    configs are dropped entirely if the specified flag is used.

    Returns: A changelog as a list of strings.
    """
    logs = []
    assert len(execution_configs) > 1
    for flag, regexp in self.drop_flags_on_content:
      # Check first if the flag we need to drop appears anywhere. This is
      # faster than processing the content.
      if not any(config.has_config_flag(flag) for config in execution_configs):
        continue
      if not re.search(regexp, testcase):
        continue
      logs += self._remove_config_flag(execution_configs[0], flag)
      logs += self._remove_config_flag(execution_configs[1], flag)
      for config in execution_configs[2:]:
        if config.has_config_flag(flag):
          execution_configs.remove(config)
          logs.append(f'Dropped {config.label} config using '
                      f'{flag} based on content rule.')
    return logs

  def adjust_configs_by_output(self, execution_configs, output):
    """Modifies the execution configs if the baseline output contains a
    certain directive.

    Currently this only searches for one particular directive, after which
    we ensure to compare on the same architecture.

    Returns: A changelog as a list of strings.
    """
    # Cross-arch configs have a same-arch fallback. Check first if there
    # are any, as this check is very cheap.
    logs = []
    if not any(config.fallback for config in execution_configs):
      return []
    if not re.search(AVOID_CROSS_ARCH_COMPARISON_RE, output, re.M):
      return []
    for i, config in enumerate(execution_configs):
      if config.fallback:
        logs.append(
            f'Running the {config.label} config on the same architecture.')
        execution_configs[i] = config.fallback
    return logs
