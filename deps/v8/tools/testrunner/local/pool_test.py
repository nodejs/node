#!/usr/bin/env python3
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.local.pool import Pool


def Run(x):
  if x == 10:
    raise Exception("Expected exception triggered by test.")
  return x


class PoolTest(unittest.TestCase):

  def testNormal(self):
    results = set()
    pool = Pool(3)
    for result in pool.imap_unordered(Run, [[x] for x in range(0, 10)]):
      if result.heartbeat:
        # Any result can be a heartbeat due to timings.
        continue
      results.add(result.value)
    self.assertEqual(set(range(0, 10)), results)

  def testException(self):
    results = set()
    pool = Pool(3)
    with self.assertRaises(Exception):
      for result in pool.imap_unordered(Run, [[x] for x in range(0, 12)]):
        if result.heartbeat:
          # Any result can be a heartbeat due to timings.
          continue
        # Item 10 will not appear in results due to an internal exception.
        results.add(result.value)
    expect = set(range(0, 12))
    expect.remove(10)
    self.assertEqual(expect, results)

  def testAdd(self):
    results = set()
    pool = Pool(3)
    for result in pool.imap_unordered(Run, [[x] for x in range(0, 10)]):
      if result.heartbeat:
        # Any result can be a heartbeat due to timings.
        continue
      results.add(result.value)
      if result.value < 30:
        pool.add([result.value + 20])
    self.assertEqual(
        set(range(0, 10)) | set(range(20, 30)) | set(range(40, 50)), results)


if __name__ == '__main__':
  unittest.main()
