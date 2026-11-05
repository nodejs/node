#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for change_targets.py. Run: python3 -m unittest discover tools/tests"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import change_targets  # pylint: disable=wrong-import-position
import edge_map  # pylint: disable=wrong-import-position

DIFF = """\
diff --git a/src/maglev/a.cc b/src/maglev/a.cc
--- a/src/maglev/a.cc
+++ b/src/maglev/a.cc
@@ -10,2 +10,3 @@ void F() {
+x
@@ -40,3 +41,0 @@ void G() {
diff --git a/test/mjsunit/t.js b/test/mjsunit/t.js
--- a/test/mjsunit/t.js
+++ b/test/mjsunit/t.js
@@ -1 +1 @@
diff --git a/src/flags/flag-definitions.h b/src/flags/flag-definitions.h
--- a/src/flags/flag-definitions.h
+++ b/src/flags/flag-definitions.h
@@ -5,0 +6,1 @@
+DEFINE_BOOL(maglev_new_thing, false, "a new thing")
+DEFINE_BOOL(on_by_default, true, "not a candidate")
diff --git a/src/old.cc b/src/old.cc
deleted file mode 100644
--- a/src/old.cc
+++ /dev/null
@@ -1,3 +0,0 @@
"""


class ParseDiffTest(unittest.TestCase):

  def test_lines_and_flags(self):
    changed, hunks, flags = change_targets.parse_diff(DIFF)
    # Added lines 10-12, and the lines around the removal after line 41.
    self.assertEqual(changed['src/maglev/a.cc'], [10, 11, 12, 41, 42])
    self.assertEqual(hunks['src/maglev/a.cc'], [3, 1])
    self.assertNotIn('test/mjsunit/t.js', changed)
    self.assertNotIn('src/old.cc', changed)
    self.assertEqual(flags, ['--maglev-new-thing'])


class ComputeTargetsTest(unittest.TestCase):
  EDGES = [
      (1, [['src/maglev/a.cc', 11, 'F()']]),
      (2, [['src/maglev/a.cc', 13, 'F()']]),
      (3, [['src/other.cc', 11, 'H()']]),
      (4, [['src/maglev/inl.h', 7, 'F()']]),
      (5, [['src/maglev/a.cc', 42, 'G()']]),
      (6, []),
  ]

  def test_line_and_function_targets(self):
    changed = {'src/maglev/a.cc': [10, 11, 12, 41, 42]}
    targets, functions, mapped = change_targets.compute_targets(
        self.EDGES, changed)
    by_edge = {t['edge']: t['w'] for t in targets}
    # Line 12 has no edge, so the edge on line 13 counts as a line target.
    self.assertEqual(by_edge, {1: 1.0, 5: 1.0, 2: 1.0, 4: 0.3})
    self.assertEqual(functions, ['F()', 'G()'])
    self.assertEqual(mapped, {'src/maglev/a.cc': {11, 42}})
    # Line targets come first.
    self.assertEqual([t['w'] for t in targets][:2], [1.0, 1.0])

  def test_near_lines(self):
    # Line 12 has no edge. The edges on lines 11 and 13 (at most 2 lines
    # away) become line targets, and their function a changed function.
    changed = {'src/maglev/a.cc': [12]}
    targets, functions, mapped = change_targets.compute_targets(
        self.EDGES, changed)
    by_edge = {t['edge']: t['w'] for t in targets}
    self.assertEqual(by_edge, {1: 1.0, 2: 1.0, 4: 0.3})
    self.assertEqual(functions, ['F()'])
    self.assertEqual(mapped, {})

  def test_noise_frames(self):
    edges = self.EDGES + [
        (7, [['src/base/logging.h', 400, 'F()']]),
        (8, [['third_party/libc++/src/include/string', 9, 'F()']]),
        (9, [['src/maglev/a.cc', 0, 'F()']]),
    ]
    changed = {'src/maglev/a.cc': [11]}
    targets, _, _ = change_targets.compute_targets(edges, changed)
    self.assertEqual(sorted(t['edge'] for t in targets), [1, 2, 4])

  def test_cap(self):
    changed = {'src/maglev/a.cc': [11, 42]}
    targets, _, _ = change_targets.compute_targets(
        self.EDGES, changed, max_targets=2)
    self.assertEqual([t['edge'] for t in targets], [1, 5])


class FocusTest(unittest.TestCase):

  def test_prefixes(self):
    changed = {'src/maglev/a.cc': [1], 'src/maglev/x64/b.cc': [2], 'c.h': [3]}
    self.assertEqual(
        change_targets.focus_prefixes(changed, 'auto'), ['src/maglev/'])
    self.assertEqual(change_targets.focus_prefixes(changed, 'none'), [])
    self.assertEqual(
        change_targets.focus_prefixes(changed, 'src/b/,src/a/'),
        ['src/a/', 'src/b/'])

  def test_ranges(self):
    # Edge 3 is in the focus through its function F(), which has an edge in
    # src/maglev/. Edge 6 has no frame.
    edges = [
        (1, [['src/maglev/a.cc', 1, 'F()']]),
        (2, [['src/other.cc', 1, 'H()']]),
        (3, [['src/objects/o-inl.h', 1, 'F()']]),
        (4, [['src/maglev/b.cc', 1, 'G()']]),
        (5, [['src/other.cc', 2, 'H()']]),
        (6, []),
    ]
    collector = change_targets.FocusCollector(['src/maglev/'])
    self.assertEqual(list(collector.wrap(edges)), edges)
    self.assertEqual(collector.ranges(), [[1, 1], [3, 4]])
    empty = change_targets.FocusCollector([])
    list(empty.wrap(edges))
    self.assertEqual(empty.ranges(), [])


class FatalPathTest(unittest.TestCase):
  # The edge at 0x10 runs a check failure. The edge at 0x30 jumps to a call
  # that does not return. The edge at 0x50 branches, so it is live.
  DISASSEMBLY = """\
      10:      \tincb\t0x100(%rip)
      16:      \tleaq\t0x200(%rip), %rdi
      1d:      \tcallq\t0x900 <_Z8V8_FatalPKciS0_z>
      22:      \tud1l\t0x1(%eax), %eax
      30:      \tincb\t0x101(%rip)
      36:      \tjmp\t0x40 <F+0x40>
      40:      \tcallq\t0x950 <_ZN2v84base2OSL5AbortEv>
      45:      \tint3
      50:      \tincb\t0x102(%rip)
      56:      \tcallq\t0x990 <_ZN2v88internal3FooEv>
      5b:      \tjne\t0x10 <F+0x10>
      5d:      \tretq
"""

  def test_classify(self):
    instructions = change_targets.parse_disassembly(self.DISASSEMBLY)
    addresses = [i[0] for i in instructions]
    fatal = [
        pc for pc in (0x10, 0x30, 0x50)
        if change_targets.leads_to_fatal(instructions, addresses, pc)
    ]
    self.assertEqual(fatal, [0x10, 0x30])
    # An address that is not an instruction start is not classified.
    self.assertFalse(
        change_targets.leads_to_fatal(instructions, addresses, 0x11))


class EdgeMapTest(unittest.TestCase):

  def test_normalize(self):
    self.assertEqual(edge_map.normalize('./../../src/d8/d8.cc'), 'src/d8/d8.cc')
    self.assertEqual(
        edge_map.normalize('../../third_party/x.h'), 'third_party/x.h')
    self.assertEqual(edge_map.normalize('gen/a.cc'), 'gen/a.cc')


if __name__ == '__main__':
  unittest.main()
