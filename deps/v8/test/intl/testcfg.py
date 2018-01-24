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

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")

class IntlTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(IntlTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if (filename.endswith(".js") and filename != "assert.js" and
            filename != "utils.js" and filename != "regexp-assert.js" and
            filename != "regexp-prepare.js"):
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetParametersForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)
    flags = testcase.flags + ["--allow-natives-syntax"] + context.mode_flags
    flags_match = re.findall(FLAGS_PATTERN, source)
    for match in flags_match:
      flags += match.strip().split()

    files = []
    files.append(os.path.join(self.root, "assert.js"))
    files.append(os.path.join(self.root, "utils.js"))
    files.append(os.path.join(self.root, "regexp-prepare.js"))
    files.append(os.path.join(self.root, testcase.path + self.suffix()))
    files.append(os.path.join(self.root, "regexp-assert.js"))

    all_files = list(files)
    if context.isolates:
      all_files += ["--isolate"] + files

    return all_files, flags, {}

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()

def GetSuite(name, root):
  return IntlTestSuite(name, root)
