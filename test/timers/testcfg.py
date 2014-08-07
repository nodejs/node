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
import shutil
from shutil import rmtree
from os import mkdir
from glob import glob
from os.path import join, dirname, exists
import re
import shlex

FAKETIME_FLAGS_PATTERN = re.compile(r"//\s+FaketimeFlags:(.*)")
FAKETIME_BIN_PATH = os.path.join("tools", "faketime", "src", "faketime")

class TimersTestCase(test.TestCase):

  def __init__(self, path, file, mode, context, config):
    super(TimersTestCase, self).__init__(context, path, mode)
    self.file = file
    self.config = config
    self.mode = mode

  def GetLabel(self):
    return "%s %s" % (self.mode, self.GetName())

  def GetName(self):
    return self.path[-1]

  def GetCommand(self):
    result = [FAKETIME_BIN_PATH];

    source = open(self.file).read()
    faketime_flags_match = FAKETIME_FLAGS_PATTERN.search(source)
    if faketime_flags_match:
      result += shlex.split(faketime_flags_match.group(1).strip())

    result += [self.config.context.GetVm(self.mode)]
    result += [self.file]

    return result

  def GetSource(self):
    return open(self.file).read()


class TimersTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(TimersTestConfiguration, self).__init__(context, root)

  def Ls(self, path):
    def SelectTest(name):
      return name.startswith('test-') and name.endswith('.js')
    return [f[:-3] for f in os.listdir(path) if SelectTest(f)]

  def ListTests(self, current_path, path, mode):
    all_tests = [current_path + [t] for t in self.Ls(join(self.root))]
    result = []
    for test in all_tests:
      if self.Contains(path, test):
        file_path = join(self.root, reduce(join, test[1:], "") + ".js")
        result.append(TimersTestCase(test, file_path, mode, self.context, self))
    return result

  def GetBuildRequirements(self):
    return ['sample', 'sample=shell']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'simple.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)



def GetConfiguration(context, root):
  return TimersTestConfiguration(context, root)
