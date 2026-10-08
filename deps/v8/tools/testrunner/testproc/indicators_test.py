#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import io
import os
import sys
import unittest

# Set up paths to import from testrunner
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc import base
from testrunner.testproc import indicators
from testrunner.testproc import result


class FakeOutput:

  def __init__(self, crashed=False, timed_out=False):
    self._crashed = crashed
    self._timed_out = timed_out

  def HasCrashed(self):
    return self._crashed

  def HasTimedOut(self):
    return self._timed_out


class FakeTest:

  def __init__(self, name, is_fail=False):
    self.name = name
    self.is_fail = is_fail

  def __str__(self):
    return self.name


class TestStreamProgressIndicator(unittest.TestCase):

  def test_initialization(self):
    indicator = indicators.StreamProgressIndicator(None, None, 10)
    self.assertEqual(indicator._requirement, base.DROP_PASS_OUTPUT)
    self.assertEqual(indicator._total, 10)

  def test_on_test_result(self):
    indicator = indicators.StreamProgressIndicator(None, None, 1)

    # Test cases: (has_unexpected_output, crashed, timed_out, is_fail, expected_prefix, test_name)
    test_cases = [
        (False, False, False, False, 'PASS', 'my-test-1'),
        (True, True, False, False, 'CRASH', 'my-test-2'),
        (True, False, True, False, 'TIMEOUT', 'my-test-3'),
        (True, False, False, True, 'UNEXPECTED PASS', 'my-test-4'),
        (True, False, False, False, 'FAIL', 'my-test-5'),
    ]
    expected_output = ("PASS: my-test-1\n"
                       "CRASH: my-test-2\n"
                       "TIMEOUT: my-test-3\n"
                       "UNEXPECTED PASS: my-test-4\n"
                       "FAIL: my-test-5\n")

    with io.StringIO() as buf, contextlib.redirect_stdout(buf):
      for has_unexpected, crashed, timed_out, is_fail, expected_prefix, test_name in test_cases:
        test = FakeTest(test_name, is_fail=is_fail)
        output = FakeOutput(crashed=crashed, timed_out=timed_out)
        test_result = result.Result(has_unexpected, output)
        indicator.on_test_result(test, test_result)

      self.assertEqual(buf.getvalue(), expected_output)

  def test_other_api_methods(self):
    indicator = indicators.StreamProgressIndicator(None, None, 1)
    # These methods should not raise any exceptions
    indicator.on_heartbeat()
    indicator.on_event("some-event")
    indicator.finished()


if __name__ == '__main__':
  unittest.main()
