# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import itertools

from ..local import statusfile
from ..testproc.result import Result


OUTCOMES_PASS = [statusfile.PASS]
OUTCOMES_FAIL = [statusfile.FAIL]


class BaseOutProc(object):
  def process(self, output):
    return Result(self.has_unexpected_output(output), output)

  def has_unexpected_output(self, output):
    return self.get_outcome(output) not in self.expected_outcomes

  def get_outcome(self, output):
    if output.HasCrashed():
      return statusfile.CRASH
    elif output.HasTimedOut():
      return statusfile.TIMEOUT
    elif self._has_failed(output):
      return statusfile.FAIL
    else:
      return statusfile.PASS

  def _has_failed(self, output):
    execution_failed = self._is_failure_output(output)
    if self.negative:
      return not execution_failed
    return execution_failed

  def _is_failure_output(self, output):
    return output.exit_code != 0

  @property
  def negative(self):
    return False

  @property
  def expected_outcomes(self):
    raise NotImplementedError()


class Negative(object):
  @property
  def negative(self):
    return True


class PassOutProc(BaseOutProc):
  """Output processor optimized for positive tests expected to PASS."""
  def has_unexpected_output(self, output):
    return self.get_outcome(output) != statusfile.PASS

  @property
  def expected_outcomes(self):
    return OUTCOMES_PASS


class OutProc(BaseOutProc):
  """Output processor optimized for positive tests with expected outcomes
  different than a single PASS.
  """
  def __init__(self, expected_outcomes):
    self._expected_outcomes = expected_outcomes

  @property
  def expected_outcomes(self):
    return self._expected_outcomes

  # TODO(majeski): Inherit from PassOutProc in case of OUTCOMES_PASS and remove
  # custom get/set state.
  def __getstate__(self):
    d = self.__dict__
    if self._expected_outcomes is OUTCOMES_PASS:
      d = d.copy()
      del d['_expected_outcomes']
    return d

  def __setstate__(self, d):
    if '_expected_outcomes' not in d:
      d['_expected_outcomes'] = OUTCOMES_PASS
    self.__dict__.update(d)


# TODO(majeski): Override __reduce__ to make it deserialize as one instance.
DEFAULT = PassOutProc()


class ExpectedOutProc(OutProc):
  """Output processor that has is_failure_output depending on comparing the
  output with the expected output.
  """
  def __init__(self, expected_outcomes, expected_filename):
    super(ExpectedOutProc, self).__init__(expected_outcomes)
    self._expected_filename = expected_filename

  def _is_failure_output(self, output):
    with open(self._expected_filename, 'r') as f:
      expected_lines = f.readlines()

    for act_iterator in self._act_block_iterator(output):
      for expected, actual in itertools.izip_longest(
          self._expected_iterator(expected_lines),
          act_iterator,
          fillvalue=''
      ):
        if expected != actual:
          return True
      return False

  def _act_block_iterator(self, output):
    """Iterates over blocks of actual output lines."""
    lines = output.stdout.splitlines()
    start_index = 0
    found_eqeq = False
    for index, line in enumerate(lines):
      # If a stress test separator is found:
      if line.startswith('=='):
        # Iterate over all lines before a separator except the first.
        if not found_eqeq:
          found_eqeq = True
        else:
          yield self._actual_iterator(lines[start_index:index])
        # The next block of output lines starts after the separator.
        start_index = index + 1
    # Iterate over complete output if no separator was found.
    if not found_eqeq:
      yield self._actual_iterator(lines)

  def _actual_iterator(self, lines):
    return self._iterator(lines, self._ignore_actual_line)

  def _expected_iterator(self, lines):
    return self._iterator(lines, self._ignore_expected_line)

  def _ignore_actual_line(self, line):
    """Ignore empty lines, valgrind output, Android output and trace
    incremental marking output.
    """
    if not line:
      return True
    return (line.startswith('==') or
            line.startswith('**') or
            line.startswith('ANDROID') or
            line.startswith('###') or
            # FIXME(machenbach): The test driver shouldn't try to use slow
            # asserts if they weren't compiled. This fails in optdebug=2.
            line == 'Warning: unknown flag --enable-slow-asserts.' or
            line == 'Try --help for options')

  def _ignore_expected_line(self, line):
    return not line

  def _iterator(self, lines, ignore_predicate):
    for line in lines:
      line = line.strip()
      if not ignore_predicate(line):
        yield line
