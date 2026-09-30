#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for the instance-type solver and macro emission.

The expectations are Torque's: the numbering rules from
src/torque/instance-type-generator.cc, the capification cases from
CapifyStringWithUnderscores (src/torque/utils.cc). A divergence in the
port shows up here, not as a --check diff against an emission that goes
away with the Torque path.
"""

import contextlib
import io
import os
import sys
import unittest

_TOOLS = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _TOOLS not in sys.path:
  sys.path.insert(0, _TOOLS)

from metagen import instance_types as it  # noqa: E402


def cls(name, base, **kwargs):
  """A ClassInfo with the fields no test cares about filled in."""
  info = it.ClassInfo(
      name=name,
      base=base,
      position=f"{name.lower()}.h:1:1",
      is_abstract=kwargs.pop("abstract", False),
      has_same_instance_type_as_parent=kwargs.pop("same_type_as_parent", False),
      highest_within_parent=kwargs.pop("highest", False),
      lowest_within_parent=kwargs.pop("lowest", False),
      constraints=it.InstanceTypeConstraints(
          value=kwargs.pop("value", -1),
          num_flags_bits=kwargs.pop("flags_bits", -1)))
  # What is left names a ClassInfo field directly. An unknown one is a
  # typo, not a silent no-op.
  for field, value in kwargs.items():
    if not hasattr(info, field):
      raise TypeError(f"ClassInfo has no field {field}")
    setattr(info, field, value)
  return info


def own_values(root):
  """Map each class holding an instance-type value to that value."""
  found = {}

  def walk(node):
    if node.num_own_values == 1:
      found[node.cls.name] = node.value
    for child in node.children:
      walk(child)

  walk(root)
  return found


def ranges(root):
  """Map each class in the solved tree to its (start, end) range."""
  found = {}

  def walk(node):
    found[node.cls.name] = (node.start, node.end)
    for child in node.children:
      walk(child)

  walk(root)
  return found


class MacroText:
  """The emitted macros, split into entries per macro name.

  A macro that was not emitted raises on lookup instead of reporting no
  entries, which would let an assertNotIn pass for the wrong reason.
  """

  def __init__(self, text):
    self.text = text
    self.macros = {}
    for block in text.split("\n\n"):
      lines = block.strip("\n").split("\n")
      # Each block is a comment, then the #define, then its entries.
      heads = [
          i for i, line in enumerate(lines)
          if line.startswith("#define ") and line.endswith("(V) \\")
      ]
      if not heads:
        continue
      head = lines[heads[0]]
      name = head[len("#define "):-len("(V) \\")]
      entries = []
      for line in lines[heads[0] + 1:]:
        # Every entry line continues the macro, the last one included.
        assert line.endswith(" \\"), f"{name}: unterminated line {line!r}"
        entries.append(line[:-2].strip())
      self.macros[name] = entries

  def __getitem__(self, macro):
    if macro not in self.macros:
      raise AssertionError(
          f"{macro} not emitted; got {sorted(self.macros)}\n{self.text}")
    return self.macros[macro]


class SolverTest(unittest.TestCase):

  def test_numbering_is_dense_from_zero(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Beta", "HeapObject"),
        cls("Alpha", "HeapObject"),
    ])
    # Equal-sized siblings fall to the name tie-break.
    self.assertEqual(own_values(root), {"Alpha": 0, "Beta": 1})
    self.assertEqual((root.start, root.end), (0, 1))

  def test_bigger_subtrees_are_placed_first(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Zoo", "HeapObject", abstract=True),
        cls("ZooA", "Zoo"),
        cls("ZooB", "Zoo"),
        cls("Ant", "HeapObject"),
    ])
    # Zoo spans two values, so it sorts ahead of Ant despite the name.
    self.assertEqual(own_values(root), {"ZooA": 0, "ZooB": 1, "Ant": 2})

  def test_lowest_and_highest_bracket_their_siblings(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Middle", "HeapObject"),
        cls("Zebra", "HeapObject", lowest=True),
        cls("Aardvark", "HeapObject", highest=True),
    ])
    # Both markers win over the name tie-break.
    self.assertEqual(own_values(root), {"Zebra": 0, "Middle": 1, "Aardvark": 2})

  def test_two_lowest_children_are_fatal(self):
    stderr = io.StringIO()
    with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as e:
      it.assign_instance_types([
          cls("HeapObject", None, abstract=True),
          cls("First", "HeapObject", lowest=True),
          cls("Second", "HeapObject", lowest=True),
      ])
    self.assertEqual(e.exception.code, 1)
    self.assertIn("two lowest children", stderr.getvalue())

  def test_abstract_class_spans_its_children(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Trailing", "HeapObject"),
        cls("Group", "HeapObject", abstract=True),
        cls("GroupA", "Group"),
        cls("GroupB", "Group"),
    ])
    solved = ranges(root)
    self.assertEqual(
        own_values(root), {
            "GroupA": 0,
            "GroupB": 1,
            "Trailing": 2
        })
    # An abstract class holds no value of its own, and its range covers
    # its children exactly. The emitted FIRST_/LAST_ check reads that
    # range.
    self.assertNotIn("Group", own_values(root))
    self.assertEqual(solved["Group"], (0, 1))
    self.assertEqual(solved["HeapObject"], (0, 2))

  def test_explicit_value_is_honored(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Filler", "HeapObject"),
        cls("Pinned", "HeapObject", value=17),
    ])
    self.assertEqual(own_values(root)["Pinned"], 17)

  def test_explicit_value_survives_unconstrained_siblings(self):
    # Constrained children go first, so the fillers flow around the
    # pinned value instead of squatting on it.
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("AFiller", "HeapObject"),
        cls("BFiller", "HeapObject"),
        cls("Pinned", "HeapObject", value=0),
    ])
    self.assertEqual(
        own_values(root), {
            "Pinned": 0,
            "AFiller": 1,
            "BFiller": 2
        })

  def test_colliding_explicit_values_are_fatal(self):
    stderr = io.StringIO()
    with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as e:
      it.assign_instance_types([
          cls("HeapObject", None, abstract=True),
          cls("PinnedA", "HeapObject", value=5),
          cls("PinnedB", "HeapObject", value=5),
      ])
    self.assertEqual(e.exception.code, 1)
    self.assertIn("PinnedB", stderr.getvalue())

  def test_type_shared_with_parent_gets_no_value(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Owner", "HeapObject"),
        cls("Sharer", "Owner", same_type_as_parent=True),
    ])
    self.assertEqual(own_values(root), {"Owner": 0})
    self.assertEqual(
        MacroText(it.emit_enum_macros(root))["TORQUE_ASSIGNED_INSTANCE_TYPES"],
        [
            "V(FIRST_HEAP_OBJECT_TYPE, 0)", "V(FIRST_OWNER_TYPE, 0)",
            "V(OWNER_TYPE, 0)", "V(LAST_OWNER_TYPE, 0)",
            "V(LAST_HEAP_OBJECT_TYPE, 0)"
        ])

  def test_flag_class_reserves_a_power_of_two(self):
    root = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Flagged", "HeapObject", flags_bits=2),
        cls("Shadowed", "Flagged"),
        cls("After", "HeapObject"),
    ])
    solved = ranges(root)
    self.assertEqual(solved["Flagged"], (0, 3))
    # Subclasses share the flag bits, so they leave the tree.
    self.assertNotIn("Shadowed", solved)
    self.assertEqual(own_values(root)["After"], 4)

  def test_multiple_roots_are_fatal(self):
    stderr = io.StringIO()
    with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as e:
      it.assign_instance_types([
          cls("HeapObject", None, abstract=True),
          cls("Stray", "NotInTheHarvest"),
      ])
    self.assertEqual(e.exception.code, 1)
    self.assertIn("multiple roots", stderr.getvalue())

  def test_no_classes_yields_no_tree(self):
    self.assertIsNone(it.assign_instance_types([]))


class EmissionTest(unittest.TestCase):

  def setUp(self):
    self.tree = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Abstract", "HeapObject", abstract=True),
        cls("AbstractA", "Abstract"),
        cls("AbstractB", "Abstract"),
        cls("Family", "HeapObject", has_printer=True, has_verifier=True),
        cls("Child", "Family"),
        cls("Loner", "HeapObject", has_printer=True, has_verifier=True),
    ])
    # Pin the layout the expectations below are written against.
    self.assertEqual(
        own_values(self.tree), {
            "AbstractA": 0,
            "AbstractB": 1,
            "Family": 2,
            "Child": 3,
            "Loner": 4
        })

  def test_every_type_gets_first_and_last_markers(self):
    macros = MacroText(it.emit_enum_macros(self.tree))
    entries = macros["TORQUE_ASSIGNED_INSTANCE_TYPES"]
    self.assertIn("V(FIRST_ABSTRACT_TYPE, 0)", entries)
    self.assertIn("V(LAST_ABSTRACT_TYPE, 1)", entries)
    # The markers alias the value for a single-value class, and
    # generated CSA reads them either way.
    self.assertIn("V(FIRST_LONER_TYPE, 4)", entries)
    self.assertIn("V(LONER_TYPE, 4)", entries)
    self.assertIn("V(LAST_LONER_TYPE, 4)", entries)
    # The flat list carries the concrete types only.
    values_list = macros["TORQUE_ASSIGNED_INSTANCE_TYPE_LIST"]
    self.assertIn("V(LONER_TYPE)", values_list)
    self.assertNotIn("V(ABSTRACT_TYPE)", values_list)

  def test_flag_class_has_no_last_marker(self):
    tree = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Flagged", "HeapObject", flags_bits=2),
    ])
    entries = MacroText(
        it.emit_enum_macros(tree))["TORQUE_ASSIGNED_INSTANCE_TYPES"]
    self.assertIn("V(FIRST_FLAGGED_TYPE, 0)", entries)
    self.assertNotIn("V(LAST_FLAGGED_TYPE, 3)", entries)

  def test_buckets_split_single_multiple_and_range(self):
    macros = MacroText(it.emit_bucket_macros(self.tree))
    # SINGLE: an own value, and nothing else in the range.
    self.assertEqual(macros["INSTANCE_TYPE_LIST_SINGLE"], [
        "V(AbstractA, ABSTRACT_A_TYPE)", "V(AbstractB, ABSTRACT_B_TYPE)",
        "V(Child, CHILD_TYPE)", "V(Loner, LONER_TYPE)"
    ])
    # MULTIPLE: an own value, and a subtree that spans more.
    self.assertEqual(macros["INSTANCE_TYPE_LIST_MULTIPLE"],
                     ["V(Family, FAMILY_TYPE)"])
    # RANGE: no own value, and a span of several.
    self.assertEqual(macros["INSTANCE_TYPE_LIST_RANGE"], [
        "V(Abstract, FIRST_ABSTRACT_TYPE, LAST_ABSTRACT_TYPE)",
        "V(Family, FIRST_FAMILY_TYPE, LAST_FAMILY_TYPE)"
    ])

  def test_root_gets_no_range_entry(self):
    # The root's check would be a tautology; Torque skips it.
    self.assertNotIn(
        "V(HeapObject, FIRST_HEAP_OBJECT_TYPE, LAST_HEAP_OBJECT_TYPE)",
        MacroText(it.emit_bucket_macros(self.tree))["INSTANCE_TYPE_LIST_RANGE"])

  def test_no_auto_checker_keeps_the_type_but_drops_the_check(self):
    tree = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Hidden", "HeapObject", no_auto_checker=True),
        cls("Visible", "HeapObject"),
    ])
    self.assertIn(
        "V(HIDDEN_TYPE, 0)",
        MacroText(it.emit_enum_macros(tree))["TORQUE_ASSIGNED_INSTANCE_TYPES"])
    self.assertEqual(
        MacroText(it.emit_bucket_macros(tree))["INSTANCE_TYPE_LIST_SINGLE"],
        ["V(Visible, VISIBLE_TYPE)"])

  def test_dispatch_needs_a_printer_and_a_verifier(self):
    tree = it.assign_instance_types([
        cls("HeapObject", None, abstract=True),
        cls("Both", "HeapObject", has_printer=True, has_verifier=True),
        cls("PrinterOnly", "HeapObject", has_printer=True),
        cls("VerifierOnly", "HeapObject", has_verifier=True),
        cls("HandWritten",
            "HeapObject",
            has_printer=True,
            has_verifier=True,
            no_auto_dispatch=True),
    ])
    macros = MacroText(it.emit_dispatch_macro(tree))
    self.assertEqual(macros["HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST"],
                     ["V(Both, BOTH_TYPE)"])

  def test_empty_tree_still_defines_every_macro(self):
    for emit, macros in ((it.emit_enum_macros, [
        "TORQUE_ASSIGNED_INSTANCE_TYPES", "TORQUE_ASSIGNED_INSTANCE_TYPE_LIST"
    ]), (it.emit_bucket_macros, [
        "INSTANCE_TYPE_LIST_SINGLE", "INSTANCE_TYPE_LIST_MULTIPLE",
        "INSTANCE_TYPE_LIST_RANGE"
    ]), (it.emit_dispatch_macro, ["HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST"])):
      emitted = MacroText(emit(None))
      for macro in macros:
        self.assertEqual(emitted[macro], [], macro)


class CapifyTest(unittest.TestCase):

  # Ground truth: CapifyStringWithUnderscores (src/torque/utils.cc).
  CASES = {
      "Code": "CODE",
      "ArrayList": "ARRAY_LIST",
      "AsyncGeneratorRequest": "ASYNC_GENERATOR_REQUEST",
      "SmallOrderedHashMap": "SMALL_ORDERED_HASH_MAP",
      # A digit closes a word.
      "Uint8TypedArrayConstructor": "UINT8_TYPED_ARRAY_CONSTRUCTOR",
      "Tuple2": "TUPLE2",
      # JSAbc yields JS_ABC for any Abc, wherever the JS sits.
      "JSObject": "JS_OBJECT",
      "JSWeakRef": "JS_WEAK_REF",
      "WasmJSFunction": "WASM_JS_FUNCTION",
      # A longer capital run after the JS stays one word.
      "JSAPIObjectWithEmbedderSlots": "JS_APIOBJECT_WITH_EMBEDDER_SLOTS",
      # Dots and dashes separate words: file names reach this too.
      "instance-types.h": "INSTANCE_TYPES_H",
  }

  def test_matches_torque(self):
    for name, expected in self.CASES.items():
      self.assertEqual(it.capify_with_underscores(name), expected, name)


if __name__ == "__main__":
  unittest.main()
