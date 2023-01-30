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

  def logs_test(self):
    stack_parser = create_stack_parser()
    for file in [f for f in os.listdir(TEST_DATA_ROOT) if f.endswith('.txt')]:
      if file in self.skipped_tests:
        continue
      filepath = os.path.join(TEST_DATA_ROOT, file)
      exp_filepath = os.path.join(TEST_DATA_ROOT,
                                  file.replace('.txt', '.expected.json'))
      with self.subTest(test_name=file[:-4]):
        with open(filepath) as f:
          result = stack_parser.analyze_crash(f.read())
        with open(exp_filepath, 'r') as exp_f:
          expectation = json.load(exp_f)
        self.assertDictEqual(result, expectation)

  def test_all(self):
    self.logs_test()


if __name__ == '__main__':
  unittest.main()
