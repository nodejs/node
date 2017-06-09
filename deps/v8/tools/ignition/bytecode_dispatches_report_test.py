# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import bytecode_dispatches_report as bdr
import unittest


class BytecodeDispatchesReportTest(unittest.TestCase):
  def test_find_top_counters(self):
    top_counters = bdr.find_top_bytecode_dispatch_pairs({
      "a": {"a": 10, "b": 8, "c": 99},
      "b": {"a":  1, "b": 4, "c":  1},
      "c": {"a": 42, "b": 3, "c":  7}}, 5)
    self.assertListEqual(top_counters, [
      ('a', 'c', 99),
      ('c', 'a', 42),
      ('a', 'a', 10),
      ('a', 'b',  8),
      ('c', 'c',  7)])

  def test_build_counters_matrix(self):
    counters_matrix, xlabels, ylabels = bdr.build_counters_matrix({
      "a": {"a": 10, "b":  8, "c":  7},
      "b": {"a":  1, "c":  4},
      "c": {"a": 42, "b": 12, "c": 99}})
    self.assertTrue((counters_matrix == [[42, 12, 99],
                                         [ 1,  0,  4],
                                         [10,  8,  7]]).all())
    self.assertListEqual(xlabels, ['a', 'b', 'c'])
    self.assertListEqual(ylabels, ['c', 'b', 'a'])

  def test_find_top_bytecodes(self):
    top_dispatch_sources = bdr.find_top_bytecodes({
      "a": {"a": 10, "b":  8, "c":  7},
      "b": {"a":  1, "c":  4},
      "c": {"a": 42, "b": 12, "c": 99}
    })
    self.assertListEqual(top_dispatch_sources, [
      ('c', 153),
      ('a',  25),
      ('b',   5)
    ])

  def test_find_top_dispatch_sources_and_destinations(self):
    d = {
      "a": {"a":  4, "b":  2, "c":  4},
      "b": {"a":  1, "c":  4},
      "c": {"a": 40, "b": 10, "c": 50}
    }
    top_sources, top_dests = bdr.find_top_dispatch_sources_and_destinations(
      d, "b", 10, False)
    self.assertListEqual(top_sources, [
      ("c", 10, 0.1),
      ("a", 2, 0.2)
    ])
    top_sources, top_dests = bdr.find_top_dispatch_sources_and_destinations(
      d, "b", 10, True)
    self.assertListEqual(top_sources, [
      ("a", 2, 0.2),
      ("c", 10, 0.1)
    ])
