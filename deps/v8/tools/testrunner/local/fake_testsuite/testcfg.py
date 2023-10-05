#!/usr/bin/env python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pathlib import Path

from testrunner.local import testsuite, statusfile
from testrunner.objects.testcase import TestCase

class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    return ["fast", "slow"]

  def list_tests(self):
    self.test_count_estimation = 2
    fast = self._create_test(Path("fast"), self.suite)
    slow = self._create_test(Path("slow"), self.suite)

    slow._statusfile_outcomes.append(statusfile.SLOW)
    yield fast
    yield slow


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase
