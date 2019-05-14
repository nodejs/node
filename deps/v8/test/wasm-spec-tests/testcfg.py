# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase


class TestLoader(testsuite.JSTestLoader):
  pass

class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    return [os.path.join(self.suite.root, self.path + self._get_suffix())]


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
