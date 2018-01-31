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

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")
FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
ENV_PATTERN = re.compile(r"//\s+Environment Variables:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")
MODULE_PATTERN = re.compile(r"^// MODULE$", flags=re.MULTILINE)
NO_HARNESS_PATTERN = re.compile(r"^// NO HARNESS$", flags=re.MULTILINE)


class MjsunitTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(MjsunitTestSuite, self).__init__(name, root)

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
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetParametersForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)

    flags = testcase.flags + context.mode_flags
    env = self._get_env(source)

    flags_match = re.findall(FLAGS_PATTERN, source)
    for match in flags_match:
      flags += match.strip().split()

    files_list = []  # List of file names to append to command arguments.
    files_match = FILES_PATTERN.search(source);
    # Accept several lines of 'Files:'.
    while True:
      if files_match:
        files_list += files_match.group(1).strip().split()
        files_match = FILES_PATTERN.search(source, files_match.end())
      else:
        break
    files = [ os.path.normpath(os.path.join(self.root, '..', '..', f))
              for f in files_list ]
    testfilename = os.path.join(self.root, testcase.path + self.suffix())
    if SELF_SCRIPT_PATTERN.search(source):
      files = (
        ["-e", "TEST_FILE_NAME=\"%s\"" % testfilename.replace("\\", "\\\\")] +
        files)

    if not context.no_harness and not NO_HARNESS_PATTERN.search(source):
      files.append(os.path.join(self.root, "mjsunit.js"))

    if MODULE_PATTERN.search(source):
      files.append("--module")
    files.append(testfilename)

    all_files = list(files)
    if context.isolates:
      all_files += ["--isolate"] + files

    return all_files, flags, env

  def _get_env(self, source):
    env_match = ENV_PATTERN.search(source)
    env = {}
    if env_match:
      for env_pair in env_match.group(1).strip().split():
        var, value = env_pair.split('=')
        env[var] = value
    return env

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()


def GetSuite(name, root):
  return MjsunitTestSuite(name, root)
