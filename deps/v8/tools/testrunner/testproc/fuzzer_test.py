#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Test functionality used by the fuzzer test processor.
"""

import os
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc import fuzzer


class TestFuzzerProcHelpers(unittest.TestCase):

  def test_invert_flag(self):
    self.assertEqual(fuzzer._invert_flag('--no-flag'), '--flag')
    self.assertEqual(fuzzer._invert_flag('--flag'), '--no-flag')

  def test_flag_prefix(self):
    self.assertEqual(fuzzer._flag_prefix('--flag'), '--flag')
    self.assertEqual(fuzzer._flag_prefix('--flag=42'), '--flag')

  def test_contradictory_flags(self):

    def check(new_flags, old_flags, expected_flags):
      actual = fuzzer._drop_contradictory_flags(new_flags, old_flags)
      self.assertEqual(actual, expected_flags)

    old_flags = ['--foo-bar', '--no-bar', '--baz', '--doom-melon=42', '--flag']
    check([], old_flags, [])
    check(['--flag', '--no-baz', '--bar'], old_flags, [])
    check(['--foo-bar=42', '--doom-melon=0'], old_flags, [])
    check(['--flag', '--something', '--bar'], old_flags, ['--something'])
    check(['--something', '--doom-melon=0'], old_flags, ['--something'])
    check(old_flags, old_flags, [])
    check(['--a', '--b'], old_flags, ['--a', '--b'])

    old_flags = []
    check(['--flag', '--no-baz'], old_flags, ['--flag', '--no-baz'])

  def test_extra_flags(self):
    class FakeRandom():
      def random(self):
        return 0.5

    def check(extra_flags, expected_flags):
      self.assertEqual(
        fuzzer.random_extra_flags(rng=FakeRandom(), extra_flags=extra_flags),
        expected_flags)

    check([], [])
    check([(1, '--f1')], ['--f1'])
    check([(0.7, '--f1'), (0.3, '--f2'), (0.9, '--f3')], ['--f1', '--f3'])
    check([(0.7, '--f1 --f2'), (0.3, '--f3'), (1, '--f4 --f5 --f6')],
          ['--f1', '--f2', '--f4', '--f5', '--f6'])


if __name__ == '__main__':
  unittest.main()
