# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Dummy test suite extension with some fruity tests.
"""

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc.base import OutProc


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    return [
          'bananas', 'apples', 'cherries', 'mangoes', 'strawberries',
          'blackberries', 'raspberries',
    ]


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class MockOutProc(OutProc):

  def __init__(self, expected_outcomes):
    OutProc.__init__(self, expected_outcomes)

  def _get_error_details(self, output):
    return "+Mock diff"


class TestCase(testcase.D8TestCase):
  def get_shell(self):
    return 'd8_mocked.py'

  def _get_files_params(self):
    return [self.name]

  @property
  def output_proc(self):
    if self.name == 'strawberries':
      return MockOutProc([])
    else:
      return super(TestCase, self).output_proc
