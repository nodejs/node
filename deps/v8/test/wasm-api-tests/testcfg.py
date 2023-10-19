# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import command
from testrunner.local import utils
from testrunner.local import testsuite
from testrunner.objects import testcase

SHELL = "wasm_api_tests"


class VariantsGenerator(testsuite.VariantsGenerator):
  def _get_variants(self, test):
    return self._standard_variant


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    output = None
    for i in range(3): # Try 3 times in case of errors.
      args = ['--gtest_list_tests'] + self.test_config.extra_flags
      cmd = self.ctx.command(
          cmd_prefix=self.test_config.command_prefix,
          shell=self.ctx.platform_shell(SHELL, args,
                                        self.test_config.shell_dir),
          args=args)
      output = cmd.execute()
      if output.exit_code == 0:
        break

      print("Test executable failed to list the tests (try %d).\n\nCmd:" % i)
      print(cmd)
      print("\nStdout:")
      print(output.stdout)
      print("\nStderr:")
      print(output.stderr)
      print("\nExit code: %d" % output.exit_code)
    else:
      raise Exception("Test executable failed to list the tests.")

    # TODO create an ExecutableTestLoader for refactoring this similar to
    # JSTestLoader.
    test_names = []
    test_case = ''
    for line in output.stdout.splitlines():
      test_desc = line.strip().split()[0]
      if test_desc.endswith('.'):
        test_case = test_desc
      elif test_case and test_desc:
        test_names.append(test_case + test_desc)

    return sorted(test_names)


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.TestCase):
  def _get_suite_flags(self):
    return (
        [f"--gtest_filter={self.name}"] +
        [f"--gtest_random_seed={self.random_seed}"] +
        ["--gtest_print_time=0"]
    )

  def get_shell(self):
    return SHELL
