# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from . import base


def _is_failure_output(output, is_async):
  return (output.exit_code != 0 or 'FAILED!' in output.stdout or
          (is_async and 'Test262:AsyncTestComplete' not in output.stdout))


class ExceptionOutProc(base.OutProc):
  """Output processor for tests with expected exception."""

  def __init__(self,
               expected_outcomes,
               expected_exception=None,
               negative=False,
               is_async=False):
    super(ExceptionOutProc, self).__init__(expected_outcomes)
    self._expected_exception = expected_exception
    self._negative = negative
    self._async = is_async

  @property
  def negative(self):
    return self._negative

  def _is_failure_output(self, output):
    if self._expected_exception != self._parse_exception(output.stdout):
      return True
    return _is_failure_output(output, self._async)

  def _parse_exception(self, string):
    # somefile:somelinenumber: someerror[: sometext]
    # somefile might include an optional drive letter on windows e.g. "e:".
    match = re.search(
        '^(?:\w:)?[^:]*:[0-9]+: ([^: ]+?)($|: )', string, re.MULTILINE)
    if match:
      return match.group(1).strip()
    else:
      return None


class NoExceptionOutProc(base.OutProc):
  """Output processor optimized for tests without expected exception."""

  def __init__(self, expected_outcomes, is_async=False):
    super(NoExceptionOutProc, self).__init__(expected_outcomes)
    self._async = is_async

  def _is_failure_output(self, output):
    return _is_failure_output(output, self._async)


class PassNoExceptionOutProc(base.PassOutProc):
  """
  Output processor optimized for tests expected to PASS without expected
  exception.
  """

  def __init__(self, is_async=False):
    super(PassNoExceptionOutProc, self).__init__()
    self._async = is_async

  def _is_failure_output(self, output):
    return _is_failure_output(output, self._async)
