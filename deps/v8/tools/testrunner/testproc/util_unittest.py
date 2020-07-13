#!/usr/bin/env python
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from util import FixedSizeTopList
import unittest

class TestOrderedFixedSizeList(unittest.TestCase):
  def test_empty(self):
    ofsl = FixedSizeTopList(3)
    self.assertEqual(ofsl.as_list(), [])

  def test_12(self):
    ofsl = FixedSizeTopList(3)
    ofsl.add(1)
    ofsl.add(2)
    self.assertEqual(ofsl.as_list(), [2,1])

  def test_4321(self):
    ofsl = FixedSizeTopList(3)
    ofsl.add(4)
    ofsl.add(3)
    ofsl.add(2)
    ofsl.add(1)
    data = ofsl.as_list()
    self.assertEqual(data, [4,3,2])

  def test_544321(self):
    ofsl = FixedSizeTopList(4)
    ofsl.add(5)
    ofsl.add(4)
    ofsl.add(4)
    ofsl.add(3)
    ofsl.add(2)
    ofsl.add(1)
    data = ofsl.as_list()
    self.assertEqual(data, [5, 4, 4, 3])

  def test_withkey(self):
    ofsl = FixedSizeTopList(3,key=lambda x: x['val'])
    ofsl.add({'val':4, 'something': 'four'})
    ofsl.add({'val':3, 'something': 'three'})
    ofsl.add({'val':-1, 'something': 'minusone'})
    ofsl.add({'val':5, 'something': 'five'})
    ofsl.add({'val':0, 'something': 'zero'})
    data = [e['something'] for e in ofsl.as_list()]
    self.assertEqual(data, ['five', 'four', 'three'])

  def test_withkeyclash(self):
    # Test that a key clash does not throw exeption
    ofsl = FixedSizeTopList(2,key=lambda x: x['val'])
    ofsl.add({'val':2, 'something': 'two'})
    ofsl.add({'val':2, 'something': 'two'})
    ofsl.add({'val':0, 'something': 'zero'})
    data = [e['something'] for e in ofsl.as_list()]
    self.assertEqual(data, ['two', 'two'])


if __name__ == '__main__':
  unittest.main()
