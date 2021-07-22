# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase

proposal_flags = [{
                    'name': 'js-types',
                    'flags': ['--experimental-wasm-type-reflection',
                              '--wasm-staging']
                  },
                  {
                    'name': 'tail-call',
                    'flags': ['--experimental-wasm-return-call',
                              '--wasm-staging']
                  },
                  {
                    'name': 'simd',
                    'flags': ['--experimental-wasm-simd',
                              '--wasm-staging']
                  },
                  {
                    'name': 'memory64',
                    'flags': ['--experimental-wasm-memory64',
                              '--wasm-staging']
                  },
                  ]

class TestLoader(testsuite.JSTestLoader):
  pass

class TestSuite(testsuite.TestSuite):
  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)
    self.test_root = os.path.join(self.root, "tests")
    self._test_loader.test_root = self.test_root

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase

class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    return [os.path.join(self.suite.test_root, self.path + self._get_suffix())]

  def _get_source_flags(self):
    for proposal in proposal_flags:
      if os.sep.join(['proposals', proposal['name']]) in self.path:
        return proposal['flags']
    return ['--experimental-wasm-reftypes']


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
