# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

from ..testproc.base import (
    DROP_RESULT, DROP_OUTPUT, DROP_PASS_OUTPUT, DROP_PASS_STDOUT)
from ..local import statusfile
from ..testproc.result import Result


OUTCOMES_PASS = [statusfile.PASS]
OUTCOMES_FAIL = [statusfile.FAIL]
OUTCOMES_PASS_OR_TIMEOUT = [statusfile.PASS, statusfile.TIMEOUT]
OUTCOMES_FAIL_OR_TIMEOUT = [statusfile.FAIL, statusfile.TIMEOUT]


class BaseOutProc(object):
  def process(self, output, reduction=None):
    has_unexpected_output = self.has_unexpected_output(output)
    if has_unexpected_output:
      self.regenerate_expected_files(output)
    return self._create_result(has_unexpected_output, output, reduction)

  def regenerate_expected_files(self, output):
    return

  def has_unexpected_output(self, output):
    return self.get_outcome(output) not in self.expected_outcomes

  def _create_result(self, has_unexpected_output, output, reduction):
    """Creates Result instance. When reduction is passed it tries to drop some
    parts of the result to save memory and time needed to send the result
    across process boundary. None disables reduction and full result is created.
    """
    if reduction == DROP_RESULT:
      return None
    if reduction == DROP_OUTPUT:
      return Result(has_unexpected_output, None)
    if not has_unexpected_output:
      if reduction == DROP_PASS_OUTPUT:
        return Result(has_unexpected_output, None)
      if reduction == DROP_PASS_STDOUT:
        return Result(has_unexpected_output, output.without_text())

    return Result(has_unexpected_output, output)

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


class NegPassOutProc(Negative, PassOutProc):
  """Output processor optimized for negative tests expected to PASS"""
  pass


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
DEFAULT_NEGATIVE = NegPassOutProc()


class ExpectedOutProc(OutProc):
  """Output processor that has is_failure_output depending on comparing the
  output with the expected output.
  """
  def __init__(self, expected_outcomes, expected_filename,
                regenerate_expected_files=False):
    super(ExpectedOutProc, self).__init__(expected_outcomes)
    self._expected_filename = expected_filename
    self._regenerate_expected_files = regenerate_expected_files

  def _is_failure_output(self, output):
    if output.exit_code != 0:
        return True

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

  def regenerate_expected_files(self, output):
    if not self._regenerate_expected_files:
      return
    lines = output.stdout.splitlines()
    with open(self._expected_filename, 'w') as f:
      for _, line in enumerate(lines):
        f.write(line+'\n')

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
            # Android linker warning.
            line.startswith('WARNING: linker:') or
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
