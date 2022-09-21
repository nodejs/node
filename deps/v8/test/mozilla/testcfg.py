# Copyright 2008 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from functools import reduce

import os

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc import mozilla

EXCLUDED = [
  "CVS",
  ".svn",
]

FRAMEWORK = [
  "browser.js",
  "shell.js",
  "jsref.js",
  "template.js",
]

TEST_DIRS = [
  "ecma",
  "ecma_2",
  "ecma_3",
  "js1_1",
  "js1_2",
  "js1_3",
  "js1_4",
  "js1_5",
]

class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_files(self):
    return set(FRAMEWORK)

  @property
  def excluded_dirs(self):
    return set(EXCLUDED)

  @property
  def test_dirs(self):
    return TEST_DIRS

  def _to_relpath(self, abspath, _):
    # TODO: refactor this by setting the test path during the TestCase creation
    return os.path.relpath(abspath, self.test_root)


class TestSuite(testsuite.TestSuite):

  def __init__(self, ctx, *args, **kwargs):
    super(TestSuite, self).__init__(ctx, *args, **kwargs)
    self.test_root = os.path.join(self.root, "data")
    self._test_loader.test_root = self.test_root

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    files = [os.path.join(self.suite.root, "mozilla-shell-emulation.js")]
    testfilename = self.path + ".js"
    testfilepath = testfilename.split(os.path.sep)
    for i in range(len(testfilepath)):
      script = os.path.join(self.suite.test_root,
                            reduce(os.path.join, testfilepath[:i], ""),
                            "shell.js")
      if os.path.exists(script):
        files.append(script)

    files.append(os.path.join(self.suite.test_root, testfilename))
    return files

  def _get_suite_flags(self):
    return ['--expose-gc']

  def _get_source_path(self):
    return os.path.join(self.suite.test_root, self.path + self._get_suffix())

  @property
  def output_proc(self):
    if not self.expected_outcomes:
      if self.path.endswith('-n'):
        return mozilla.MOZILLA_PASS_NEGATIVE
      return mozilla.MOZILLA_PASS_DEFAULT
    if self.path.endswith('-n'):
      return mozilla.NegOutProc(self.expected_outcomes)
    return mozilla.OutProc(self.expected_outcomes)
