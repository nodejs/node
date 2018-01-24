# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase

class WasmSpecTestsTestSuite(testsuite.TestSuite):
  def __init__(self, name, root):
    super(WasmSpecTestsTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      for filename in files:
        if (filename.endswith(".js")):
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetParametersForTestCase(self, testcase, context):
    flags = testcase.flags + context.mode_flags
    files = [os.path.join(self.root, testcase.path + self.suffix())]
    return files, flags, {}


def GetSuite(name, root):
  return WasmSpecTestsTestSuite(name, root)
