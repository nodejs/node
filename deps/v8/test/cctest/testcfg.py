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

import os
import shutil

from testrunner.local import commands
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


class CcTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(CcTestSuite, self).__init__(name, root)
    if utils.IsWindows():
      build_dir = "build"
    else:
      build_dir = "out"
    self.serdes_dir = os.path.normpath(
        os.path.join(root, "..", "..", build_dir, ".serdes"))
    if os.path.exists(self.serdes_dir):
      shutil.rmtree(self.serdes_dir, True)
    os.makedirs(self.serdes_dir)

  def ListTests(self, context):
    shell = os.path.abspath(os.path.join(context.shell_dir, self.shell()))
    if utils.IsWindows():
      shell += ".exe"
    output = commands.Execute(context.command_prefix +
                              [shell, "--list"] +
                              context.extra_flags)
    if output.exit_code != 0:
      print output.stdout
      print output.stderr
      return []
    tests = []
    for test_desc in output.stdout.strip().split():
      if test_desc.find('<') < 0:
        # Native Client output can contain a few non-test arguments
        # before the tests. Skip these.
        continue
      raw_test, dependency = test_desc.split('<')
      if dependency != '':
        dependency = raw_test.split('/')[0] + '/' + dependency
      else:
        dependency = None
      test = testcase.TestCase(self, raw_test, dependency=dependency)
      tests.append(test)
    tests.sort()
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    testname = testcase.path.split(os.path.sep)[-1]
    serialization_file = os.path.join(self.serdes_dir, "serdes_" + testname)
    serialization_file += ''.join(testcase.flags).replace('-', '_')
    return (testcase.flags + [testcase.path] + context.mode_flags +
            ["--testing_serialization_file=" + serialization_file])

  def shell(self):
    return "cctest"


def GetSuite(name, root):
  return CcTestSuite(name, root)
