# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase


class FuzzerVariantGenerator(testsuite.VariantGenerator):
  # Only run the fuzzer with standard variant.
  def FilterVariantsByTest(self, testcase):
    return self.standard_variant

  def GetFlagSets(self, testcase, variant):
    return testsuite.FAST_VARIANT_FLAGS[variant]


class FuzzerTestSuite(testsuite.TestSuite):
  SUB_TESTS = ( 'json', 'parser', 'regexp', 'wasm', 'wasm_asmjs', 'wasm_call',
          'wasm_code', 'wasm_compile', 'wasm_data_section',
          'wasm_function_sigs_section', 'wasm_globals_section',
          'wasm_imports_section', 'wasm_memory_section', 'wasm_names_section',
          'wasm_types_section' )

  def __init__(self, name, root):
    super(FuzzerTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for subtest in FuzzerTestSuite.SUB_TESTS:
      shell = 'v8_simple_%s_fuzzer' % subtest
      for fname in os.listdir(os.path.join(self.root, subtest)):
        if subtest in ["wasm", "wasm_asmjs"] and fname.endswith(".wasm"):
          os.remove(os.path.join(self.root, subtest, fname))
          continue
        if not os.path.isfile(os.path.join(self.root, subtest, fname)):
          continue
        test = testcase.TestCase(self, '%s/%s' % (subtest, fname),
                                 override_shell=shell)
        tests.append(test)
    tests.sort()
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    suite, name = testcase.path.split('/')
    return [os.path.join(self.root, suite, name)]

  def _VariantGeneratorFactory(self):
    return FuzzerVariantGenerator


def GetSuite(name, root):
  return FuzzerTestSuite(name, root)
