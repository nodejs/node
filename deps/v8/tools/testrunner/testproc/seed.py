# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
from collections import defaultdict

from . import base
from ..utils import random_utils


class SeedProc(base.TestProcProducer):
  def __init__(self, count, seed=None, parallel_subtests=1):
    """
    Args:
      count: How many subtests with different seeds to create for each test.
        0 means infinite.
      seed: seed to use. None means random seed for each subtest.
      parallel_subtests: How many subtest of each test to run at the same time.
    """
    super(SeedProc, self).__init__('Seed')
    self._count = count
    self._seed = seed
    self._last_idx = defaultdict(int)
    self._todo = defaultdict(int)
    self._parallel_subtests = parallel_subtests
    if count:
      self._parallel_subtests = min(self._parallel_subtests, count)

  def setup(self, requirement=base.DROP_RESULT):
    super(SeedProc, self).setup(requirement)

    # SeedProc is optimized for dropping the result
    assert requirement == base.DROP_RESULT

  def _next_test(self, test):
    is_loaded = False
    for _ in range(0, self._parallel_subtests):
      is_loaded |= self._try_send_next_test(test)

    return is_loaded

  def _result_for(self, test, subtest, result):
    self._todo[test.procid] -= 1
    if not self._try_send_next_test(test):
      if not self._todo.get(test.procid):
        del self._last_idx[test.procid]
        del self._todo[test.procid]
        self._send_result(test, None)

  def _try_send_next_test(self, test):
    def create_subtest(idx):
      seed = self._seed or random_utils.random_seed()
      return self._create_subtest(test, idx, random_seed=seed)

    num = self._last_idx[test.procid]
    if not self._count or num < self._count:
      num += 1
      self._todo[test.procid] += 1
      self._last_idx[test.procid] = num
      return self._send_test(create_subtest(num))

    return False
