# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase
from testrunner.outproc import webkit

FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")


class TestLoader(testsuite.JSTestLoader):
  @property
  def excluded_dirs(self):
    return {"resources"}


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
    files = [self.suite.root.parents[1] / f for f in files_list]
    testfilename = self.suite.root / self.path_js
    if SELF_SCRIPT_PATTERN.search(source):
      clean_name = testfilename.as_posix().replace("\\", "\\\\")
      env = ['-e', f'TEST_FILE_NAME="{clean_name}"']
      files = env + files
    files.append(self.suite.root / "resources/standalone-pre.js")
    files.append(testfilename)
    files.append(self.suite.root / "resources/standalone-post.js")
    return files

  def _get_files_params(self):
    files = self._source_files
    if self.test_config.isolates:
      files = files + ['--isolate'] + files
    return files

  def _get_source_flags(self):
    return self._source_flags

  def _get_source_path(self):
    return self.suite.root / self.path_js

  @property
  def output_proc(self):
    return webkit.OutProc(
        self.expected_outcomes,
        self.suite.root / self.path_and_suffix('-expected.txt'))
