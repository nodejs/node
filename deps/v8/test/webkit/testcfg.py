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

import itertools
import os
import re

from testrunner.local import testsuite
from testrunner.objects import testcase

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")
FILES_PATTERN = re.compile(r"//\s+Files:(.*)")
SELF_SCRIPT_PATTERN = re.compile(r"//\s+Env: TEST_FILE_NAME")


# TODO (machenbach): Share commonalities with mjstest.
class WebkitTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(WebkitTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      if 'resources' in dirs:
        dirs.remove('resources')

      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js"):
          testname = os.path.join(dirname[len(self.root) + 1:], filename[:-3])
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)
    flags = [] + context.mode_flags
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
      env = ["-e", "TEST_FILE_NAME=\"%s\"" % testfilename.replace("\\", "\\\\")]
      files = env + files
    files.append(os.path.join(self.root, "resources/standalone-pre.js"))
    files.append(testfilename)
    files.append(os.path.join(self.root, "resources/standalone-post.js"))

    flags += files
    if context.isolates:
      flags.append("--isolate")
      flags += files

    return testcase.flags + flags

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()

  # TODO(machenbach): Share with test/message/testcfg.py
  def _IgnoreLine(self, string):
    """Ignore empty lines, valgrind output and Android output."""
    if not string: return True
    return (string.startswith("==") or string.startswith("**") or
            string.startswith("ANDROID") or
            # These five patterns appear in normal Native Client output.
            string.startswith("DEBUG MODE ENABLED") or
            string.startswith("tools/nacl-run.py") or
            string.find("BYPASSING ALL ACL CHECKS") > 0 or
            string.find("Native Client module will be loaded") > 0 or
            string.find("NaClHostDescOpen:") > 0)

  def IsFailureOutput(self, output, testpath):
    if super(WebkitTestSuite, self).IsFailureOutput(output, testpath):
      return True
    file_name = os.path.join(self.root, testpath) + "-expected.txt"
    with file(file_name, "r") as expected:
      expected_lines = expected.readlines()

    def ExpIterator():
      for line in expected_lines:
        if line.startswith("#") or not line.strip(): continue
        yield line.strip()

    def ActIterator(lines):
      for line in lines:
        if self._IgnoreLine(line.strip()): continue
        yield line.strip()

    def ActBlockIterator():
      """Iterates over blocks of actual output lines."""
      lines = output.stdout.splitlines()
      start_index = 0
      found_eqeq = False
      for index, line in enumerate(lines):
        # If a stress test separator is found:
        if line.startswith("=="):
          # Iterate over all lines before a separator except the first.
          if not found_eqeq:
            found_eqeq = True
          else:
            yield ActIterator(lines[start_index:index])
          # The next block of ouput lines starts after the separator.
          start_index = index + 1
      # Iterate over complete output if no separator was found.
      if not found_eqeq:
        yield ActIterator(lines)

    for act_iterator in ActBlockIterator():
      for (expected, actual) in itertools.izip_longest(
          ExpIterator(), act_iterator, fillvalue=''):
        if expected != actual:
          return True
      return False


def GetSuite(name, root):
  return WebkitTestSuite(name, root)
