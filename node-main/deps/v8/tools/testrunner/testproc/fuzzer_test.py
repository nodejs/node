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


if __name__ == '__main__':
  unittest.main()
