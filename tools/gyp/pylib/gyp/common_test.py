#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the common.py file."""

import gyp.common
import unittest


class TestTopologicallySorted(unittest.TestCase):
  def test_Valid(self):
    """Test that sorting works on a valid graph with one possible order."""
    graph = {
        'a': ['b', 'c'],
        'b': [],
        'c': ['d'],
        'd': ['b'],
        }
    def GetEdge(node):
      return tuple(graph[node])
    self.assertEqual(
      gyp.common.TopologicallySorted(graph.keys(), GetEdge),
      ['a', 'c', 'd', 'b'])

  def test_Cycle(self):
    """Test that an exception is thrown on a cyclic graph."""
    graph = {
        'a': ['b'],
        'b': ['c'],
        'c': ['d'],
        'd': ['a'],
        }
    def GetEdge(node):
      return tuple(graph[node])
    self.assertRaises(
      gyp.common.CycleError, gyp.common.TopologicallySorted,
      graph.keys(), GetEdge)


if __name__ == '__main__':
  unittest.main()
