#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from pool import Pool

def Run(x):
  if x == 10:
    raise Exception("Expected exception triggered by test.")
  return x

class PoolTest(unittest.TestCase):
  def testNormal(self):
    results = set()
    pool = Pool(3)
    for result in pool.imap_unordered(Run, [[x] for x in range(0, 10)]):
      results.add(result)
    self.assertEquals(set(range(0, 10)), results)

  def testException(self):
    results = set()
    pool = Pool(3)
    for result in pool.imap_unordered(Run, [[x] for x in range(0, 12)]):
      # Item 10 will not appear in results due to an internal exception.
      results.add(result)
    expect = set(range(0, 12))
    expect.remove(10)
    self.assertEquals(expect, results)

  def testAdd(self):
    results = set()
    pool = Pool(3)
    for result in pool.imap_unordered(Run, [[x] for x in range(0, 10)]):
      results.add(result)
      if result < 30:
        pool.add([result + 20])
    self.assertEquals(set(range(0, 10) + range(20, 30) + range(40, 50)),
                      results)
