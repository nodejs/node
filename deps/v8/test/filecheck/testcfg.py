# Copyright 2025 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc.filecheck import FileCheckOutProc


class TestSuite(testsuite.TestSuite):

  def _test_loader_class(self):
    return testsuite.JSTestLoader

  def _test_class(self):
    return FileCheckTestCase


class FileCheckTestCase(testcase.D8TestCase):
  """Test case using llvm's FileCheck."""

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._source_flags = self._parse_source_flags()

  def _get_source_flags(self):
    return self._source_flags

  def _get_files_params(self):
    return [self._get_source_path()]

  def _get_source_path(self):
    return self.suite.root / self.path_js

  @property
  def output_proc(self):
    return FileCheckOutProc(self.expected_outcomes,
                            self.suite.root / self.path_and_suffix('.js'))
