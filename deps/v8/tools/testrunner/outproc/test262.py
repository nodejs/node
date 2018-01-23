# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from . import base


class ExceptionOutProc(base.OutProc):
  """Output processor for tests with expected exception."""
  def __init__(self, expected_outcomes, expected_exception=None):
    super(ExceptionOutProc, self).__init__(expected_outcomes)
    self._expected_exception = expected_exception

  def _is_failure_output(self, output):
    if output.exit_code != 0:
      return True
    if self._expected_exception != self._parse_exception(output.stdout):
      return True
    return 'FAILED!' in output.stdout

  def _parse_exception(self, string):
    # somefile:somelinenumber: someerror[: sometext]
    # somefile might include an optional drive letter on windows e.g. "e:".
    match = re.search(
        '^(?:\w:)?[^:]*:[0-9]+: ([^: ]+?)($|: )', string, re.MULTILINE)
    if match:
      return match.group(1).strip()
    else:
      return None


def _is_failure_output(self, output):
  return (
    output.exit_code != 0 or
    'FAILED!' in output.stdout
  )


class NoExceptionOutProc(base.OutProc):
  """Output processor optimized for tests without expected exception."""
NoExceptionOutProc._is_failure_output = _is_failure_output


class PassNoExceptionOutProc(base.PassOutProc):
  """
  Output processor optimized for tests expected to PASS without expected
  exception.
  """
PassNoExceptionOutProc._is_failure_output = _is_failure_output


PASS_NO_EXCEPTION = PassNoExceptionOutProc()
