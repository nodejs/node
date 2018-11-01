# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase

ANY_JS = ".any.js"
WPT_ROOT = "/wasm/jsapi/"
META_SCRIPT_REGEXP = re.compile(r"META:\s*script=(.*)")

class TestSuite(testsuite.TestSuite):
  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)
    self.testroot = os.path.join(self.root, "data", "test", "js-api")
    self.mjsunit_js = os.path.join(os.path.dirname(self.root), "mjsunit",
                                   "mjsunit.js")

  def ListTests(self):
    tests = []
    for dirname, dirs, files in os.walk(self.testroot):
      for dotted in [x for x in dirs if x.startswith(".")]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if (filename.endswith(ANY_JS)):
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.testroot) + 1 : -len(ANY_JS)]
          testname = relpath.replace(os.path.sep, "/")
          test = self._create_test(testname)
          tests.append(test)
    return tests

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    files = [os.path.join(self.suite.mjsunit_js),
             os.path.join(self.suite.root, "testharness.js")]

    source = self.get_source()
    for script in META_SCRIPT_REGEXP.findall(source):
      if script.startswith(WPT_ROOT):
        # Matched an absolute path, strip the root and replace it with our
        # local root.
        script = os.path.join(self.suite.testroot, script[len(WPT_ROOT):])
      elif not script.startswith("/"):
        # Matched a relative path, prepend this test's directory.
        thisdir = os.path.dirname(self._get_source_path())
        script = os.path.join(thisdir, script)
      else:
        raise Exception("Unexpected absolute path for script: \"%s\"" % script);

      files.append(script)

    files.extend([
      self._get_source_path(),
      os.path.join(self.suite.root, "testharness-after.js")
    ])
    return files

  def _get_source_path(self):
    # All tests are named `path/name.any.js`
    return os.path.join(self.suite.testroot, self.path + ANY_JS)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
