# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from testrunner.local import command
from testrunner.local import testsuite
from testrunner.objects import testcase

SHELL = 'bigint_shell'

class VariantsGenerator(testsuite.VariantsGenerator):
  def _get_variants(self, test):
    return self._standard_variant


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    args = ['--list'] + self.test_config.extra_flags
    cmd = self.ctx.command(
        cmd_prefix=self.test_config.command_prefix,
        shell=self.ctx.platform_shell(SHELL, args, self.test_config.shell_dir),
        args=args)
    output = cmd.execute()

    if output.exit_code != 0:
      print("Test executable failed to list the tests.\n\nCmd:")
      print(cmd)
      print("\nStdout:")
      print(output.stdout)
      print("\nStderr:")
      print(output.stderr)
      print("\nExit code: %d" % output.exit_code)

    return sorted(output.stdout.strip().split())


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def _get_files_params(self):
    return [self.name]

  def get_shell(self):
    return SHELL
