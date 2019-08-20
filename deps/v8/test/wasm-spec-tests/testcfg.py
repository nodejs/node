# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from testrunner.local import testsuite
from testrunner.objects import testcase

proposal_flags = [{
                    'name': 'reference-types',
                    'flags': ['--experimental-wasm-anyref',
                               '--no-experimental-wasm-bulk-memory']
                  },
                  {
                    'name': 'bulk-memory-operations',
                    'flags': ['--experimental-wasm-bulk-memory']
                  }]

class TestLoader(testsuite.JSTestLoader):
  pass

class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def _get_files_params(self):
    return [os.path.join(self.suite.root, self.path + self._get_suffix())]

  def _get_source_flags(self):
    for proposal in proposal_flags:
      if os.sep.join(['proposals', proposal['name']]) in self.path:
        return proposal['flags']
    return []


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
