# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64

from testrunner.local import testsuite
from testrunner.objects import testcase


ADDITIONAL_VARIANTS = set([
    "maglev",
    "no_memory_protection_keys",
    "minor_ms",
    "stress_maglev",
    "conservative_stack_scanning",
    "precise_pinning",
])
SHELL = "v8_unittests"


class VariantsGenerator(testsuite.VariantsGenerator):

  def __init__(self, variants):
    super().__init__(variants)
    self._supported_variants = self._standard_variant + [
        v for v in variants if v in ADDITIONAL_VARIANTS
    ]

  def _get_variants(self, test):
    if test.only_standard_variant:
      return self._standard_variant
    return self._supported_variants


class TestListerDummy():
  """A one-off test case used for the look-up call that lists all the tests."""
  def __init__(self, suite):
    self.suite = suite

  def get_android_resources(self):
    # We require all golden files on the Android device when we list the tests.
    expectations = self.suite.root / 'interpreter' / 'bytecode_expectations'
    return list(expectations.glob('**/*.golden'))


class TestLoader(testsuite.TestLoader):
  def _list_test_filenames(self):
    args = ['--gtest_list_tests'] + self.test_config.extra_flags
    shell = self.ctx.platform_shell(SHELL, args, self.test_config.shell_dir)
    output = None
    for i in range(3):  # Try 3 times in case of errors.
      cmd = self.ctx.command(
          cmd_prefix=self.test_config.command_prefix,
          test_case=TestListerDummy(self.suite),
          shell=shell,
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
      # When the command runs through another executable (e.g. iOS Simulator),
      # it is possible that the stdout will show something else besides the
      # actual test output, so it is necessary to harness this case by checking
      # whether the line exists here.
      if not line.strip().split():
        continue
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

  def _get_cmd_env(self):
    # FuzzTest uses this seed when running fuzz tests as normal gtests.
    # We derive this base64 encoded seed from our random seed.
    fuzztest_seed = base64.b64encode(str(self.random_seed).encode()).decode()
    return {'FUZZTEST_PRNG_SEED': fuzztest_seed}

  def get_android_resources(self):
    # Bytecode-generator tests are the only ones requiring extra files on
    # Android.
    parts = self.name.split('.')
    if parts[0] == 'BytecodeGeneratorTest':
      expectation_file = (
          self.suite.root / 'interpreter' / 'bytecode_expectations' /
          f'{parts[1]}.golden')
      if expectation_file.exists():
        return [expectation_file]
    return []
