# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase

ANY_JS = ".any.js"
WPT_ROOT = "/wasm/jsapi/"
META_SCRIPT_REGEXP = re.compile(r"META:\s*script=(.*)")
META_TIMEOUT_REGEXP = re.compile(r"META:\s*timeout=(.*)")

proposal_flags = [{
                    'name': 'reference-types',
                    'flags': ['--experimental-wasm-reftypes',
                              '--no-experimental-wasm-bulk-memory',
                              '--wasm-staging']
                  },
                  {
                    'name': 'bulk-memory-operations',
                    'flags': ['--experimental-wasm-bulk-memory',
                              '--wasm-staging']
                  },
                  {
                    'name': 'js-types',
                    'flags': ['--experimental-wasm-type-reflection',
                              '--wasm-staging']
                  },
                  {
                    'name': 'tail-call',
                    'flags': ['--experimental-wasm-tail-call',
                              '--wasm-staging']
                  },
                  {
                    'name': 'simd',
                    'flags': ['--experimental-wasm-simd',
                              '--wasm-staging']
                  },
                  ]


class TestLoader(testsuite.JSTestLoader):
  @property
  def extensions(self):
    return [ANY_JS]


class TestSuite(testsuite.TestSuite):
  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)
    self.mjsunit_js = os.path.join(os.path.dirname(self.root), "mjsunit",
                                   "mjsunit.js")
    self.test_root = os.path.join(self.root, "tests")
    self._test_loader.test_root = self.test_root

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase

def get_proposal_path_identifier(proposal):
  return os.sep.join(['proposals', proposal['name']])

class TestCase(testcase.D8TestCase):
  def _get_timeout_param(self):
    source = self.get_source()
    timeout_params = META_TIMEOUT_REGEXP.findall(source)
    if not timeout_params:
      return None

    if timeout_params[0] in ["long"]:
      return timeout_params[0]
    else:
      print("unknown timeout param %s in %s%s"
            % (timeout_params[0], self.path, ANY_JS))
      return None

  def _get_files_params(self):
    files = [self.suite.mjsunit_js,
             os.path.join(self.suite.root, "third_party", "testharness.js"),
             os.path.join(self.suite.root, "testharness-additions.js"),
             os.path.join(self.suite.root, "report.js")]

    source = self.get_source()
    current_dir = os.path.dirname(self._get_source_path())
    for script in META_SCRIPT_REGEXP.findall(source):
      if script.startswith(WPT_ROOT):
        # Matched an absolute path, strip the root and replace it with our
        # local root.
        found = False
        for proposal in proposal_flags:
          if get_proposal_path_identifier(proposal) in current_dir:
            found = True
            script = os.path.join(self.suite.test_root,
                                  os.sep.join(['proposals', proposal['name']]),
                                  script[len(WPT_ROOT):])
        if not found:
          script = os.path.join(self.suite.test_root, script[len(WPT_ROOT):])
      elif not script.startswith("/"):
        # Matched a relative path, prepend this test's directory.
        script = os.path.join(current_dir, script)
      else:
        raise Exception("Unexpected absolute path for script: \"%s\"" % script);

      files.append(script)

    files.extend([self._get_source_path(),
                  os.path.join(self.suite.root, "after.js")])
    return files

  def _get_source_flags(self):
    for proposal in proposal_flags:
      if get_proposal_path_identifier(proposal) in self.path:
        return proposal['flags']
    return ['--wasm-staging']

  def _get_source_path(self):
    # All tests are named `path/name.any.js`
    return os.path.join(self.suite.test_root, self.path + ANY_JS)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
