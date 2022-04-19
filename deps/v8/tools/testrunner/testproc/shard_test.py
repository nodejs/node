#!/usr/bin/env python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import tempfile
import unittest

# Needed because the test runner contains relative imports.
TOOLS_PATH = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(TOOLS_PATH)

from testrunner.testproc.shard import radix_hash


class TestRadixHashing(unittest.TestCase):

  def test_hash_character_by_radix(self):
    self.assertEqual(97, radix_hash(capacity=2**32, key="a"))

  def test_hash_character_by_radix_with_capacity(self):
    self.assertEqual(6, radix_hash(capacity=7, key="a"))

  def test_hash_string(self):
    self.assertEqual(6, radix_hash(capacity=7, key="ab"))

  def test_hash_test_id(self):
    self.assertEqual(
        5,
        radix_hash(
            capacity=7, key="test262/Map/class-private-method-Variant-0-1"))

  def test_hash_boundaries(self):
    total_variants = 5
    cases = []
    for case in [
        "test262/Map/class-private-method",
        "test262/Map/class-public-method",
        "test262/Map/object-retrieval",
        "test262/Map/object-deletion",
        "test262/Map/object-creation",
        "test262/Map/garbage-collection",
    ]:
      for variant_index in range(total_variants):
        cases.append("%s-Variant-%d" % (case, variant_index))

    for case in cases:
      self.assertTrue(0 <= radix_hash(capacity=7, key=case) < 7)


if __name__ == '__main__':
  unittest.main()
