# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase

proposal_flags = [
    {
        'name': 'js-types',
        'flags': ['--experimental-wasm-type-reflection']
    },
    {
        'name': 'tail-call',
        'flags': ['--experimental-wasm-return-call']
    },
    {
        'name': 'memory64',
        'flags': ['--experimental-wasm-memory64']
    },
    {
        'name': 'extended-const',
        'flags': ['--experimental-wasm-extended-const']
    },
    {
        'name': 'function-references',
        # Some of these tests need `global.get` to be a constant instruction,
        # which is part of the GC proposal. We'll ship both proposals at once
        # anyway, so we might as well enable both for these tests.
        'flags': [
            '--experimental-wasm-typed-funcref', '--experimental-wasm-gc'
        ]
    },
    {
        'name': 'gc',
        'flags': ['--experimental-wasm-gc', '--wasm-final-types']
    },
    {
        'name': 'multi-memory',
        'flags': ['--experimental-wasm-multi-memory']
    },
]


class TestLoader(testsuite.JSTestLoader):
  pass

class TestSuite(testsuite.TestSuite):

  def __init__(self, ctx, *args, **kwargs):
    super(TestSuite, self).__init__(ctx, *args, **kwargs)
    self.test_root = self.root / "tests"
    self._test_loader.test_root = self.test_root

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase

class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    return [self.suite.test_root / self.path_js]

  def _get_source_flags(self):
    for proposal in proposal_flags:
      if f"proposals/{proposal['name']}" in self.name:
        return proposal['flags']
    return []
