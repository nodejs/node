# Copyright 2011 the V8 project authors. All rights reserved.
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
from testrunner.local import utils
from testrunner.objects import testcase


class PreparserTestSuite(testsuite.TestSuite):
  def __init__(self, name, root):
    super(PreparserTestSuite, self).__init__(name, root)

  def shell(self):
    return "d8"

  def _GetExpectations(self):
    expects_file = os.path.join(self.root, "preparser.expectation")
    expectations_map = {}
    if not os.path.exists(expects_file): return expectations_map
    rule_regex = re.compile("^([\w\-]+)(?::([\w\-]+))?(?::(\d+),(\d+))?$")
    for line in utils.ReadLinesFrom(expects_file):
      rule_match = rule_regex.match(line)
      if not rule_match: continue
      expects = []
      if (rule_match.group(2)):
        expects += [rule_match.group(2)]
        if (rule_match.group(3)):
          expects += [rule_match.group(3), rule_match.group(4)]
      expectations_map[rule_match.group(1)] = " ".join(expects)
    return expectations_map

  def _ParsePythonTestTemplates(self, result, filename):
    pathname = os.path.join(self.root, filename + ".pyt")
    def Test(name, source, expectation):
      source = source.replace("\n", " ")
      testname = os.path.join(filename, name)
      flags = ["-e", source]
      if expectation:
        flags += ["--throws"]
      test = testcase.TestCase(self, testname, flags=flags)
      result.append(test)
    def Template(name, source):
      def MkTest(replacement, expectation):
        testname = name
        testsource = source
        for key in replacement.keys():
          testname = testname.replace("$" + key, replacement[key]);
          testsource = testsource.replace("$" + key, replacement[key]);
        Test(testname, testsource, expectation)
      return MkTest
    execfile(pathname, {"Test": Test, "Template": Template})

  def ListTests(self, context):
    expectations = self._GetExpectations()
    result = []

    # Find all .js files in this directory.
    filenames = [f[:-3] for f in os.listdir(self.root) if f.endswith(".js")]
    filenames.sort()
    for f in filenames:
      throws = expectations.get(f, None)
      flags = [f + ".js"]
      if throws:
        flags += ["--throws"]
      test = testcase.TestCase(self, f, flags=flags)
      result.append(test)

    # Find all .pyt files in this directory.
    filenames = [f[:-4] for f in os.listdir(self.root) if f.endswith(".pyt")]
    filenames.sort()
    for f in filenames:
      self._ParsePythonTestTemplates(result, f)
    return result

  def GetFlagsForTestCase(self, testcase, context):
    first = testcase.flags[0]
    if first != "-e":
      testcase.flags[0] = os.path.join(self.root, first)
    return testcase.flags

  def GetSourceForTest(self, testcase):
    if testcase.flags[0] == "-e":
      return testcase.flags[1]
    with open(testcase.flags[0]) as f:
      return f.read()

  def VariantFlags(self, testcase, default_flags):
    return [[]];


def GetSuite(name, root):
  return PreparserTestSuite(name, root)
