# Copyright 2012 the V8 project authors. All rights reserved.
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


import imp
import os

from . import statusfile

class TestSuite(object):

  @staticmethod
  def LoadTestSuite(root):
    name = root.split(os.path.sep)[-1]
    f = None
    try:
      (f, pathname, description) = imp.find_module("testcfg", [root])
      module = imp.load_module("testcfg", f, pathname, description)
      suite = module.GetSuite(name, root)
    finally:
      if f:
        f.close()
    return suite

  def __init__(self, name, root):
    self.name = name  # string
    self.root = root  # string containing path
    self.tests = None  # list of TestCase objects
    self.rules = None  # dictionary mapping test path to list of outcomes
    self.wildcards = None  # dictionary mapping test paths to list of outcomes
    self.total_duration = None  # float, assigned on demand

  def shell(self):
    return "d8"

  def suffix(self):
    return ".js"

  def status_file(self):
    return "%s/%s.status" % (self.root, self.name)

  # Used in the status file and for stdout printing.
  def CommonTestName(self, testcase):
    return testcase.path

  def ListTests(self, context):
    raise NotImplementedError

  def VariantFlags(self):
    return None

  def DownloadData(self):
    pass

  def ReadStatusFile(self, variables):
    (self.rules, self.wildcards) = \
        statusfile.ReadStatusFile(self.status_file(), variables)

  def ReadTestCases(self, context):
    self.tests = self.ListTests(context)

  def FilterTestCasesByStatus(self, warn_unused_rules):
    filtered = []
    used_rules = set()
    for t in self.tests:
      testname = self.CommonTestName(t)
      if testname in self.rules:
        used_rules.add(testname)
        outcomes = self.rules[testname]
        t.outcomes = outcomes  # Even for skipped tests, as the TestCase
        # object stays around and PrintReport() uses it.
        if statusfile.DoSkip(outcomes):
          continue  # Don't add skipped tests to |filtered|.
      if len(self.wildcards) != 0:
        skip = False
        for rule in self.wildcards:
          assert rule[-1] == '*'
          if testname.startswith(rule[:-1]):
            used_rules.add(rule)
            outcomes = self.wildcards[rule]
            t.outcomes = outcomes
            if statusfile.DoSkip(outcomes):
              skip = True
              break  # "for rule in self.wildcards"
        if skip: continue  # "for t in self.tests"
      filtered.append(t)
    self.tests = filtered

    if not warn_unused_rules:
      return

    for rule in self.rules:
      if rule not in used_rules:
        print("Unused rule: %s -> %s" % (rule, self.rules[rule]))
    for rule in self.wildcards:
      if rule not in used_rules:
        print("Unused rule: %s -> %s" % (rule, self.wildcards[rule]))

  def FilterTestCasesByArgs(self, args):
    filtered = []
    filtered_args = []
    for a in args:
      argpath = a.split(os.path.sep)
      if argpath[0] != self.name:
        continue
      if len(argpath) == 1 or (len(argpath) == 2 and argpath[1] == '*'):
        return  # Don't filter, run all tests in this suite.
      path = os.path.sep.join(argpath[1:])
      if path[-1] == '*':
        path = path[:-1]
      filtered_args.append(path)
    for t in self.tests:
      for a in filtered_args:
        if t.path.startswith(a):
          filtered.append(t)
          break
    self.tests = filtered

  def GetFlagsForTestCase(self, testcase, context):
    raise NotImplementedError

  def GetSourceForTest(self, testcase):
    return "(no source available)"

  def IsFailureOutput(self, output, testpath):
    return output.exit_code != 0

  def IsNegativeTest(self, testcase):
    return False

  def HasFailed(self, testcase):
    execution_failed = self.IsFailureOutput(testcase.output, testcase.path)
    if self.IsNegativeTest(testcase):
      return not execution_failed
    else:
      return execution_failed

  def HasUnexpectedOutput(self, testcase):
    if testcase.output.HasCrashed():
      outcome = statusfile.CRASH
    elif testcase.output.HasTimedOut():
      outcome = statusfile.TIMEOUT
    elif self.HasFailed(testcase):
      outcome = statusfile.FAIL
    else:
      outcome = statusfile.PASS
    if not testcase.outcomes:
      return outcome != statusfile.PASS
    return not outcome in testcase.outcomes

  def StripOutputForTransmit(self, testcase):
    if not self.HasUnexpectedOutput(testcase):
      testcase.output.stdout = ""
      testcase.output.stderr = ""

  def CalculateTotalDuration(self):
    self.total_duration = 0.0
    for t in self.tests:
      self.total_duration += t.duration
    return self.total_duration
