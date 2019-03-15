#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os
import sys
import tempfile
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.local.testsuite import TestSuite, TestGenerator
from testrunner.objects.testcase import TestCase
from testrunner.test_config import TestConfig


class TestSuiteTest(unittest.TestCase):
  def setUp(self):
    test_dir = os.path.dirname(__file__)
    self.test_root = os.path.join(test_dir, "fake_testsuite")
    self.test_config = TestConfig(
        command_prefix=[],
        extra_flags=[],
        isolates=False,
        mode_flags=[],
        no_harness=False,
        noi18n=False,
        random_seed=0,
        run_skipped=False,
        shell_dir='fake_testsuite/fake_d8',
        timeout=10,
        verbose=False,
    )

    self.suite = TestSuite.Load(self.test_root, self.test_config)

  def testLoadingTestSuites(self):
    self.assertEquals(self.suite.name, "fake_testsuite")
    self.assertEquals(self.suite.test_config, self.test_config)

    # Verify that the components of the TestSuite aren't loaded yet.
    self.assertIsNone(self.suite.tests)
    self.assertIsNone(self.suite.statusfile)

  def testLoadingTestsFromDisk(self):
    tests = self.suite.load_tests_from_disk(
      statusfile_variables={})
    def is_generator(iterator):
      return iterator == iter(iterator)

    self.assertTrue(is_generator(tests))
    self.assertEquals(tests.test_count_estimate, 2)

    slow_tests, fast_tests = list(tests.slow_tests), list(tests.fast_tests)
    # Verify that the components of the TestSuite are loaded.
    self.assertTrue(len(slow_tests) == len(fast_tests) == 1)
    self.assertTrue(all(test.is_slow for test in slow_tests))
    self.assertFalse(any(test.is_slow for test in fast_tests))
    self.assertIsNotNone(self.suite.statusfile)

  def testMergingTestGenerators(self):
    tests = self.suite.load_tests_from_disk(
      statusfile_variables={})
    more_tests = self.suite.load_tests_from_disk(
      statusfile_variables={})

    # Merge the test generators
    tests.merge(more_tests)
    self.assertEquals(tests.test_count_estimate, 4)

    # Check the tests are sorted by speed
    test_speeds = []
    for test in tests:
      test_speeds.append(test.is_slow)

    self.assertEquals(test_speeds, [True, True, False, False])


if __name__ == '__main__':
    unittest.main()
