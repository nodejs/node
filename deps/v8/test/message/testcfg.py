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

import itertools
import os
import re

from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


FLAGS_PATTERN = re.compile(r"//\s+Flags:(.*)")


class MessageTestSuite(testsuite.TestSuite):
  def __init__(self, name, root):
    super(MessageTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    tests = []
    for dirname, dirs, files in os.walk(self.root):
      for dotted in [x for x in dirs if x.startswith('.')]:
        dirs.remove(dotted)
      dirs.sort()
      files.sort()
      for filename in files:
        if filename.endswith(".js"):
          testname = join(dirname[len(self.root) + 1:], filename[:-3])
          test = testcase.TestCase(self, testname)
          tests.append(test)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    source = self.GetSourceForTest(testcase)
    result = []
    flags_match = re.findall(FLAGS_PATTERN, source)
    for match in flags_match:
      result += match.strip().split()
    result += context.mode_flags
    result.append(os.path.join(self.root, testcase.path + ".js"))
    return testcase.flags + result

  def GetSourceForTest(self, testcase):
    filename = os.path.join(self.root, testcase.path + self.suffix())
    with open(filename) as f:
      return f.read()

  def _IgnoreLine(self, string):
    """Ignore empty lines, valgrind output and Android output."""
    if not string: return True
    return (string.startswith("==") or string.startswith("**") or
            string.startswith("ANDROID"))

  def IsFailureOutput(self, output, testpath):
    expected_path = os.path.join(self.root, testpath + ".out")
    expected_lines = []
    # Can't use utils.ReadLinesFrom() here because it strips whitespace.
    with open(expected_path) as f:
      for line in f:
        if line.startswith("#") or not line.strip(): continue
        expected_lines.append(line)
    raw_lines = output.stdout.splitlines()
    actual_lines = [ s for s in raw_lines if not self._IgnoreLine(s) ]
    env = { "basename": os.path.basename(testpath + ".js") }
    if len(expected_lines) != len(actual_lines):
      return True
    for (expected, actual) in itertools.izip(expected_lines, actual_lines):
      pattern = re.escape(expected.rstrip() % env)
      pattern = pattern.replace("\\*", ".*")
      pattern = "^%s$" % pattern
      if not re.match(pattern, actual):
        return True
    return False

  def StripOutputForTransmit(self, testcase):
    pass


def GetSuite(name, root):
  return MessageTestSuite(name, root)


# Deprecated definitions below.
# TODO(jkummerow): Remove when SCons is no longer supported.


import test
from os.path import join, exists, basename, isdir

class MessageTestCase(test.TestCase):

  def __init__(self, path, file, expected, mode, context, config):
    super(MessageTestCase, self).__init__(context, path, mode)
    self.file = file
    self.expected = expected
    self.config = config

  def IgnoreLine(self, str):
    """Ignore empty lines, valgrind output and Android output."""
    if not str: return True
    return (str.startswith('==') or str.startswith('**') or
            str.startswith('ANDROID'))

  def IsFailureOutput(self, output):
    f = file(self.expected)
    # Skip initial '#' comment and spaces
    for line in f:
      if (not line.startswith('#')) and (not line.strip()):
        break
    # Convert output lines to regexps that we can match
    env = { 'basename': basename(self.file) }
    patterns = [ ]
    for line in f:
      if not line.strip():
        continue
      pattern = re.escape(line.rstrip() % env)
      pattern = pattern.replace('\\*', '.*')
      pattern = '^%s$' % pattern
      patterns.append(pattern)
    # Compare actual output with the expected
    raw_lines = output.stdout.splitlines()
    outlines = [ s for s in raw_lines if not self.IgnoreLine(s) ]
    if len(outlines) != len(patterns):
      return True
    for i in xrange(len(patterns)):
      if not re.match(patterns[i], outlines[i]):
        return True
    return False

  def GetLabel(self):
    return "%s %s" % (self.mode, self.GetName())

  def GetName(self):
    return self.path[-1]

  def GetCommand(self):
    result = self.config.context.GetVmCommand(self, self.mode)
    source = open(self.file).read()
    flags_match = re.findall(FLAGS_PATTERN, source)
    for match in flags_match:
      result += match.strip().split()
    result.append(self.file)
    return result

  def GetSource(self):
    return (open(self.file).read()
          + "\n--- expected output ---\n"
          + open(self.expected).read())


class MessageTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(MessageTestConfiguration, self).__init__(context, root)

  def Ls(self, path):
    if isdir(path):
        return [f[:-3] for f in os.listdir(path) if f.endswith('.js')]
    else:
        return []

  def ListTests(self, current_path, path, mode, variant_flags):
    mjsunit = [current_path + [t] for t in self.Ls(self.root)]
    regress = [current_path + ['regress', t] for t in self.Ls(join(self.root, 'regress'))]
    bugs = [current_path + ['bugs', t] for t in self.Ls(join(self.root, 'bugs'))]
    mjsunit.sort()
    regress.sort()
    bugs.sort()
    all_tests = mjsunit + regress + bugs
    result = []
    for test in all_tests:
      if self.Contains(path, test):
        file_prefix = join(self.root, reduce(join, test[1:], ""))
        file_path = file_prefix + ".js"
        output_path = file_prefix + ".out"
        if not exists(output_path):
          print "Could not find %s" % output_path
          continue
        result.append(MessageTestCase(test, file_path, output_path, mode,
                                      self.context, self))
    return result

  def GetBuildRequirements(self):
    return ['d8']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'message.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return MessageTestConfiguration(context, root)
