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

from . import commands
from . import statusfile
from . import utils
from ..objects import testcase
from variants import ALL_VARIANTS, ALL_VARIANT_FLAGS, FAST_VARIANT_FLAGS


FAST_VARIANTS = set(["default", "turbofan"])
STANDARD_VARIANT = set(["default"])


class VariantGenerator(object):
  def __init__(self, suite, variants):
    self.suite = suite
    self.all_variants = ALL_VARIANTS & variants
    self.fast_variants = FAST_VARIANTS & variants
    self.standard_variant = STANDARD_VARIANT & variants

  def FilterVariantsByTest(self, testcase):
    result = self.all_variants
    if testcase.outcomes:
      if statusfile.OnlyStandardVariant(testcase.outcomes):
        return self.standard_variant
      if statusfile.OnlyFastVariants(testcase.outcomes):
        result = self.fast_variants
    return result

  def GetFlagSets(self, testcase, variant):
    if testcase.outcomes and statusfile.OnlyFastVariants(testcase.outcomes):
      return FAST_VARIANT_FLAGS[variant]
    else:
      return ALL_VARIANT_FLAGS[variant]


class TestSuite(object):

  @staticmethod
  def LoadTestSuite(root, global_init=True):
    name = root.split(os.path.sep)[-1]
    f = None
    try:
      (f, pathname, description) = imp.find_module("testcfg", [root])
      module = imp.load_module(name + "_testcfg", f, pathname, description)
      return module.GetSuite(name, root)
    except ImportError:
      # Use default if no testcfg is present.
      return GoogleTestSuite(name, root)
    finally:
      if f:
        f.close()

  def __init__(self, name, root):
    # Note: This might be called concurrently from different processes.
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
    if utils.IsWindows():
      return testcase.path.replace("\\", "/")
    else:
      return testcase.path

  def ListTests(self, context):
    raise NotImplementedError

  def _VariantGeneratorFactory(self):
    """The variant generator class to be used."""
    return VariantGenerator

  def CreateVariantGenerator(self, variants):
    """Return a generator for the testing variants of this suite.

    Args:
      variants: List of variant names to be run as specified by the test
                runner.
    Returns: An object of type VariantGenerator.
    """
    return self._VariantGeneratorFactory()(self, set(variants))

  def PrepareSources(self):
    """Called once before multiprocessing for doing file-system operations.

    This should not access the network. For network access use the method
    below.
    """
    pass

  def DownloadData(self):
    pass

  def ReadStatusFile(self, variables):
    with open(self.status_file()) as f:
      self.rules, self.wildcards = (
          statusfile.ReadStatusFile(f.read(), variables))

  def ReadTestCases(self, context):
    self.tests = self.ListTests(context)

  @staticmethod
  def _FilterSlow(slow, mode):
    return (mode == "run" and not slow) or (mode == "skip" and slow)

  @staticmethod
  def _FilterPassFail(pass_fail, mode):
    return (mode == "run" and not pass_fail) or (mode == "skip" and pass_fail)

  def FilterTestCasesByStatus(self, warn_unused_rules,
                              slow_tests="dontcare",
                              pass_fail_tests="dontcare",
                              variants=False):

    # Use only variants-dependent rules and wildcards when filtering
    # respective test cases and generic rules when filtering generic test
    # cases.
    if not variants:
      rules = self.rules[""]
      wildcards = self.wildcards[""]
    else:
      # We set rules and wildcards to a variant-specific version for each test
      # below.
      rules = {}
      wildcards = {}

    filtered = []

    # Remember used rules as tuples of (rule, variant), where variant is "" for
    # variant-independent rules.
    used_rules = set()

    for t in self.tests:
      slow = False
      pass_fail = False
      testname = self.CommonTestName(t)
      variant = t.variant or ""
      if variants:
        rules = self.rules[variant]
        wildcards = self.wildcards[variant]
      if testname in rules:
        used_rules.add((testname, variant))
        # Even for skipped tests, as the TestCase object stays around and
        # PrintReport() uses it.
        t.outcomes = t.outcomes | rules[testname]
        if statusfile.DoSkip(t.outcomes):
          continue  # Don't add skipped tests to |filtered|.
        for outcome in t.outcomes:
          if outcome.startswith('Flags: '):
            t.flags += outcome[7:].split()
        slow = statusfile.IsSlow(t.outcomes)
        pass_fail = statusfile.IsPassOrFail(t.outcomes)
      skip = False
      for rule in wildcards:
        assert rule[-1] == '*'
        if testname.startswith(rule[:-1]):
          used_rules.add((rule, variant))
          t.outcomes = t.outcomes | wildcards[rule]
          if statusfile.DoSkip(t.outcomes):
            skip = True
            break  # "for rule in wildcards"
          slow = slow or statusfile.IsSlow(t.outcomes)
          pass_fail = pass_fail or statusfile.IsPassOrFail(t.outcomes)
      if (skip
          or self._FilterSlow(slow, slow_tests)
          or self._FilterPassFail(pass_fail, pass_fail_tests)):
        continue  # "for t in self.tests"
      filtered.append(t)
    self.tests = filtered

    if not warn_unused_rules:
      return

    if not variants:
      for rule in self.rules[""]:
        if (rule, "") not in used_rules:
          print("Unused rule: %s -> %s (variant independent)" % (
              rule, self.rules[""][rule]))
      for rule in self.wildcards[""]:
        if (rule, "") not in used_rules:
          print("Unused rule: %s -> %s (variant independent)" % (
              rule, self.wildcards[""][rule]))
    else:
      for variant in ALL_VARIANTS:
        for rule in self.rules[variant]:
          if (rule, variant) not in used_rules:
            print("Unused rule: %s -> %s (variant: %s)" % (
                rule, self.rules[variant][rule], variant))
        for rule in self.wildcards[variant]:
          if (rule, variant) not in used_rules:
            print("Unused rule: %s -> %s (variant: %s)" % (
                rule, self.wildcards[variant][rule], variant))


  def FilterTestCasesByArgs(self, args):
    """Filter test cases based on command-line arguments.

    An argument with an asterisk in the end will match all test cases
    that have the argument as a prefix. Without asterisk, only exact matches
    will be used with the exeption of the test-suite name as argument.
    """
    filtered = []
    globs = []
    exact_matches = []
    for a in args:
      argpath = a.split('/')
      if argpath[0] != self.name:
        continue
      if len(argpath) == 1 or (len(argpath) == 2 and argpath[1] == '*'):
        return  # Don't filter, run all tests in this suite.
      path = '/'.join(argpath[1:])
      if path[-1] == '*':
        path = path[:-1]
        globs.append(path)
      else:
        exact_matches.append(path)
    for t in self.tests:
      for a in globs:
        if t.path.startswith(a):
          filtered.append(t)
          break
      for a in exact_matches:
        if t.path == a:
          filtered.append(t)
          break
    self.tests = filtered

  def GetFlagsForTestCase(self, testcase, context):
    raise NotImplementedError

  def GetSourceForTest(self, testcase):
    return "(no source available)"

  def IsFailureOutput(self, testcase):
    return testcase.output.exit_code != 0

  def IsNegativeTest(self, testcase):
    return False

  def HasFailed(self, testcase):
    execution_failed = self.IsFailureOutput(testcase)
    if self.IsNegativeTest(testcase):
      return not execution_failed
    else:
      return execution_failed

  def GetOutcome(self, testcase):
    if testcase.output.HasCrashed():
      return statusfile.CRASH
    elif testcase.output.HasTimedOut():
      return statusfile.TIMEOUT
    elif self.HasFailed(testcase):
      return statusfile.FAIL
    else:
      return statusfile.PASS

  def HasUnexpectedOutput(self, testcase):
    outcome = self.GetOutcome(testcase)
    return not outcome in (testcase.outcomes or [statusfile.PASS])

  def StripOutputForTransmit(self, testcase):
    if not self.HasUnexpectedOutput(testcase):
      testcase.output.stdout = ""
      testcase.output.stderr = ""

  def CalculateTotalDuration(self):
    self.total_duration = 0.0
    for t in self.tests:
      self.total_duration += t.duration
    return self.total_duration


class StandardVariantGenerator(VariantGenerator):
  def FilterVariantsByTest(self, testcase):
    return self.standard_variant


class GoogleTestSuite(TestSuite):
  def __init__(self, name, root):
    super(GoogleTestSuite, self).__init__(name, root)

  def ListTests(self, context):
    shell = os.path.abspath(os.path.join(context.shell_dir, self.shell()))
    if utils.IsWindows():
      shell += ".exe"

    output = None
    for i in xrange(3): # Try 3 times in case of errors.
      output = commands.Execute(context.command_prefix +
                                [shell, "--gtest_list_tests"] +
                                context.extra_flags)
      if output.exit_code == 0:
        break
      print "Test executable failed to list the tests (try %d).\n\nStdout:" % i
      print output.stdout
      print "\nStderr:"
      print output.stderr
      print "\nExit code: %d" % output.exit_code
    else:
      raise Exception("Test executable failed to list the tests.")

    tests = []
    test_case = ''
    for line in output.stdout.splitlines():
      test_desc = line.strip().split()[0]
      if test_desc.endswith('.'):
        test_case = test_desc
      elif test_case and test_desc:
        test = testcase.TestCase(self, test_case + test_desc)
        tests.append(test)
    tests.sort(key=lambda t: t.path)
    return tests

  def GetFlagsForTestCase(self, testcase, context):
    return (testcase.flags + ["--gtest_filter=" + testcase.path] +
            ["--gtest_random_seed=%s" % context.random_seed] +
            ["--gtest_print_time=0"] +
            context.mode_flags)

  def _VariantGeneratorFactory(self):
    return StandardVariantGenerator

  def shell(self):
    return self.name
