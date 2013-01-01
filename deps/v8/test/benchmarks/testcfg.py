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
from os.path import join, split

def GetSuite(name, root):
  # Not implemented.
  return None


def IsNumber(string):
  try:
    float(string)
    return True
  except ValueError:
    return False


class BenchmarkTestCase(test.TestCase):

  def __init__(self, path, context, mode):
    super(BenchmarkTestCase, self).__init__(context, split(path), mode)
    self.root = path

  def GetLabel(self):
    return '%s benchmark %s' % (self.mode, self.GetName())

  def IsFailureOutput(self, output):
    if output.exit_code != 0:
      return True
    lines = output.stdout.splitlines()
    for line in lines:
      colon_index = line.find(':')
      if colon_index >= 0:
        if not IsNumber(line[colon_index+1:].strip()):
          return True
    return False

  def GetCommand(self):
    result = self.context.GetVmCommand(self, self.mode)
    result.append(join(self.root, 'run.js'))
    return result

  def GetName(self):
    return 'V8'

  def BeforeRun(self):
    os.chdir(self.root)

  def AfterRun(self, result):
    os.chdir(self.context.buildspace)

  def GetSource(self):
    return open(join(self.root, 'run.js')).read()

  def GetCustomFlags(self, mode):
    return []


class BenchmarkTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(BenchmarkTestConfiguration, self).__init__(context, root)

  def ListTests(self, current_path, path, mode, variant_flags):
    path = self.context.workspace
    path = join(path, 'benchmarks')
    test = BenchmarkTestCase(path, self.context, mode)
    return [test]

  def GetBuildRequirements(self):
    return ['d8']

  def GetTestStatus(self, sections, defs):
    pass

def GetConfiguration(context, root):
  return BenchmarkTestConfiguration(context, root)
