# Copyright 2009 the V8 project authors. All rights reserved.
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
from os.path import join, exists
import sys
import test
import time


def GetSuite(name, root):
  # Not implemented.
  return None


class SputnikTestCase(test.TestCase):

  def __init__(self, case, path, context, mode):
    super(SputnikTestCase, self).__init__(context, path, mode)
    self.case = case
    self.tmpfile = None
    self.source = None

  def IsNegative(self):
    return '@negative' in self.GetSource()

  def IsFailureOutput(self, output):
    if output.exit_code != 0:
      return True
    out = output.stdout
    return "SputnikError" in out

  def BeforeRun(self):
    self.tmpfile = sputnik.TempFile(suffix='.js', prefix='sputnik-', text=True)
    self.tmpfile.Write(self.GetSource())
    self.tmpfile.Close()

  def AfterRun(self, result):
    # Dispose the temporary file if everything looks okay.
    if result is None or not result.HasPreciousOutput(): self.tmpfile.Dispose()
    self.tmpfile = None

  def GetCommand(self):
    result = self.context.GetVmCommand(self, self.mode)
    result.append(self.tmpfile.name)
    return result

  def GetLabel(self):
    return "%s sputnik %s" % (self.mode, self.GetName())

  def GetName(self):
    return self.path[-1]

  def GetSource(self):
    if not self.source:
      self.source = self.case.GetSource()
    return self.source

class SputnikTestConfiguration(test.TestConfiguration):

  def __init__(self, context, root):
    super(SputnikTestConfiguration, self).__init__(context, root)

  def ListTests(self, current_path, path, mode, variant_flags):
    # Import the sputnik test runner script as a module
    testroot = join(self.root, 'sputniktests')
    modroot = join(testroot, 'tools')
    sys.path.append(modroot)
    import sputnik
    globals()['sputnik'] = sputnik
    # Do not run strict mode tests yet. TODO(mmaly)
    test_suite = sputnik.TestSuite(testroot, False)
    test_suite.Validate()
    tests = test_suite.EnumerateTests([])
    result = []
    for test in tests:
      full_path = current_path + [test.GetPath()[-1]]
      if self.Contains(path, full_path):
        case = SputnikTestCase(test, full_path, self.context, mode)
        result.append(case)
    return result

  def GetBuildRequirements(self):
    return ['d8']

  def GetTestStatus(self, sections, defs):
    status_file = join(self.root, 'sputnik.status')
    if exists(status_file):
      test.ReadConfigurationInto(status_file, sections, defs)


def GetConfiguration(context, root):
  return SputnikTestConfiguration(context, root)
