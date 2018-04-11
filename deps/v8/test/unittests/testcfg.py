# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import command
from testrunner.local import utils
from testrunner.local import testsuite
from testrunner.objects import testcase


class VariantsGenerator(testsuite.VariantsGenerator):
  def _get_variants(self, test):
    return self._standard_variant


class TestSuite(testsuite.TestSuite):
  def ListTests(self):
    shell = os.path.abspath(os.path.join(self.test_config.shell_dir, self.name))
    if utils.IsWindows():
      shell += ".exe"

    output = None
    for i in xrange(3): # Try 3 times in case of errors.
      cmd = command.Command(
        cmd_prefix=self.test_config.command_prefix,
        shell=shell,
        args=['--gtest_list_tests'] + self.test_config.extra_flags)
      output = cmd.execute()
      if output.exit_code == 0:
        break
      print "Test executable failed to list the tests (try %d).\n\nCmd:" % i
      print cmd
      print "\nStdout:"
      print output.stdout
      print "\nStderr:"
      print output.stderr
      print "\nExit code: %d" % output.exit_code
    else:
      raise Exception("Test executable failed to list the tests.")

    tests = []
    test_case = ''
    for line in output.stdout.splitlines():
      test_desc = line.strip().split()[0]
      if test_desc.endswith('.'):
        test_case = test_desc
      elif test_case and test_desc:
        test_path = test_case + test_desc
        tests.append(self._create_test(test_path))
    tests.sort(key=lambda t: t.path)
    return tests

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.TestCase):
  def _get_suite_flags(self):
    return (
        ["--gtest_filter=" + self.path] +
        ["--gtest_random_seed=%s" % self.random_seed] +
        ["--gtest_print_time=0"]
    )

  def get_shell(self):
    return self.suite.name


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
