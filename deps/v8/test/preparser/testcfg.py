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

import test
import os
from os.path import join, dirname, exists, isfile
import platform
import utils
import re

class PreparserTestCase(test.TestCase):

  def __init__(self, root, path, executable, mode, throws, context, source):
    super(PreparserTestCase, self).__init__(context, path, mode)
    self.executable = executable
    self.root = root
    self.throws = throws
    self.source = source

  def GetLabel(self):
    return "%s %s %s" % (self.mode, self.path[-2], self.path[-1])

  def GetName(self):
    return self.path[-1]

  def HasSource(self):
    return self.source is not None

  def GetSource():
    return self.source

  def BuildCommand(self, path):
    if (self.source is not None):
      result = [self.executable, "-e", self.source]
    else:
      testfile = join(self.root, self.GetName()) + ".js"
      result = [self.executable, testfile]
    if (self.throws):
      result += ['throws'] + self.throws
    return result

  def GetCommand(self):
    return self.BuildCommand(self.path)

  def Run(self):
    return test.TestCase.Run(self)


class PreparserTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(PreparserTestConfiguration, self).__init__(context, root)

  def GetBuildRequirements(self):
    return ['preparser']

  def GetExpectations(self):
    expects_file = join(self.root, 'preparser.expectation')
    map = {}
    if exists(expects_file):
      rule_regex = re.compile("^([\w\-]+)(?::([\w\-]+))?(?::(\d+),(\d+))?$")
      for line in utils.ReadLinesFrom(expects_file):
        if (line[0] == '#'): continue
        rule_match = rule_regex.match(line)
        if rule_match:
          expects = []
          if (rule_match.group(2)):
            expects = expects + [rule_match.group(2)]
            if (rule_match.group(3)):
              expects = expects + [rule_match.group(3), rule_match.group(4)]
          map[rule_match.group(1)] = expects
    return map;

  def ParsePythonTestTemplates(self, result, filename,
                               executable, current_path, mode):
    pathname = join(self.root, filename + ".pyt")
    def Test(name, source, expectation):
      throws = None
      if (expectation is not None):
        throws = [expectation]
      test = PreparserTestCase(self.root,
                               current_path + [filename, name],
                               executable,
                               mode, throws, self.context,
                               source.replace("\n", " "))
      result.append(test)
    def Template(name, source):
      def MkTest(replacement, expectation):
        testname = name
        testsource = source
        for key in replacement.keys():
          testname = testname.replace("$"+key, replacement[key]);
          testsource = testsource.replace("$"+key, replacement[key]);
        Test(testname, testsource, expectation)
      return MkTest
    execfile(pathname, {"Test": Test, "Template": Template})

  def ListTests(self, current_path, path, mode, variant_flags):
    executable = 'preparser'
    if utils.IsWindows():
      executable += '.exe'
    executable = join(self.context.buildspace, executable)
    if not isfile(executable):
      executable = join('obj', 'preparser', mode, 'preparser')
      if utils.IsWindows():
        executable += '.exe'
      executable = join(self.context.buildspace, executable)
    expectations = self.GetExpectations()
    result = []
    # Find all .js files in tests/preparser directory.
    filenames = [f[:-3] for f in os.listdir(self.root) if f.endswith(".js")]
    filenames.sort()
    for file in filenames:
      throws = None;
      if (file in expectations):
        throws = expectations[file]
      result.append(PreparserTestCase(self.root,
                                      current_path + [file], executable,
                                      mode, throws, self.context, None))
    # Find all .pyt files in test/preparser directory.
    filenames = [f[:-4] for f in os.listdir(self.root) if f.endswith(".pyt")]
    filenames.sort()
    for file in filenames:
      # Each file as a python source file to be executed in a specially
      # created environment (defining the Template and Test functions)
      self.ParsePythonTestTemplates(result, file,
                                    executable, current_path, mode)
    return result

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'preparser.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)

  def VariantFlags(self):
    return [[]];


def GetConfiguration(context, root):
  return PreparserTestConfiguration(context, root)
