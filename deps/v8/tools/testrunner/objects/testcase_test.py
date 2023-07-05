#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.objects.testcase import TestCase


class TestCaseTest(unittest.TestCase):

  def testSubtestsProperties(self):
    test = TestCase(
        suite=FakeSuite(),
        path='far/away',
        name='parent')
    self.assertEqual(test.rdb_test_id, 'fakeSuite/parent')
    # provide by DuckProcessor
    self.assertEqual(test.processor.name, None)
    self.assertEqual(test.procid, 'fakeSuite/parent')
    self.assertEqual(test.keep_output, False)

    subtest = test.create_subtest(FakeProcessor(), 0, keep_output=True)
    self.assertEqual(subtest.rdb_test_id, 'fakeSuite/parent//fakep/0')
    # provide by FakeProcessor
    self.assertEqual(subtest.processor.name, 'fake_processor1')
    self.assertEqual(subtest.procid, 'fakeSuite/parent.fake_processor1-0')
    self.assertEqual(subtest.keep_output, True)

    subsubtest = subtest.create_subtest(FakeProcessor(), 1)
    self.assertEqual(subsubtest.rdb_test_id,
                     'fakeSuite/parent//fakep/0/fakep/1')
    # provide by FakeProcessor
    self.assertEqual(subsubtest.processor.name, 'fake_processor2')
    self.assertEqual(subsubtest.procid,
                     'fakeSuite/parent.fake_processor1-0.fake_processor2-1')
    self.assertEqual(subsubtest.keep_output, True)


class FakeSuite:

  @property
  def name(self):
    return 'fakeSuite'

  def statusfile_outcomes(self, name, variant):
    return []


class FakeProcessor:
  instance_count = 0

  def __init__(self):
    FakeProcessor.instance_count += 1
    self.idx = FakeProcessor.instance_count

  @property
  def name(self):
    return f'fake_processor{self.idx}'

  def test_suffix(self, test):
    return f'fakep/{test.subtest_id}'


if __name__ == '__main__':
  unittest.main()
