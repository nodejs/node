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
import shutil

from testrunner.local import commands
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase


class CcTestSuite(testsuite.TestSuite):

  def __init__(self, name, root):
    super(CcTestSuite, self).__init__(name, root)
    self.serdes_dir = os.path.normpath(
        os.path.join(root, "..", "..", "out", ".serdes"))
    if os.path.exists(self.serdes_dir):
      shutil.rmtree(self.serdes_dir, True)
    os.makedirs(self.serdes_dir)

  def ListTests(self, context):
    if utils.IsWindows():
      shell += '.exe'
    shell = os.path.abspath(os.path.join(context.shell_dir, self.shell()))
    output = commands.Execute([context.command_prefix,
                               shell,
                               '--list',
                               context.extra_flags])
    if output.exit_code != 0:
      print output.stdout
      print output.stderr
      return []
    tests = []
    for test_desc in output.stdout.strip().split():
      raw_test, dependency = test_desc.split('<')
      if dependency != '':
        dependency = raw_test.split('/')[0] + '/' + dependency
      else:
        dependency = None
      test = testcase.TestCase(self, raw_test, dependency=dependency)
      tests.append(test)
    tests.sort()
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    testname = testcase.path.split(os.path.sep)[-1]
    serialization_file = os.path.join(self.serdes_dir, "serdes_" + testname)
    serialization_file += ''.join(testcase.flags).replace('-', '_')
    return (testcase.flags + [testcase.path] + context.mode_flags +
            ["--testing_serialization_file=" + serialization_file])

  def shell(self):
    return "cctest"


def GetSuite(name, root):
  return CcTestSuite(name, root)


# Deprecated definitions below.
# TODO(jkummerow): Remove when SCons is no longer supported.


from os.path import exists, join, normpath
import test


class CcTestCase(test.TestCase):

  def __init__(self, path, executable, mode, raw_name, dependency, context, variant_flags):
    super(CcTestCase, self).__init__(context, path, mode)
    self.executable = executable
    self.raw_name = raw_name
    self.dependency = dependency
    self.variant_flags = variant_flags

  def GetLabel(self):
    return "%s %s %s" % (self.mode, self.path[-2], self.path[-1])

  def GetName(self):
    return self.path[-1]

  def BuildCommand(self, name):
    serialization_file = ''
    if exists(join(self.context.buildspace, 'obj', 'test', self.mode)):
      serialization_file = join('obj', 'test', self.mode, 'serdes')
    else:
      serialization_file = join('obj', 'serdes')
      if not exists(join(self.context.buildspace, 'obj')):
        os.makedirs(join(self.context.buildspace, 'obj'))
    serialization_file += '_' + self.GetName()
    serialization_file = join(self.context.buildspace, serialization_file)
    serialization_file += ''.join(self.variant_flags).replace('-', '_')
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

  def ListTests(self, current_path, path, mode, variant_flags):
    executable = 'cctest'
    if utils.IsWindows():
      executable += '.exe'
    executable = join(self.context.buildspace, executable)
    if not exists(executable):
      executable = join('obj', 'test', mode, 'cctest')
      if utils.IsWindows():
        executable += '.exe'
      executable = join(self.context.buildspace, executable)
    full_command = self.context.processor([executable, '--list'])
    output = test.Execute(full_command, self.context)
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
        result.append(CcTestCase(full_path, executable, mode, raw_test, dependency, self.context, variant_flags))
    result.sort()
    return result

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'cctest.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return CcTestConfiguration(context, root)
