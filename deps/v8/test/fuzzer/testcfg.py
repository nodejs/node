# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase


class VariantsGenerator(testsuite.VariantsGenerator):
  def _get_variants(self, test):
    return self._standard_variant


class TestSuite(testsuite.TestSuite):
  SUB_TESTS = ( 'json', 'parser', 'regexp_builtins', 'regexp', 'multi_return', 'wasm',
          'wasm_async', 'wasm_code', 'wasm_compile')

  def ListTests(self):
    tests = []
    for subtest in TestSuite.SUB_TESTS:
      for fname in os.listdir(os.path.join(self.root, subtest)):
        if not os.path.isfile(os.path.join(self.root, subtest, fname)):
          continue
        test = self._create_test('%s/%s' % (subtest, fname))
        tests.append(test)
    tests.sort()
    return tests

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.TestCase):
  def _get_files_params(self):
    suite, name = self.path.split('/')
    return [os.path.join(self.suite.root, suite, name)]

  def _get_variant_flags(self):
    return []

  def _get_statusfile_flags(self):
    return []

  def _get_mode_flags(self):
    return []

  def get_shell(self):
    group, _ = self.path.split('/', 1)
    return 'v8_simple_%s_fuzzer' % group


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
