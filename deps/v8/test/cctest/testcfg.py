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

import test
import os
from os.path import join, dirname, exists
import platform
import utils


class CcTestCase(test.TestCase):

  def __init__(self, path, executable, mode, raw_name, dependency, context):
    super(CcTestCase, self).__init__(context, path, mode)
    self.executable = executable
    self.raw_name = raw_name
    self.dependency = dependency

  def GetLabel(self):
    return "%s %s %s" % (self.mode, self.path[-2], self.path[-1])

  def GetName(self):
    return self.path[-1]

  def BuildCommand(self, name):
    serialization_file = join('obj', 'test', self.mode, 'serdes')
    serialization_file += '_' + self.GetName()
    serialization_option = '--testing_serialization_file=' + serialization_file
    result = [ self.executable, name, serialization_option ]
    result += self.context.GetVmFlags(self, self.mode)
    return result

  def GetCommand(self):
    return self.BuildCommand(self.raw_name)

  def Run(self):
    if self.dependency != '':
      dependent_command = self.BuildCommand(self.dependency)
      output = self.RunCommand(dependent_command)
      if output.HasFailed():
        return output
    return test.TestCase.Run(self)


class CcTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(CcTestConfiguration, self).__init__(context, root)

  def GetBuildRequirements(self):
    return ['cctests']

  def ListTests(self, current_path, path, mode):
    executable = join('obj', 'test', mode, 'cctest')
    if utils.IsWindows():
      executable += '.exe'
    output = test.Execute([executable, '--list'], self.context)
    if output.exit_code != 0:
      print output.stdout
      print output.stderr
      return []
    result = []
    for test_desc in output.stdout.strip().split():
      raw_test, dependency = test_desc.split('<')
      relative_path = raw_test.split('/')
      full_path = current_path + relative_path
      if dependency != '':
        dependency = relative_path[0] + '/' + dependency
      if self.Contains(path, full_path):
        result.append(CcTestCase(full_path, executable, mode, raw_test, dependency, self.context))
    return result

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'cctest.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return CcTestConfiguration(context, root)
