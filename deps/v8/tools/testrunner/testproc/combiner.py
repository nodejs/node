# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import time

from . import base
from ..objects import testcase
from ..outproc import base as outproc

class CombinerProc(base.TestProc):
  def __init__(self, rng, min_group_size, max_group_size, count):
    """
    Args:
      rng: random number generator
      min_group_size: minimum number of tests to combine
      max_group_size: maximum number of tests to combine
      count: how many tests to generate. 0 means infinite running
    """
    super(CombinerProc, self).__init__()

    self._rng = rng
    self._min_size = min_group_size
    self._max_size = max_group_size
    self._count = count

    # Index of the last generated test
    self._current_num = 0

    # {suite name: instance of TestGroups}
    self._groups = defaultdict(TestGroups)

    # {suite name: instance of TestCombiner}
    self._combiners = {}

  def setup(self, requirement=base.DROP_RESULT):
    # Combiner is not able to pass results (even as None) to the previous
    # processor.
    assert requirement == base.DROP_RESULT
    self._next_proc.setup(base.DROP_RESULT)

  def next_test(self, test):
    group_key = self._get_group_key(test)
    if not group_key:
      # Test not suitable for combining
      return False

    self._groups[test.suite.name].add_test(group_key, test)
    return True

  def _get_group_key(self, test):
    combiner =  self._get_combiner(test.suite)
    if not combiner:
      print ('>>> Warning: There is no combiner for %s testsuite' %
             test.suite.name)
      return None
    return combiner.get_group_key(test)

  def result_for(self, test, result):
    self._send_next_test()

  def generate_initial_tests(self, num=1):
    for _ in range(0, num):
      self._send_next_test()

  def _send_next_test(self):
    if self.is_stopped:
      return False

    if self._count and self._current_num >= self._count:
      return False

    combined_test = self._create_new_test()
    if not combined_test:
      # Not enough tests
      return False

    return self._send_test(combined_test)

  def _create_new_test(self):
    suite, combiner = self._select_suite()
    groups = self._groups[suite]

    max_size = self._rng.randint(self._min_size, self._max_size)
    sample = groups.sample(self._rng, max_size)
    if not sample:
      return None

    self._current_num += 1
    return combiner.combine('%s-%d' % (suite, self._current_num), sample)

  def _select_suite(self):
    """Returns pair (suite name, combiner)."""
    selected = self._rng.randint(0, len(self._groups) - 1)
    for n, suite in enumerate(self._groups):
      if n == selected:
        return suite, self._combiners[suite]

  def _get_combiner(self, suite):
    combiner = self._combiners.get(suite.name)
    if not combiner:
      combiner = suite.get_test_combiner()
      self._combiners[suite.name] = combiner
    return combiner


class TestGroups(object):
  def __init__(self):
    self._groups = defaultdict(list)
    self._keys = []

  def add_test(self, key, test):
    self._groups[key].append(test)
    self._keys.append(key)

  def sample(self, rng, max_size):
    # Not enough tests
    if not self._groups:
      return None

    group_key = rng.choice(self._keys)
    tests = self._groups[group_key]
    return [rng.choice(tests) for _ in range(0, max_size)]
