# Copyright 2008 the V8 project authors. All rights reserved.
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

FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
ENV_PATTERN = re.compile(r"//\s+Environment Variables:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")
MODULE_PATTERN = re.compile(r"^// MODULE$", flags=re.MULTILINE)
NO_HARNESS_PATTERN = re.compile(r"^// NO HARNESS$", flags=re.MULTILINE)


class TestSuite(testsuite.TestSuite):
  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root, followlinks=True):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js") and filename != "mjsunit.js":
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = self._create_test(testname)
          tests.append(test)
    return tests

  def _test_class(self):
    return TestCase


class TestCase(testcase.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()

    files_list = []  # List of file names to append to command arguments.
    files_match = FILES_PATTERN.search(source);
    # Accept several lines of 'Files:'.
    while True:
      if files_match:
        files_list += files_match.group(1).strip().split()
        files_match = FILES_PATTERN.search(source, files_match.end())
      else:
        break
    files = [ os.path.normpath(os.path.join(self.suite.root, '..', '..', f))
              for f in files_list ]
    testfilename = os.path.join(self.suite.root,
                                self.path + self._get_suffix())
    if SELF_SCRIPT_PATTERN.search(source):
      files = (
        ["-e", "TEST_FILE_NAME=\"%s\"" % testfilename.replace("\\", "\\\\")] +
        files)

    if NO_HARNESS_PATTERN.search(source):
      mjsunit_files = []
    else:
      mjsunit_files = [os.path.join(self.suite.root, "mjsunit.js")]

    files_suffix = []
    if MODULE_PATTERN.search(source):
      files_suffix.append("--module")
    files_suffix.append(testfilename)

    self._source_files = files
    self._source_flags = self._parse_source_flags(source)
    self._mjsunit_files = mjsunit_files
    self._files_suffix = files_suffix
    self._env = self._parse_source_env(source)

  def _parse_source_env(self, source):
    env_match = ENV_PATTERN.search(source)
    env = {}
    if env_match:
      for env_pair in env_match.group(1).strip().split():
        var, value = env_pair.split('=')
        env[var] = value
    return env

  def _get_source_flags(self):
    return self._source_flags

  def _get_files_params(self, ctx):
    files = list(self._source_files)
    if not ctx.no_harness:
      files += self._mjsunit_files
    files += self._files_suffix
    if ctx.isolates:
      files += ['--isolate'] + files

    return files

  def _get_cmd_env(self):
    return self._env

  def _get_source_path(self):
    return os.path.join(self.suite.root, self.path + self._get_suffix())


def GetSuite(name, root):
  return TestSuite(name, root)
