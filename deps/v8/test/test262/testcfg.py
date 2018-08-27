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
import itertools
import os
import re
import sys

from testrunner.local import statusfile
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.objects import testcase
from testrunner.outproc import base as outproc
from testrunner.outproc import test262


# TODO(littledan): move the flag mapping into the status file
FEATURE_FLAGS = {
  'BigInt': '--harmony-bigint',
  'regexp-named-groups': '--harmony-regexp-named-captures',
  'regexp-unicode-property-escapes': '--harmony-regexp-property',
  'Promise.prototype.finally': '--harmony-promise-finally',
  'class-fields-public': '--harmony-public-fields',
  'optional-catch-binding': '--harmony-optional-catch-binding',
  'class-fields-private': '--harmony-private-fields',
  'Array.prototype.flatten': '--harmony-array-flatten',
  'Array.prototype.flatMap': '--harmony-array-flatten',
  'String.prototype.matchAll': '--harmony-string-matchall',
  'Symbol.matchAll': '--harmony-string-matchall',
  'numeric-separator-literal': '--harmony-numeric-separator',
}

SKIPPED_FEATURES = set([])

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")

TEST_262_HARNESS_FILES = ["sta.js", "assert.js"]
TEST_262_NATIVE_FILES = ["detachArrayBuffer.js"]

TEST_262_SUITE_PATH = ["data", "test"]
TEST_262_HARNESS_PATH = ["data", "harness"]
TEST_262_TOOLS_PATH = ["harness", "src"]
TEST_262_LOCAL_TESTS_PATH = ["local-tests", "test"]

TEST_262_RELPATH_REGEXP = re.compile(
    r'.*[\\/]test[\\/]test262[\\/][^\\/]+[\\/]test[\\/](.*)\.js')

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             *TEST_262_TOOLS_PATH))


class VariantsGenerator(testsuite.VariantsGenerator):
  def gen(self, test):
    flags_set = self._get_flags_set(test)
    test_record = test.test_record
    for n, variant in enumerate(self._get_variants(test)):
      flags = flags_set[variant][0]
      if 'noStrict' in test_record:
        yield (variant, flags, str(n))
      elif 'onlyStrict' in test_record:
        yield (variant, flags + ['--use-strict'], 'strict-%d' % n)
      else:
        yield (variant, flags, str(n))
        yield (variant, flags + ['--use-strict'], 'strict-%d' % n)


class TestSuite(testsuite.TestSuite):
  # Match the (...) in '/path/to/v8/test/test262/subdir/test/(...).js'
  # In practice, subdir is data or local-tests

  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)
    self.testroot = os.path.join(self.root, *TEST_262_SUITE_PATH)
    self.harnesspath = os.path.join(self.root, *TEST_262_HARNESS_PATH)
    self.harness = [os.path.join(self.harnesspath, f)
                    for f in TEST_262_HARNESS_FILES]
    self.harness += [os.path.join(self.root, "harness-adapt.js")]
    self.localtestroot = os.path.join(self.root, *TEST_262_LOCAL_TESTS_PATH)
    self.parse_test_record = self._load_parse_test_record()

  def _load_parse_test_record(self):
    root = os.path.join(self.root, *TEST_262_TOOLS_PATH)
    f = None
    try:
      (f, pathname, description) = imp.find_module("parseTestRecord", [root])
      module = imp.load_module("parseTestRecord", f, pathname, description)
      return module.parseTestRecord
    except:
      print ('Cannot load parseTestRecord; '
             'you may need to gclient sync for test262')
      raise
    finally:
      if f:
        f.close()

  def ListTests(self):
    testnames = set()
    for dirname, dirs, files in itertools.chain(os.walk(self.testroot),
                                                os.walk(self.localtestroot)):
      for dotted in [x for x in dirs if x.startswith(".")]:
        dirs.remove(dotted)
      if self.test_config.noi18n and "intl402" in dirs:
        dirs.remove("intl402")
      dirs.sort()
      files.sort()
      for filename in files:
        if not filename.endswith(".js"):
          continue
        if filename.endswith("_FIXTURE.js"):
          continue
        fullpath = os.path.join(dirname, filename)
        relpath = re.match(TEST_262_RELPATH_REGEXP, fullpath).group(1)
        testnames.add(relpath.replace(os.path.sep, "/"))
    cases = map(self._create_test, testnames)
    return [case for case in cases if len(
                SKIPPED_FEATURES.intersection(
                    case.test_record.get("features", []))) == 0]

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()
    self.test_record = self.suite.parse_test_record(source, self.path)
    self._expected_exception = (
        self.test_record
          .get('negative', {})
          .get('type', None)
    )

  def _get_files_params(self):
    return (
        list(self.suite.harness) +
        ([os.path.join(self.suite.root, "harness-agent.js")]
         if self.path.startswith('built-ins/Atomics') else []) +
        self._get_includes() +
        (["--module"] if "module" in self.test_record else []) +
        [self._get_source_path()]
    )

  def _get_suite_flags(self):
    return (
        (["--throws"] if "negative" in self.test_record else []) +
        (["--allow-natives-syntax"]
         if "detachArrayBuffer.js" in self.test_record.get("includes", [])
         else []) +
        [flag for (feature, flag) in FEATURE_FLAGS.items()
          if feature in self.test_record.get("features", [])]
    )

  def _get_includes(self):
    return [os.path.join(self._base_path(filename), filename)
            for filename in self.test_record.get("includes", [])]

  def _base_path(self, filename):
    if filename in TEST_262_NATIVE_FILES:
      return self.suite.root
    else:
      return self.suite.harnesspath

  def _get_source_path(self):
    filename = self.path + self._get_suffix()
    path = os.path.join(self.suite.localtestroot, filename)
    if os.path.exists(path):
      return path
    return os.path.join(self.suite.testroot, filename)

  @property
  def output_proc(self):
    if self._expected_exception is not None:
      return test262.ExceptionOutProc(self.expected_outcomes,
                                      self._expected_exception)
    if self.expected_outcomes == outproc.OUTCOMES_PASS:
      return test262.PASS_NO_EXCEPTION
    return test262.NoExceptionOutProc(self.expected_outcomes)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
