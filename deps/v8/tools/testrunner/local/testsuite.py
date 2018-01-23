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

from . import command
from . import statusfile
from . import utils
from ..objects.testcase import TestCase
from variants import ALL_VARIANTS, ALL_VARIANT_FLAGS


STANDARD_VARIANT = set(["default"])


class LegacyVariantsGenerator(object):
  def __init__(self, suite, variants):
    self.suite = suite
    self.all_variants = ALL_VARIANTS & variants
    self.standard_variant = STANDARD_VARIANT & variants

  def FilterVariantsByTest(self, test):
    if test.only_standard_variant:
      return self.standard_variant
    return self.all_variants

  def GetFlagSets(self, test, variant):
    return ALL_VARIANT_FLAGS[variant]


class StandardLegacyVariantsGenerator(LegacyVariantsGenerator):
  def FilterVariantsByTest(self, testcase):
    return self.standard_variant


class VariantsGenerator(object):
  def __init__(self, variants):
    self._all_variants = [v for v in variants if v in ALL_VARIANTS]
    self._standard_variant = [v for v in variants if v in STANDARD_VARIANT]

  def gen(self, test):
    """Generator producing (variant, flags, procid suffix) tuples."""
    flags_set = self._get_flags_set(test)
    for n, variant in enumerate(self._get_variants(test)):
      yield (variant, flags_set[variant][0], n)

  def _get_flags_set(self, test):
    return ALL_VARIANT_FLAGS

  def _get_variants(self, test):
    if test.only_standard_variant:
      return self._standard_variant
    return self._all_variants


class TestSuite(object):
  @staticmethod
  def LoadTestSuite(root):
    name = root.split(os.path.sep)[-1]
    f = None
    try:
      (f, pathname, description) = imp.find_module("testcfg", [root])
      module = imp.load_module(name + "_testcfg", f, pathname, description)
      return module.GetSuite(name, root)
    finally:
      if f:
        f.close()

  def __init__(self, name, root):
    # Note: This might be called concurrently from different processes.
    self.name = name  # string
    self.root = root  # string containing path
    self.tests = None  # list of TestCase objects
    self.statusfile = None

  def status_file(self):
    return "%s/%s.status" % (self.root, self.name)

  def ListTests(self, context):
    raise NotImplementedError

  def _LegacyVariantsGeneratorFactory(self):
    """The variant generator class to be used."""
    return LegacyVariantsGenerator

  def CreateLegacyVariantsGenerator(self, variants):
    """Return a generator for the testing variants of this suite.

    Args:
      variants: List of variant names to be run as specified by the test
                runner.
    Returns: An object of type LegacyVariantsGenerator.
    """
    return self._LegacyVariantsGeneratorFactory()(self, set(variants))

  def get_variants_gen(self, variants):
    return self._variants_gen_class()(variants)

  def _variants_gen_class(self):
    return VariantsGenerator

  def ReadStatusFile(self, variables):
    self.statusfile = statusfile.StatusFile(self.status_file(), variables)

  def ReadTestCases(self, context):
    self.tests = self.ListTests(context)


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
      if test.do_skip:
        return False
      if _skip_slow(test.is_slow, slow_tests_mode):
        return False
      if _skip_pass_fail(test.is_pass_or_fail, pass_fail_tests_mode):
        return False
      return True

    self.tests = filter(_compliant, self.tests)

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

  def _create_test(self, path, **kwargs):
    test = self._test_class()(self, path, self._path_to_name(path), **kwargs)
    return test

  def _test_class(self):
    raise NotImplementedError

  def _path_to_name(self, path):
    if utils.IsWindows():
      return path.replace("\\", "/")
    return path
