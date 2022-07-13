#!/usr/bin/env python3
# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Test integrating the sequence processor into a simple test pipeline.
"""

import os
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc import base
from testrunner.testproc.loader import LoadProc
from testrunner.testproc.sequence import SequenceProc


class FakeExecutionProc(base.TestProc):
  """Simulates the pipeline sink consuming and running the tests.

  Test execution is simulated for each test by calling run().
  """

  def __init__(self):
    super(FakeExecutionProc, self).__init__()
    self.tests = []

  def next_test(self, test):
    self.tests.append(test)
    return True

  def run(self):
    test = self.tests.pop()
    self._send_result(test, test.n)


class FakeResultObserver(base.TestProcObserver):
  """Observer to track all results sent back through the pipeline."""

  def __init__(self):
    super(FakeResultObserver, self).__init__()
    self.tests = set([])

  def _on_result_for(self, test, result):
    self.tests.add(test.n)


class FakeTest(object):
  """Simple test representation to differentiate light/heavy tests."""

  def __init__(self, n, is_heavy):
    self.n = n
    self.is_heavy = is_heavy
    self.keep_output = False


class TestSequenceProc(unittest.TestCase):

  def _test(self, tests, batch_size, max_heavy):
    # Set up a simple processing pipeline:
    # Loader -> observe results -> sequencer -> execution.
    loader = LoadProc(iter(tests))
    results = FakeResultObserver()
    sequence_proc = SequenceProc(max_heavy)
    execution = FakeExecutionProc()
    loader.connect_to(results)
    results.connect_to(sequence_proc)
    sequence_proc.connect_to(execution)

    # Fill the execution queue (with the number of tests potentially
    # executed in parallel).
    loader.load_initial_tests(batch_size)

    # Simulate the execution test by test.
    while execution.tests:
      # Assert the invariant of maximum heavy tests executed simultaneously.
      self.assertLessEqual(
          sum(int(test.is_heavy) for test in execution.tests), max_heavy)

      # As in the real pipeline, running a test and returning its result
      # will add another test into the pipeline.
      execution.run()

    # Ensure that all tests are processed and deliver results.
    self.assertEqual(set(test.n for test in tests), results.tests)

  def test_wrong_usage(self):
    with self.assertRaises(Exception):
      SequenceProc(0)

  def test_no_tests(self):
    self._test([], 1, 1)

  def test_large_batch_light(self):
    self._test([
        FakeTest(0, False),
        FakeTest(1, False),
        FakeTest(2, False),
    ], 4, 1)

  def test_small_batch_light(self):
    self._test([
        FakeTest(0, False),
        FakeTest(1, False),
        FakeTest(2, False),
    ], 2, 1)

  def test_large_batch_heavy(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, True),
        FakeTest(2, True),
    ], 4, 1)

  def test_small_batch_heavy(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, True),
        FakeTest(2, True),
    ], 2, 1)

  def test_large_batch_mixed(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, False),
        FakeTest(2, True),
        FakeTest(3, False),
    ], 4, 1)

  def test_small_batch_mixed(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, False),
        FakeTest(2, True),
        FakeTest(3, False),
    ], 2, 1)

  def test_large_batch_more_heavy(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, True),
        FakeTest(2, True),
        FakeTest(3, False),
        FakeTest(4, True),
        FakeTest(5, True),
        FakeTest(6, False),
    ], 4, 2)

  def test_small_batch_more_heavy(self):
    self._test([
        FakeTest(0, True),
        FakeTest(1, True),
        FakeTest(2, True),
        FakeTest(3, False),
        FakeTest(4, True),
        FakeTest(5, True),
        FakeTest(6, False),
    ], 2, 2)


if __name__ == '__main__':
  unittest.main()
