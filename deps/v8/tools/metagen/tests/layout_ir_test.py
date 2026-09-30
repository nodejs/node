# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Schema tests for the layout IR: validation, determinism, round-trip."""

import json
import os
import sys
import unittest

sys.path.insert(
    0,
    os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from metagen.layout_ir import (Annotation, ClassLayout, Config, Field,
                               LayoutError, CppType, StorageType, Tail,
                               named_type, parse_document, serialize_document,
                               serialize_positions, union_type)

# The configuration the fixtures describe: compressed tagged values on a
# 64-bit target with the sandbox on.
CONFIG = Config(
    tagged_size=4,
    pointer_size=8,
    external_pointer_size=4,
    cpp_heap_pointer_size=4,
    trusted_pointer_size=4)


def document(classes):
  return serialize_document(CONFIG, classes)


def tagged(name, *args):
  return StorageType(kind="tagged", arg=named_type(name, *args))


def int_storage(width, signed=True):
  return StorageType(kind="int", width=width, is_signed=signed)


def field(cpp_name, offset, size, storage=None, cpp_type=None, **kwargs):
  return Field(
      cpp_name=cpp_name,
      cpp_type=cpp_type or "TaggedMember<Object>",
      position="src/objects/test.h:1:1",
      offset=offset,
      size=size,
      storage=storage or tagged("Object"),
      **kwargs)


def cell_layout(**overrides):
  kwargs = dict(
      cpp_name="v8::internal::Cell",
      position="src/objects/cell.h:16:18",
      base_cpp_name="v8::internal::HeapObject",
      base_size=4,
      size=8,
      alignment=4,
      fields=(field("maybe_value_", 4, 4, tagged("MaybeObject")),))
  kwargs.update(overrides)
  return ClassLayout(**kwargs)


class CppTypeTest(unittest.TestCase):

  def test_name_with_args(self):
    t = named_type("TrustedPointer", named_type("BytecodeArray"))
    self.assertEqual(
        t.to_json(), {
            "kind": "name",
            "name": "TrustedPointer",
            "args": [{
                "kind": "name",
                "name": "BytecodeArray"
            }],
        })

  def test_union_roundtrip(self):
    t = union_type(named_type("String"), named_type("Undefined"))
    self.assertEqual(CppType.from_json(t.to_json()), t)

  def test_union_needs_two_members(self):
    with self.assertRaises(LayoutError):
      union_type(named_type("String"))

  def test_rejects_non_identifier_name(self):
    with self.assertRaises(LayoutError):
      named_type("Foo Bar")

  def test_rejects_unknown_kind(self):
    with self.assertRaises(LayoutError):
      CppType.from_json({"kind": "tuple", "members": []})

  def test_rejects_unknown_key(self):
    with self.assertRaises(LayoutError):
      CppType.from_json({"kind": "name", "name": "A", "extra": 1})


class StorageTypeTest(unittest.TestCase):

  def test_unknown_kind_fatal(self):
    with self.assertRaises(LayoutError):
      StorageType(kind="mystery")

  def test_tagged_needs_arg(self):
    with self.assertRaises(LayoutError):
      StorageType(kind="tagged")

  def test_trusted_pointer_needs_tag(self):
    with self.assertRaises(LayoutError):
      StorageType(kind="trusted_pointer", arg=named_type("BytecodeArray"))

  def test_int_width_validated(self):
    with self.assertRaises(LayoutError):
      StorageType(kind="int", width=3, is_signed=True)

  def test_enum_roundtrip(self):
    s = StorageType(kind="enum", name="InstanceType", width=2, is_signed=False)
    self.assertEqual(StorageType.from_json(s.to_json()), s)


class FieldTest(unittest.TestCase):

  def test_array_size_must_be_multiple_of_extent(self):
    with self.assertRaises(LayoutError):
      field("bits_", 4, 10, int_storage(4, signed=False), array_extent=3)

  def test_optional_keys_omitted_from_json(self):
    f = field("maybe_value_", 4, 4)
    self.assertNotIn("annotations", f.to_json())
    self.assertNotIn("array_extent", f.to_json())

  def test_annotations_roundtrip(self):
    f = field(
        "flags_",
        4,
        4,
        int_storage(4, signed=False),
        annotations=(Annotation("V8_TQ_CONST"), Annotation("V8_TQ_RELAXED")))
    self.assertEqual(Field.from_json(f.to_json()), f)

  def test_annotations_sort_and_reject_repeats(self):
    f = field(
        "flags_",
        4,
        4,
        annotations=(Annotation("V8_TQ_RELAXED"), Annotation("V8_TQ_CONST")))
    self.assertEqual([a["name"] for a in f.to_json()["annotations"]],
                     ["V8_TQ_CONST", "V8_TQ_RELAXED"])
    with self.assertRaises(LayoutError):
      Field.from_json({
          "cpp_name": "f_",
          "offset": 4,
          "size": 4,
          "storage": tagged("Object").to_json(),
          "annotations": [{
              "name": "V8_TQ_CONST"
          }, {
              "name": "V8_TQ_CONST"
          }],
      })

  def test_annotation_name_must_be_prefixed(self):
    with self.assertRaises(LayoutError):
      Annotation("CONST")

  def test_unparsed_annotation_arguments_roundtrip(self):
    # Metagen never parses these; a comma inside the argument must
    # survive untouched.
    f = field(
        "values_",
        4,
        8,
        int_storage(4, signed=False),
        array_extent=2,
        annotations=(Annotation("V8_TQ_NAME", "otherValues"),
                     Annotation("V8_TQ_TYPE", "foo::Bar<Baz, Qux>"),
                     Annotation("V8_TQ_EXTENT_NAME", "kNamedExtent")))
    self.assertEqual(Field.from_json(f.to_json()), f)

  def test_empty_annotation_argument_is_fatal(self):
    with self.assertRaises(LayoutError):
      Annotation("V8_TQ_TYPE", "")


class ClassLayoutTest(unittest.TestCase):

  def test_valid_layout(self):
    c = cell_layout()
    self.assertEqual(c.size, 8)

  def test_gap_is_fatal(self):
    with self.assertRaises(LayoutError) as ctx:
      cell_layout(size=12)
    self.assertIn("fields end at 8", str(ctx.exception))

  def test_overlap_is_fatal(self):
    with self.assertRaises(LayoutError):
      cell_layout(
          size=8, fields=(field("a_", 4, 4), field("b_", 6, 2, int_storage(2))))

  def test_field_before_base_is_fatal(self):
    with self.assertRaises(LayoutError):
      cell_layout(fields=(field("early_", 0, 8),))

  def test_unqualified_cpp_name_fatal(self):
    with self.assertRaises(LayoutError):
      cell_layout(cpp_name="Cell")

  def test_duplicate_field_names_fatal(self):
    with self.assertRaises(LayoutError):
      cell_layout(
          size=12, fields=(field("value_", 4, 4), field("value_", 8, 4)))

  def test_root_class_has_no_base(self):
    c = ClassLayout(
        cpp_name="v8::internal::HeapObject",
        position="src/objects/heap-object.h:100:1",
        base_cpp_name=None,
        base_size=0,
        size=4,
        alignment=4,
        fields=(field(
            "map_",
            0,
            4,
            tagged("Map"),
            annotations=(Annotation("V8_TQ_CONST"),)),))
    self.assertIsNone(ClassLayout.from_json(c.to_json()).base_cpp_name)

  def test_non_power_of_two_alignment_fatal(self):
    with self.assertRaises(LayoutError):
      cell_layout(alignment=6)

  def test_tail_offset_must_match_header(self):
    tail = Tail(
        cpp_name="flexible_array_member_data_",
        cpp_element_type="uint8_t",
        position="src/objects/test.h:9:1",
        offset=12,
        element_size=1,
        element_storage=int_storage(1, signed=False))
    with self.assertRaises(LayoutError):
      cell_layout(
          size=8, fields=(field("length_", 4, 4, tagged("Smi")),), tail=tail)

  def test_tail_roundtrip(self):
    tail = Tail(
        cpp_name="flexible_array_member_data_",
        cpp_element_type="uint8_t",
        position="src/objects/test.h:9:1",
        offset=8,
        element_size=1,
        element_storage=int_storage(1, signed=False))
    c = cell_layout(fields=(field("length_", 4, 4, tagged("Smi")),), tail=tail)
    self.assertEqual(ClassLayout.from_json(c.to_json()), c)


class ConfigTest(unittest.TestCase):

  def test_config_is_part_of_the_document(self):
    text = document([cell_layout()])
    self.assertIn('"config"', text)
    self.assertIn('"tagged_size": 4', text)

  def test_rejects_an_implausible_size(self):
    with self.assertRaises(LayoutError):
      Config(
          tagged_size=3,
          pointer_size=8,
          external_pointer_size=4,
          cpp_heap_pointer_size=4,
          trusted_pointer_size=4)

  def test_rejects_tagged_wider_than_pointer(self):
    with self.assertRaises(LayoutError):
      Config(
          tagged_size=8,
          pointer_size=4,
          external_pointer_size=4,
          cpp_heap_pointer_size=4,
          trusted_pointer_size=4)

  def test_missing_config_is_fatal(self):
    doc = json.loads(document([cell_layout()]))
    del doc["config"]
    with self.assertRaises(LayoutError):
      parse_document(json.dumps(doc, indent=1) + "\n")

  def test_unknown_config_key_is_fatal(self):
    doc = json.loads(document([cell_layout()]))
    doc["config"]["smi_shift_size"] = 1
    with self.assertRaises(LayoutError):
      parse_document(json.dumps(doc, indent=1) + "\n")


class CppTypeSpellingTest(unittest.TestCase):

  def test_cpp_type_is_recorded_verbatim(self):
    spelling = "TaggedMember<UnionOf<Smi, JSObject>>"
    text = document(
        [cell_layout(fields=(field("a_", 4, 4, cpp_type=spelling),))])
    self.assertIn(spelling, text)
    _, classes = parse_document(text)
    self.assertEqual(classes[0].fields[0].cpp_type, spelling)

  def test_empty_cpp_type_is_fatal(self):
    # Constructed directly: the `field` helper defaults cpp_type to empty.
    with self.assertRaises(LayoutError):
      Field(
          cpp_name="a_",
          cpp_type="",
          offset=4,
          size=4,
          storage=tagged("Object"))

  def test_cpp_type_is_required(self):
    doc = json.loads(document([cell_layout()]))
    del doc["classes"][0]["fields"][0]["cpp_type"]
    with self.assertRaises(LayoutError):
      parse_document(json.dumps(doc, indent=1) + "\n")


class DocumentTest(unittest.TestCase):

  def test_serialization_is_input_order_independent(self):
    a = cell_layout()
    b = cell_layout(cpp_name="v8::internal::AccessorPair")
    self.assertEqual(document([a, b]), document([b, a]))
    text = document([a, b])
    self.assertLess(text.index("AccessorPair"), text.index("Cell"))

  def test_classes_emit_in_inheritance_preorder(self):
    root = cell_layout(
        cpp_name="v8::internal::HeapObject",
        base_cpp_name=None,
        base_size=0,
        size=4,
        fields=(field("map_", 0, 4),))
    zebra = cell_layout(cpp_name="v8::internal::Zebra")
    aardvark = cell_layout(cpp_name="v8::internal::Aardvark")
    # Derived off Zebra, so it precedes the alphabetically earlier Aardvark.
    stripe = cell_layout(
        cpp_name="v8::internal::Stripe",
        base_cpp_name="v8::internal::Zebra",
        base_size=8,
        size=12,
        fields=(field("extra_", 8, 4),))
    text = document([aardvark, stripe, root, zebra])
    self.assertEqual([c["cpp_name"] for c in json.loads(text)["classes"]], [
        "v8::internal::HeapObject", "v8::internal::Aardvark",
        "v8::internal::Zebra", "v8::internal::Stripe"
    ])

  def test_classes_with_an_absent_base_root_their_own_tree(self):
    # No HeapObject layout: both classes name a base outside the set.
    text = document(
        [cell_layout(cpp_name="v8::internal::Zebra"),
         cell_layout()])
    self.assertLess(text.index("Cell"), text.index("Zebra"))

  def test_base_chain_cycle_fatal(self):
    a = cell_layout(cpp_name="v8::internal::A", base_cpp_name="v8::internal::B")
    b = cell_layout(cpp_name="v8::internal::B", base_cpp_name="v8::internal::A")
    with self.assertRaises(LayoutError):
      document([a, b])

  def test_duplicate_class_fatal(self):
    with self.assertRaises(LayoutError):
      document([cell_layout(), cell_layout()])

  def test_roundtrip(self):
    text = document([cell_layout()])
    config, classes = parse_document(text)
    self.assertEqual(config, CONFIG)
    self.assertEqual(serialize_document(config, classes), text)

  def test_torque_names_are_not_serialized(self):
    text = document([cell_layout()])
    doc = json.loads(text)
    self.assertNotIn("torque_name", text)
    self.assertEqual(doc["classes"][0]["base"], "v8::internal::HeapObject")
    self.assertEqual(doc["classes"][0]["fields"][0]["cpp_name"], "maybe_value_")

  def test_positions_split_from_layouts(self):
    self.assertNotIn("position", document([cell_layout()]))
    positions = json.loads(serialize_positions([cell_layout()]))
    (c,) = positions["classes"]
    self.assertEqual(c["cpp_name"], "v8::internal::Cell")
    self.assertEqual(c["position"], "src/objects/cell.h:16:18")
    self.assertEqual(c["members"], {"maybe_value_": "src/objects/test.h:1:1"})

  def test_positions_include_the_tail(self):
    import json
    tail = Tail(
        cpp_name="flexible_array_member_data_",
        cpp_element_type="uint8_t",
        position="src/objects/test.h:9:1",
        offset=8,
        element_size=1,
        element_storage=int_storage(1, signed=False))
    c = cell_layout(fields=(field("length_", 4, 4, tagged("Smi")),), tail=tail)
    positions = json.loads(serialize_positions([c]))
    self.assertEqual(
        positions["classes"][0]["members"], {
            "length_": "src/objects/test.h:1:1",
            "flexible_array_member_data_": "src/objects/test.h:9:1",
        })

  def test_positions_require_a_position(self):
    with self.assertRaises(LayoutError):
      serialize_positions([cell_layout(position=None)])

  def test_trailing_newline(self):
    self.assertTrue(document([]).endswith("}\n"))

  def test_version_mismatch_fatal(self):
    text = document([]).replace('"schema_version": 1', '"schema_version": 99')
    with self.assertRaises(LayoutError) as ctx:
      parse_document(text)
    self.assertIn("schema version", str(ctx.exception))

  def test_non_canonical_input_fatal(self):
    # Same JSON value, different formatting: not canonical.
    import json
    text = document([cell_layout()])
    reformatted = json.dumps(json.loads(text), indent=4) + "\n"
    with self.assertRaises(LayoutError):
      parse_document(reformatted)

  def test_unknown_document_key_fatal(self):
    import json
    doc = json.loads(document([]))
    doc["extras"] = []
    with self.assertRaises(LayoutError):
      parse_document(json.dumps(doc, indent=1) + "\n")


if __name__ == "__main__":
  unittest.main()
