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


import fnmatch
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
    outcomes = testcase.suite.GetStatusFileOutcomes(testcase)
    if outcomes:
      if statusfile.OnlyStandardVariant(outcomes):
        return self.standard_variant
      if statusfile.OnlyFastVariants(outcomes):
        result = self.fast_variants
    return result

  def GetFlagSets(self, testcase, variant):
    outcomes = testcase.suite.GetStatusFileOutcomes(testcase)
    if outcomes and statusfile.OnlyFastVariants(outcomes):
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
    self.rules = None  # {variant: {test name: [rule]}}
    self.prefix_rules = None  # {variant: {test name prefix: [rule]}}
    self.total_duration = None  # float, assigned on demand

    self._outcomes_cache = dict()

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

  def ReadStatusFile(self, variables):
    with open(self.status_file()) as f:
      self.rules, self.prefix_rules = (
          statusfile.ReadStatusFile(f.read(), variables))

  def ReadTestCases(self, context):
    self.tests = self.ListTests(context)

  def GetStatusfileFlags(self, test):
    """Gets runtime flags from a status file.

    Every outcome that starts with "--" is a flag. Status file has to be loaded
    before using this function.
    """
    flags = []
    for outcome in self.GetStatusFileOutcomes(test):
      if outcome.startswith('--'):
        flags.append(outcome)
    return flags

  def FilterTestCasesByStatus(self,
                              slow_tests_mode=None,
                              pass_fail_tests_mode=None):
    """Filters tests by outcomes from status file.

    Status file has to be loaded before using this function.

    Args:
      slow_tests_mode: What to do with slow tests.
      pass_fail_tests_mode: What to do with pass or fail tests.

    Mode options:
      None (default) - don't skip
      "skip" - skip if slow/pass_fail
      "run" - skip if not slow/pass_fail
    """
    def _skip_slow(is_slow, mode):
      return (
        (mode == 'run' and not is_slow) or
        (mode == 'skip' and is_slow))

    def _skip_pass_fail(pass_fail, mode):
      return (
        (mode == 'run' and not pass_fail) or
        (mode == 'skip' and pass_fail))

    def _compliant(test):
      outcomes = self.GetStatusFileOutcomes(test)
      if statusfile.DoSkip(outcomes):
        return False
      if _skip_slow(statusfile.IsSlow(outcomes), slow_tests_mode):
        return False
      if _skip_pass_fail(statusfile.IsPassOrFail(outcomes),
                         pass_fail_tests_mode):
        return False
      return True

    self.tests = filter(_compliant, self.tests)

  def WarnUnusedRules(self, check_variant_rules=False):
    """Finds and prints unused rules in status file.

    Rule X is unused when it doesn't apply to any tests, which can also mean
    that all matching tests were skipped by another rule before evaluating X.

    Status file has to be loaded before using this function.
    """

    if check_variant_rules:
      variants = list(ALL_VARIANTS)
    else:
      variants = ['']
    used_rules = set()

    for t in self.tests:
      testname = self.CommonTestName(t)
      variant = t.variant or ""

      if testname in self.rules.get(variant, {}):
        used_rules.add((testname, variant))
        if statusfile.DoSkip(self.rules[variant][testname]):
          continue

      for prefix in self.prefix_rules.get(variant, {}):
        if testname.startswith(prefix):
          used_rules.add((prefix, variant))
          if statusfile.DoSkip(self.prefix_rules[variant][prefix]):
            break

    for variant in variants:
      for rule, value in (list(self.rules.get(variant, {}).iteritems()) +
                          list(self.prefix_rules.get(variant, {}).iteritems())):
        if (rule, variant) not in used_rules:
          if variant == '':
            variant_desc = 'variant independent'
          else:
            variant_desc = 'variant: %s' % variant
          print('Unused rule: %s -> %s (%s)' % (rule, value, variant_desc))

  def FilterTestCasesByArgs(self, args):
    """Filter test cases based on command-line arguments.

    args can be a glob: asterisks in any position of the argument
    represent zero or more characters. Without asterisks, only exact matches
    will be used with the exeption of the test-suite name as argument.
    """
    filtered = []
    globs = []
    for a in args:
      argpath = a.split('/')
      if argpath[0] != self.name:
        continue
      if len(argpath) == 1 or (len(argpath) == 2 and argpath[1] == '*'):
        return  # Don't filter, run all tests in this suite.
      path = '/'.join(argpath[1:])
      globs.append(path)

    for t in self.tests:
      for g in globs:
        if fnmatch.fnmatch(t.path, g):
          filtered.append(t)
          break
    self.tests = filtered

  def GetExpectedOutcomes(self, testcase):
    """Gets expected outcomes from status file.

    It differs from GetStatusFileOutcomes by selecting only outcomes that can
    be result of test execution.
    Status file has to be loaded before using this function.
    """
    outcomes = self.GetStatusFileOutcomes(testcase)

    expected = []
    if (statusfile.FAIL in outcomes or
        statusfile.FAIL_OK in outcomes):
      expected.append(statusfile.FAIL)

    if statusfile.CRASH in outcomes:
      expected.append(statusfile.CRASH)

    if statusfile.PASS in outcomes:
      expected.append(statusfile.PASS)

    return expected or [statusfile.PASS]

  def GetStatusFileOutcomes(self, testcase):
    """Gets outcomes from status file.

    Merges variant dependent and independent rules. Status file has to be loaded
    before using this function.
    """
    variant = testcase.variant or ''
    testname = self.CommonTestName(testcase)
    cache_key = '%s$%s' % (testname, variant)

    if cache_key not in self._outcomes_cache:
      # Load statusfile to get outcomes for the first time.
      assert(self.rules is not None)
      assert(self.prefix_rules is not None)

      outcomes = frozenset()

      for key in set([variant, '']):
        rules = self.rules.get(key, {})
        prefix_rules = self.prefix_rules.get(key, {})

        if testname in rules:
          outcomes |= rules[testname]

        for prefix in prefix_rules:
          if testname.startswith(prefix):
            outcomes |= prefix_rules[prefix]

      self._outcomes_cache[cache_key] = outcomes

    return self._outcomes_cache[cache_key]

  def GetShellForTestCase(self, testcase):
    """Returns shell to be executed for this test case."""
    return 'd8'

  def GetParametersForTestCase(self, testcase, context):
    """Returns a tuple of (files, flags, env) for this test case."""
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
    return self.GetOutcome(testcase) not in self.GetExpectedOutcomes(testcase)

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
    shell = os.path.abspath(
      os.path.join(context.shell_dir, self.GetShellForTestCase(None)))
    if utils.IsWindows():
      shell += ".exe"

    output = None
    for i in xrange(3): # Try 3 times in case of errors.
      cmd = (
          context.command_prefix +
          [shell, "--gtest_list_tests"] +
          context.extra_flags
      )
      output = commands.Execute(cmd)
      if output.exit_code == 0:
        break
      print "Test executable failed to list the tests (try %d).\n\nCmd:" % i
      print ' '.join(cmd)
      print "\nStdout:"
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

  def GetParametersForTestCase(self, testcase, context):
    flags = (
      testcase.flags +
      ["--gtest_filter=" + testcase.path] +
      ["--gtest_random_seed=%s" % context.random_seed] +
      ["--gtest_print_time=0"] +
      context.mode_flags)
    return [], flags, {}

  def _VariantGeneratorFactory(self):
    return StandardVariantGenerator

  def GetShellForTestCase(self, testcase):
    return self.name
