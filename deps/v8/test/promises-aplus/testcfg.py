# Copyright 2014 the V8 project authors. All rights reserved.
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
# 'AS IS' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import hashlib
import os
import shutil
import sys
import tarfile

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


"""
Requirements for using this test suite:
Download http://sinonjs.org/releases/sinon-1.7.3.js into
test/promises-aplus/sinon.
Download https://github.com/promises-aplus/promises-tests/tree/2.0.3 into
test/promises-aplus/promises-tests.
"""

TEST_NAME = 'promises-tests'


class PromiseAplusTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    self.root = root
    self.test_files_root = os.path.join(self.root, TEST_NAME, 'lib', 'tests')
    self.name = name
    self.helper_files_pre = [
      os.path.join(root, 'lib', name) for name in
      ['global.js', 'require.js', 'mocha.js', 'adapter.js']
    ]
    self.helper_files_post = [
      os.path.join(root, 'lib', name) for name in
      ['run-tests.js']
    ]

  def CommonTestName(self, testcase):
    return testcase.path.split(os.path.sep)[-1]

  def ListTests(self, context):
    return [testcase.TestCase(self, fname[:-len('.js')]) for fname in
            os.listdir(os.path.join(self.root, TEST_NAME, 'lib', 'tests'))
            if fname.endswith('.js')]

  def GetParametersForTestCase(self, testcase, context):
    files = (
        self.helper_files_pre +
        [os.path.join(self.test_files_root, testcase.path + '.js')] +
        self.helper_files_post
    )
    flags = testcase.flags + context.mode_flags + ['--allow-natives-syntax']
    return files, flags, {}

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, TEST_NAME,
                            'lib', 'tests', testcase.path + '.js')
    with open(filename) as f:
      return f.read()

  def IsNegativeTest(self, testcase):
    return '@negative' in self.GetSourceForTest(testcase)

  def IsFailureOutput(self, testcase):
    if testcase.output.exit_code != 0:
      return True
    return not 'All tests have run.' in testcase.output.stdout or \
           'FAIL:' in testcase.output.stdout

def GetSuite(name, root):
  return PromiseAplusTestSuite(name, root)
