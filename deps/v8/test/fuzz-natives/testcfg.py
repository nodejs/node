# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import commands
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

class FuzzNativesTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(FuzzNativesTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    shell = os.path.abspath(os.path.join(context.shell_dir, self.shell()))
    if utils.IsWindows():
      shell += ".exe"
    output = commands.Execute(
        context.command_prefix +
        [shell, "--allow-natives-syntax", "-e",
         "try { var natives = %ListNatives();"
         "  for (var n in natives) { print(natives[n]); }"
         "} catch(e) {}"] +
        context.extra_flags)
    if output.exit_code != 0:
      print output.stdout
      print output.stderr
      assert False, "Failed to get natives list."
    tests = []
    for line in output.stdout.strip().split():
      try:
        (name, argc) = line.split(",")
        flags = ["--allow-natives-syntax",
                 "-e", "var NAME = '%s', ARGC = %s;" % (name, argc)]
        test = testcase.TestCase(self, name, flags)
        tests.append(test)
      except:
        # Work-around: If parsing didn't work, it might have been due to output
        # caused by other d8 flags.
        pass
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    name = testcase.path
    basefile = os.path.join(self.root, "base.js")
    return testcase.flags + [basefile] + context.mode_flags

def GetSuite(name, root):
  return FuzzNativesTestSuite(name, root)
