# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase
from testrunner.outproc import base as outproc

PROTOCOL_TEST_JS = "protocol-test.js"
EXPECTED_SUFFIX = "-expected.txt"
RESOURCES_FOLDER = "resources"

class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_files(self):
    return {PROTOCOL_TEST_JS}

  @property
  def excluded_dirs(self):
    return {RESOURCES_FOLDER}


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    self._source_flags = self._parse_source_flags()

  def _get_files_params(self):
    return [
      os.path.join(self.suite.root, PROTOCOL_TEST_JS),
      os.path.join(self.suite.root, self.path + self._get_suffix()),
    ]

  def _get_source_flags(self):
    return self._source_flags

  def _get_source_path(self):
    return os.path.join(self.suite.root, self.path + self._get_suffix())

  def get_shell(self):
    return 'inspector-test'

  def _get_resources(self):
    return [
      os.path.join(
        'test', 'inspector', 'debugger', 'resources', 'break-locations.js'),
    ]

  @property
  def output_proc(self):
    return outproc.ExpectedOutProc(
        self.expected_outcomes,
        os.path.join(self.suite.root, self.path) + EXPECTED_SUFFIX)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
