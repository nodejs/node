# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase

FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
MODULE_PATTERN = re.compile(r"^// MODULE$", flags=re.MULTILINE)


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
    files.append(os.path.normpath(os.path.join(
        self.suite.root, "..", "mjsunit", "mjsunit.js")))
    files.append(os.path.join(self.suite.root, "test-api.js"))
    files.extend([os.path.normpath(os.path.join(self.suite.root, '..', '..', f))
                  for f in files_list])
    if MODULE_PATTERN.search(source):
      files.append("--module")
    files.append(os.path.join(self.suite.root, self.path + self._get_suffix()))
    return files

  def _get_files_params(self):
    files = self._source_files
    if self._test_config.isolates:
      files = files + ['--isolate'] + files
    return files

  def _get_source_flags(self):
    return self._source_flags

  def _get_suite_flags(self):
    return ['--enable-inspector', '--allow-natives-syntax']

  def _get_source_path(self):
    return os.path.join(self.suite.root, self.path + self._get_suffix())


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
