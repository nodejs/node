# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase

FILES_PATTERN = re.compile(r"//\s+Files:(.*)")


class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_files(self):
    return {"test-api.js"}


class TestSuite(testsuite.TestSuite):
  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase


class TestCase(testcase.D8TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()
    self._source_files = self._parse_source_files(source)
    self._source_flags = self._parse_source_flags(source)

  def _parse_source_files(self, source):
    files_list = []  # List of file names to append to command arguments.
    files_match = FILES_PATTERN.search(source);
    # Accept several lines of 'Files:'.
    while True:
      if files_match:
        files_list += files_match.group(1).strip().split()
        files_match = FILES_PATTERN.search(source, files_match.end())
      else:
        break

    files = []
    files.append(self.suite.root.parent / "mjsunit" / "mjsunit.js")
    files.append(self.suite.root / "test-api.js")
    files.extend([self.suite.root.parents[1] / f for f in files_list])
    files.append(self._get_source_path())
    return files

  def _get_files_params(self):
    files = self._source_files
    if self.test_config.isolates:
      files = files + ['--isolate'] + files
    return files

  def _get_source_flags(self):
    return self._source_flags

  def _get_suite_flags(self):
    return ['--enable-inspector', '--allow-natives-syntax']

  def _get_source_path(self):
    # Try .js first, and fall back to .mjs.
    # TODO(v8:9406): clean this up by never separating the path from
    # the extension in the first place.
    js_file = self.suite.root / self.path_js
    if js_file.exists():
      return js_file
    return self.suite.root / self.path_mjs
