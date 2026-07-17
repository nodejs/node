#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

TEST_DATA_ROOT = os.path.join(TOOLS_PATH, 'testproc', 'stack_utils', 'testdata')
TEST_DATA_GENERAL = os.path.join(TEST_DATA_ROOT, 'analyze_crash')
TEST_DATA_CUSTOM = os.path.join(TEST_DATA_ROOT, 'custom_analyzer')

from testproc.stack_utils.stack_analyzer_util import create_stack_parser


class TestScript(unittest.TestCase):
  # TODO(almuthanna): find out why these test cases are not analyzed and add
  # logic to make them pass.
  skipped_tests = [
      'type_assertion_1.txt',
      'type_assertion_2.txt',
      'static_assertion_2.txt',
      'static_assertion_1.txt',
  ]

  def test_analyze_crash(self):
    stack_parser = create_stack_parser()
    for file in [
        f for f in os.listdir(TEST_DATA_GENERAL) if f.endswith('.txt')
    ]:
      if file in self.skipped_tests:
        continue
      filepath = os.path.join(TEST_DATA_GENERAL, file)
      exp_filepath = os.path.join(TEST_DATA_GENERAL,
                                  file.replace('.txt', '.expected.json'))
      with self.subTest(test_name=file[:-4]):
        with open(filepath) as f:
          result = stack_parser.analyze_crash(f.read())
        with open(exp_filepath, 'r') as exp_f:
          expectation = json.load(exp_f)
        self.assertDictEqual(result, expectation)

  def test_fallback_crash_state(self):
    self.maxDiff = None
    stack_parser = create_stack_parser()
    for file in [f for f in os.listdir(TEST_DATA_CUSTOM) if f.endswith('.txt')]:
      filepath = os.path.join(TEST_DATA_CUSTOM, file)
      exp_filepath = os.path.join(TEST_DATA_CUSTOM,
                                  file.replace('.txt', '.expected'))
      with self.subTest(test_name=file[:-4]):
        with open(filepath) as f:
          result = stack_parser._fallback_crash_state(f.read())
        with open(exp_filepath, 'r') as exp_f:
          expectation = exp_f.read()
        self.assertEqual(result, expectation)


if __name__ == '__main__':
  unittest.main()
