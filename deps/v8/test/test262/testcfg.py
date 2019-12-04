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

# for py2/py3 compatibility
from __future__ import print_function

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
  'Intl.DateTimeFormat-datetimestyle': '--harmony-intl-datetime-style',
  'Intl.DateTimeFormat-formatRange': '--harmony-intl-date-format-range',
  'Intl.NumberFormat-unified': '--harmony-intl-numberformat-unified',
  'Intl.Segmenter': '--harmony-intl-segmenter',
  'Intl.DateTimeFormat-dayPeriod': '--harmony-intl-dateformat-day-period',
  'Intl.DateTimeFormat-quarter': '--harmony-intl-dateformat-quarter',
  'Intl.DateTimeFormat-fractionalSecondDigits': '--harmony-intl-dateformat-fractional-second-digits',
  'Symbol.prototype.description': '--harmony-symbol-description',
  'export-star-as-namespace-from-module': '--harmony-namespace-exports',
  'BigInt': '--harmony-intl-bigint',
  'Promise.allSettled': '--harmony-promise-all-settled',
  'FinalizationGroup': '--harmony-weak-refs',
  'WeakRef': '--harmony-weak-refs',
  'host-gc-required': '--expose-gc-as=v8GC',
  'optional-chaining': '--harmony-optional-chaining',
}

SKIPPED_FEATURES = set(['class-methods-private',
                        'class-static-methods-private',
                        'top-level-await'])

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")

TEST_262_HARNESS_FILES = ["sta.js", "assert.js"]
TEST_262_NATIVE_FILES = ["detachArrayBuffer.js"]

TEST_262_SUITE_PATH = ["data", "test"]
TEST_262_HARNESS_PATH = ["data", "harness"]
TEST_262_TOOLS_PATH = ["harness", "src"]
TEST_262_LOCAL_TESTS_PATH = ["local-tests", "test"]

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             *TEST_262_TOOLS_PATH))


class VariantsGenerator(testsuite.VariantsGenerator):
  def gen(self, test):
    flags_set = self._get_flags_set(test)
    test_record = test.test_record

    # Add a reverse test ensuring that FAIL_PHASE_ONLY is only used for tests
    # that actually fail to throw an exception at wrong phase.
    phase_variants = ['']
    if test.fail_phase_only:
      phase_variants.append('-fail-phase-reverse')

    for phase_var in phase_variants:
      for n, variant in enumerate(self._get_variants(test)):
        flags = flags_set[variant][0]
        if 'noStrict' in test_record:
          yield (variant, flags, str(n) + phase_var)
        elif 'onlyStrict' in test_record:
          yield (variant, flags + ['--use-strict'], 'strict-%d' % n + phase_var)
        else:
          yield (variant, flags, str(n))
          yield (variant, flags + ['--use-strict'], 'strict-%d' % n + phase_var)


class TestLoader(testsuite.JSTestLoader):
  @property
  def test_dirs(self):
    return [
      self.test_root,
      os.path.join(self.suite.root, *TEST_262_LOCAL_TESTS_PATH),
    ]

  @property
  def excluded_suffixes(self):
    return {"_FIXTURE.js"}

  @property
  def excluded_dirs(self):
    return {"intl402"} if self.test_config.noi18n else set()

  def _should_filter_by_test(self, test):
    features = test.test_record.get("features", [])
    return SKIPPED_FEATURES.intersection(features)


class TestSuite(testsuite.TestSuite):
  def __init__(self, *args, **kwargs):
    super(TestSuite, self).__init__(*args, **kwargs)
    self.test_root = os.path.join(self.root, *TEST_262_SUITE_PATH)
    # TODO: this makes the TestLoader mutable, refactor it.
    self._test_loader.test_root = self.test_root
    self.harnesspath = os.path.join(self.root, *TEST_262_HARNESS_PATH)
    self.harness = [os.path.join(self.harnesspath, f)
                    for f in TEST_262_HARNESS_FILES]
    self.harness += [os.path.join(self.root, "harness-adapt.js")]
    self.local_test_root = os.path.join(self.root, *TEST_262_LOCAL_TESTS_PATH)
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

  def _test_loader_class(self):
    return TestLoader

  def _test_class(self):
    return TestCase

  def _variants_gen_class(self):
    return VariantsGenerator


class TestCase(testcase.D8TestCase):
  def __init__(self, *args, **kwargs):
    super(TestCase, self).__init__(*args, **kwargs)

    source = self.get_source()
    self.test_record = self.suite.parse_test_record(source, self.path)
    self._expected_exception = (
        self.test_record
          .get('negative', {})
          .get('type', None)
    )

    # We disallow combining FAIL_PHASE_ONLY with any other fail outcome types.
    # Outcome parsing logic in the base class converts all outcomes specified in
    # the status file into either FAIL, CRASH or PASS, thus we do not need to
    # handle FAIL_OK, FAIL_SLOPPY and various other outcomes.
    if self.fail_phase_only:
      assert (
          statusfile.FAIL not in self.expected_outcomes and
          statusfile.CRASH not in self.expected_outcomes), self.name

  @property
  def fail_phase_only(self):
    # The FAIL_PHASE_ONLY is defined in tools/testrunner/local/statusfile.py and
    # can be used in status files to mark tests that throw an exception at wrong
    # phase, e.g. SyntaxError is thrown at execution phase instead of parsing
    # phase. See https://crbug.com/v8/8467 for more details.
    return statusfile.FAIL_PHASE_ONLY in self._statusfile_outcomes

  @property
  def _fail_phase_reverse(self):
    return 'fail-phase-reverse' in self.procid

  def __needs_harness_agent(self):
    tokens = self.path.split(os.path.sep)
    return tokens[:2] == ["built-ins", "Atomics"]

  def _get_files_params(self):
    return (
        list(self.suite.harness) +
        ([os.path.join(self.suite.root, "harness-agent.js")]
         if self.__needs_harness_agent() else []) +
        ([os.path.join(self.suite.root, "harness-adapt-donotevaluate.js")]
         if self.fail_phase_only and not self._fail_phase_reverse else []) +
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
          if feature in self.test_record.get("features", [])] +
        ["--no-arguments"]  # disable top-level arguments in d8
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
    path = os.path.join(self.suite.local_test_root, filename)
    if os.path.exists(path):
      return path
    return os.path.join(self.suite.test_root, filename)

  @property
  def output_proc(self):
    if self._expected_exception is not None:
      return test262.ExceptionOutProc(self.expected_outcomes,
                                      self._expected_exception,
                                      self._fail_phase_reverse)
    else:
      # We only support fail phase reverse on tests that expect an exception.
      assert not self._fail_phase_reverse

    if self.expected_outcomes == outproc.OUTCOMES_PASS:
      return test262.PASS_NO_EXCEPTION
    return test262.NoExceptionOutProc(self.expected_outcomes)


def GetSuite(*args, **kwargs):
  return TestSuite(*args, **kwargs)
