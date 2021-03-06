#!/usr/bin/env python
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc import base
from testrunner.testproc.variant import VariantProc


class FakeResultObserver(base.TestProcObserver):
  def __init__(self):
    super(FakeResultObserver, self).__init__()

    self.results = set()

  def result_for(self, test, result):
    self.results.add((test, result))


class FakeFilter(base.TestProcFilter):
  def __init__(self, filter_predicate):
    super(FakeFilter, self).__init__()

    self._filter_predicate = filter_predicate

    self.loaded = set()
    self.call_counter = 0

  def next_test(self, test):
    self.call_counter += 1

    if self._filter_predicate(test):
      return False

    self.loaded.add(test)
    return True


class FakeSuite(object):
  def __init__(self, name):
    self.name = name


class FakeTest(object):
  def __init__(self, procid):
    self.suite = FakeSuite("fake_suite")
    self.procid = procid

    self.keep_output = False

  def create_subtest(self, proc, subtest_id, **kwargs):
    variant = kwargs['variant']

    variant.origin = self
    return variant


class FakeVariantGen(object):
  def __init__(self, variants):
    self._variants = variants

  def gen(self, test):
    for variant in self._variants:
      yield variant, [], "fake_suffix"


class TestVariantProcLoading(unittest.TestCase):
  def setUp(self):
    self.test = FakeTest("test")

  def _simulate_proc(self, variants):
    """Expects the list of instantiated test variants to load into the
    VariantProc."""
    variants_mapping = {self.test: variants}

    # Creates a Variant processor containing the possible types of test
    # variants.
    self.variant_proc = VariantProc(variants=["to_filter", "to_load"])
    self.variant_proc._variant_gens = {
      "fake_suite": FakeVariantGen(variants)}

    # FakeFilter only lets tests passing the predicate to be loaded.
    self.fake_filter = FakeFilter(
      filter_predicate=(lambda t: t.procid == "to_filter"))

    # FakeResultObserver to verify that VariantProc calls result_for correctly.
    self.fake_result_observer = FakeResultObserver()

    # Links up processors together to form a test processing pipeline.
    self.variant_proc._prev_proc = self.fake_result_observer
    self.fake_filter._prev_proc = self.variant_proc
    self.variant_proc._next_proc = self.fake_filter

    # Injects the test into the VariantProc
    is_loaded = self.variant_proc.next_test(self.test)

    # Verifies the behavioral consistency by using the instrumentation in
    # FakeFilter
    loaded_variants = list(self.fake_filter.loaded)
    self.assertEqual(is_loaded, any(loaded_variants))
    return self.fake_filter.loaded, self.fake_filter.call_counter

  def test_filters_first_two_variants(self):
    variants = [
      FakeTest('to_filter'),
      FakeTest('to_filter'),
      FakeTest('to_load'),
      FakeTest('to_load'),
    ]
    expected_load_results = {variants[2]}

    load_results, call_count = self._simulate_proc(variants)

    self.assertSetEqual(expected_load_results, load_results)
    self.assertEqual(call_count, 3)

  def test_stops_loading_after_first_successful_load(self):
    variants = [
      FakeTest('to_load'),
      FakeTest('to_load'),
      FakeTest('to_filter'),
    ]
    expected_load_results = {variants[0]}

    loaded_tests, call_count = self._simulate_proc(variants)

    self.assertSetEqual(expected_load_results, loaded_tests)
    self.assertEqual(call_count, 1)

  def test_return_result_when_out_of_variants(self):
    variants = [
      FakeTest('to_filter'),
      FakeTest('to_load'),
    ]

    self._simulate_proc(variants)

    self.variant_proc.result_for(variants[1], None)

    expected_results = {(self.test, None)}

    self.assertSetEqual(expected_results, self.fake_result_observer.results)

  def test_return_result_after_running_variants(self):
    variants = [
      FakeTest('to_filter'),
      FakeTest('to_load'),
      FakeTest('to_load'),
    ]

    self._simulate_proc(variants)
    self.variant_proc.result_for(variants[1], None)

    self.assertSetEqual(set(variants[1:]), self.fake_filter.loaded)

    self.variant_proc.result_for(variants[2], None)

    expected_results = {(self.test, None)}
    self.assertSetEqual(expected_results, self.fake_result_observer.results)

if __name__ == '__main__':
  unittest.main()
