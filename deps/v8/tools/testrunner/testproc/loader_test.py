#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest
from unittest.mock import patch
from contextlib import contextmanager

TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc.loader import LoadProc


class LoadProcTest(unittest.TestCase):
  # TODO(liviurau): test interaction between load_initialtests and results_for.

  def setUp(self):
    self.loader = LoadProc(iter(range(4)), 2)

  @contextmanager
  def send_test_return_values(self, l):

    def do_pop(*args):
      return l.pop()

    with patch(
        'testrunner.testproc.loader.LoadProc._send_test', side_effect=do_pop):
      yield

  def test_react_to_2_results(self):
    with self.send_test_return_values([True] * 2):
      self.loader.result_for(None, None)
      self.loader.result_for(None, None)
      self.assertEqual(2, next(self.loader.tests))

  def test_react_to_result_but_fail_to_send(self):
    with self.send_test_return_values([False] * 4):
      self.loader.result_for(None, None)
      self.assertEqual("empty", next(self.loader.tests, "empty"))

  def test_init(self):
    with self.send_test_return_values([True] * 4):
      self.loader.load_initial_tests()
      self.assertEqual(2, next(self.loader.tests))

  def test_init_fully_filtered(self):
    with self.send_test_return_values([False] * 4):
      self.loader.load_initial_tests()
      self.assertEqual("empty", next(self.loader.tests, "empty"))

  def test_init_filter_1(self):
    with self.send_test_return_values([True, False, True]):
      self.loader.load_initial_tests()
      self.assertEqual(3, next(self.loader.tests))

  def test_init_infinity(self):
    self.loader = LoadProc(iter(range(500)))
    with self.send_test_return_values(([False] * 100) + ([True] * 400)):
      self.loader.load_initial_tests()
      self.assertEqual("empty", next(self.loader.tests, "empty"))

  def test_init_0(self):
    self.loader = LoadProc(iter(range(10)), 0)
    with self.send_test_return_values([]):  # _send_test never gets called
      self.loader.load_initial_tests()
      self.assertEqual(0, next(self.loader.tests))


if __name__ == '__main__':
  unittest.main()
