# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from pathlib import Path

from testrunner.local import testsuite
from testrunner.objects import testcase

ANY_JS = ".any.js"
WPT_ROOT = "/wasm/jsapi/"
META_SCRIPT_REGEXP = re.compile(r"META:\s*script=(.*)")
META_TIMEOUT_REGEXP = re.compile(r"META:\s*timeout=(.*)")

# Flags per Wasm proposal.
proposal_flags = {
    # currently none; if needed add entries in this form:
    # 'exception-handling': ['--experimental-wasm-exnref'],
}

# Flags per WPT subdirectory.
wpt_flags = {'memory': ['--experimental-wasm-rab-integration']}


class TestLoader(testsuite.JSTestLoader):
  @property
  def extensions(self):
    return [ANY_JS]


class TestSuite(testsuite.TestSuite):

  def __init__(self, ctx, *args, **kwargs):
    super(TestSuite, self).__init__(ctx, *args, **kwargs)
    self.mjsunit_js = self.root.parent / "mjsunit" / "mjsunit.js"
    self.test_root = self.root / "tests"
    self._test_loader.test_root = self.test_root

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


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
             self.suite.root / "third_party" / "testharness.js",
             self.suite.root / "testharness-additions.js",
             self.suite.root / "report.js"]

    source = self.get_source()
    current_dir = self._get_source_path().parent
    for script in META_SCRIPT_REGEXP.findall(source):
      if script.startswith(WPT_ROOT):
        # Matched an absolute path, strip the root and replace it with our
        # local root.
        script_suffix = script[len(WPT_ROOT):]
        subdirs = []
        # If the test is in the 'proposals' directory, load relative to that
        # proposal.
        if self.path.parts[0] == 'proposals':
          subdirs = self.path.parts[0:2]
        # Similarly, load from the 'wpt' directory for wpt tests.
        elif self.path.parts[0] == 'wpt':
          subdirs = ['wpt']
        script = self.suite.test_root.joinpath(*subdirs) / script_suffix
      elif not Path(script).is_absolute():
        # Matched a relative path, prepend this test's directory.
        script = current_dir / script
      else:
        raise Exception(f"Unexpected absolute path for script: \"{script}\"")

      files.append(script)

    files.extend([self._get_source_path(), self.suite.root / "after.js"])
    return files

  def _get_source_flags(self):
    if self.path.parts[0] == 'proposals':
      proposal_name = self.path.parts[1]
      if proposal_name in proposal_flags:
        return proposal_flags[proposal_name]
    if self.path.parts[0] == 'wpt':
      wpt_subdir = self.path.parts[1]
      if wpt_subdir in wpt_flags:
        return wpt_flags[wpt_subdir]
    return ['--wasm-staging']

  def _get_source_path(self):
    # All tests are named `path/name.any.js`
    return self.suite.test_root / self.path_and_suffix(ANY_JS)
