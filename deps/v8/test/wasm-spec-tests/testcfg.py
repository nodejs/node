# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from testrunner.local import testsuite
from testrunner.objects import testcase

# We use the 'wasm-3.0' branch on the main spec repo, so enable all proposals
# that were merged into that branch.
default_flags = []

proposal_flags = [{
    'name': 'tail-call',
    'flags': []
}, {
    'name': 'memory64',
    'flags': []
}, {
    'name': 'extended-const',
    'flags': []
}, {
    'name': 'function-references',
    'flags': []
}, {
    'name': 'gc',
    'flags': []
}, {
    'name': 'multi-memory',
    'flags': []
}, {
    'name': 'exception-handling',
    'flags': []
}, {
    'name': 'js-promise-integration',
    'flags': []
}, {
    'name': 'stack-switching',
    'flags': ['--experimental-wasm-wasmfx']
}, {
    'name': 'custom-descriptors',
    'flags': ['--experimental-wasm-custom-descriptors']
}]


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
    return default_flags
