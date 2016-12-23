# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os
import re

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase

FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")
PROTOCOL_TEST_JS = "protocol-test.js"
EXPECTED_SUFFIX = "-expected.txt"

class InspectorProtocolTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(InspectorProtocolTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(os.path.join(self.root), followlinks=True):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js") and filename != PROTOCOL_TEST_JS:
          fullpath = os.path.join(dirname, filename)
          relpath = fullpath[len(self.root) + 1 : -3]
          testname = relpath.replace(os.path.sep, "/")
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)
    flags_match = re.findall(FLAGS_PATTERN, source)
    flags = []
    for match in flags_match:
      flags += match.strip().split()
    testname = testcase.path.split(os.path.sep)[-1]
    testfilename = os.path.join(self.root, testcase.path + self.suffix())
    protocoltestfilename = os.path.join(self.root, PROTOCOL_TEST_JS)
    return [ protocoltestfilename, testfilename ] + flags

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()

  def shell(self):
    return "inspector-test"

  def _IgnoreLine(self, string):
    """Ignore empty lines, valgrind output and Android output."""
    if not string: return True
    return (string.startswith("==") or string.startswith("**") or
            string.startswith("ANDROID") or
            # FIXME(machenbach): The test driver shouldn't try to use slow
            # asserts if they weren't compiled. This fails in optdebug=2.
            string == "Warning: unknown flag --enable-slow-asserts." or
            string == "Try --help for options")

  def IsFailureOutput(self, testcase):
    file_name = os.path.join(self.root, testcase.path) + EXPECTED_SUFFIX
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
      lines = testcase.output.stdout.splitlines()
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
          # The next block of output lines starts after the separator.
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
  return InspectorProtocolTestSuite(name, root)
