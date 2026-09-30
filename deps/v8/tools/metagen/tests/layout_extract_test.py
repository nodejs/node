# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extractor tests against small synthetic translation units.

Each test parses a C++ snippet with stand-ins for V8 object macros and
member wrappers using the bundled libclang. The tests use the production
code for class discovery, base resolution, and layout extraction.
"""

import os
import sys
import unittest

_TOOLS_DIR = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_V8_ROOT = os.path.dirname(_TOOLS_DIR)
sys.path.insert(0, _TOOLS_DIR)

from metagen import clang_bootstrap

clang_bootstrap.bootstrap(
    libclang_dir=os.path.join(_V8_ROOT, "third_party", "llvm-libclang"))

import clang.cindex as cindex  # noqa: E402

from metagen import cpp_hier  # noqa: E402
from metagen import layout_extract  # noqa: E402
from metagen import layout_ir  # noqa: E402
from metagen.layout_ir import (
    LayoutError,
    named_type,  # noqa: E402
    serialize_document,
    serialize_positions)

# A fake of the V8 object macros as V8_METAGEN_GENERATION_PASS expands
# them, member wrappers with layout-compatible stand-in storage (4-byte
# handles, as in compressed-pointer builds), and the alias/union type
# stubs. Every concrete class below that reaches HeapObject participates
# in layout export automatically.
_PRELUDE = r"""
#define V8_TQ_ANNOTATE(P) [[clang::annotate(P)]]
#define V8_TQ_MARK(NAME, PAYLOAD) \
  using V8TQ_##NAME V8_TQ_ANNOTATE(PAYLOAD) = void
#define V8_TQ_TAIL_NAME(N) \
  V8_TQ_MARK(TqTailName, "V8_TQ_TAIL_NAME(" #N ")")
#define V8_TQ_TAIL_LENGTH(F) \
  V8_TQ_MARK(TqTailLength, "V8_TQ_TAIL_LENGTH(" #F ")")
#define V8_TQ_CONST V8_TQ_ANNOTATE("V8_TQ_CONST")
#define V8_TQ_RELAXED V8_TQ_ANNOTATE("V8_TQ_RELAXED")
#define V8_TQ_ACQ_REL V8_TQ_ANNOTATE("V8_TQ_ACQ_REL")
#define V8_TQ_CUSTOM_WEAK V8_TQ_ANNOTATE("V8_TQ_CUSTOM_WEAK")
#define V8_TQ_TYPE(T) V8_TQ_ANNOTATE("V8_TQ_TYPE(" #T ")")
#define V8_TQ_NAME(N) V8_TQ_ANNOTATE("V8_TQ_NAME(" #N ")")

using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using int32_t = int;
using int16_t = short;

namespace std {
template <typename T>
class atomic {
  T value_;
};
}  // namespace std

namespace v8 {
namespace internal {

using IndirectPointerHandle = uint32_t;

class Map;
class Smi;
class String;
class Undefined;

// Modelled as in globals.h: the schemes are aliases for an impl
// template parameterised by a cage, which is what the extraction
// matches on.
template <typename Cage>
class V8HeapCompressionSchemeImpl;
class MainCage;
class TrustedCage;
using V8HeapCompressionScheme = V8HeapCompressionSchemeImpl<MainCage>;
using TrustedSpaceCompressionScheme = V8HeapCompressionSchemeImpl<TrustedCage>;

template <typename T, typename CompressionScheme = V8HeapCompressionScheme>
class TaggedMember {
  uint32_t storage_;
};
template <typename T>
using ProtectedTaggedMember = TaggedMember<T, TrustedSpaceCompressionScheme>;

enum IndirectPointerTag { kTestBytecodeTag, kTestCodeTag };
enum ExternalPointerTag { kTestCallbackTag };
template <typename Tag>
struct TagRange {
  constexpr TagRange(Tag tag) : first(tag), last(tag) {}
  constexpr TagRange(Tag first, Tag last) : first(first), last(last) {}
  Tag first;
  Tag last;
};
using ExternalPointerTagRange = TagRange<ExternalPointerTag>;
using IndirectPointerTagRange = TagRange<IndirectPointerTag>;
template <typename T, IndirectPointerTagRange tag>
class TrustedPointerMember {
  uint32_t handle_;
};
template <ExternalPointerTagRange tag>
class ExternalPointerMember {
  uint32_t handle_;
};

template <typename... Ts>
class Union {};
template <typename... Ts>
using UnionOf = Union<Ts...>;
template <typename T>
class Weak {};
template <typename T>
using MaybeWeak = UnionOf<T, Weak<T>>;

class HeapObject;
using MaybeObject = Union<Smi, HeapObject, Weak<HeapObject>>;

class HeapObject {
 public:
  TaggedMember<Map> map_;
};

}  // namespace internal
}  // namespace v8
"""

_PARSE_OPTIONS = (
    cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
    | cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)

_INDEX = cindex.Index.create()

# Absolute so the v8-root-relative paths come out right.
_V8_ROOT = "/nonexistent-v8-root"


def parse(body: str,
          extra_headers=None,
          *,
          prelude=_PRELUDE) -> cpp_hier.ParsedTU:
  """Parse a snippet. `extra_headers` maps a v8-root-relative path to
  its contents and prepends an #include of it, for the checks that read
  which header a declaration came from."""
  includes = "".join(
      f'#include "{_V8_ROOT}/{path}"\n' for path in (extra_headers or {}))
  source = prelude + includes + "namespace v8 { namespace internal {\n" + \
      body + "\n} }\n"
  unsaved = [("metagen_layout_test.cc", source)]
  for path, contents in (extra_headers or {}).items():
    unsaved.append((f"{_V8_ROOT}/{path}",
                    "#pragma once\nnamespace v8 { namespace internal {\n" +
                    contents + "\n} }\n"))
  tu = _INDEX.parse(
      "metagen_layout_test.cc",
      args=["-x", "c++", "-std=c++20", "-fsyntax-only"],
      unsaved_files=unsaved,
      options=_PARSE_OPTIONS)
  errors = [d for d in tu.diagnostics if d.severity >= cindex.Diagnostic.Error]
  if errors:
    raise AssertionError("test snippet does not compile: " +
                         "; ".join(str(d) for d in errors))
  return cpp_hier._harvest_classes(tu, _V8_ROOT).parsed


def extract_layouts(body: str, extra_headers=None):
  h = parse(body, extra_headers)
  return {
      r.cpp_name.rsplit("::", 1)[-1]: r
      for r in layout_extract.extract_layouts(h, _V8_ROOT)
  }


class SimpleClassTest(unittest.TestCase):

  def test_cell_analog(self):
    layouts = extract_layouts("""
      class Cell : public HeapObject {
       public:
        TaggedMember<MaybeObject> maybe_value_;
      };
    """)
    self.assertEqual(set(layouts), {"HeapObject", "Cell"})
    cell = layouts["Cell"]
    self.assertEqual(cell.cpp_name, "v8::internal::Cell")
    self.assertEqual(cell.base_cpp_name, "v8::internal::HeapObject")
    self.assertEqual(cell.base_size, 4)
    self.assertEqual(cell.size, 8)
    (f,) = cell.fields
    self.assertEqual(f.cpp_name, "maybe_value_")
    self.assertEqual(f.annotations, ())
    self.assertEqual(f.offset, 4)
    self.assertEqual(f.size, 4)
    self.assertEqual(f.storage.kind, "tagged")
    # The alias name is kept; the canonical Union<...> is not used.
    self.assertEqual(f.storage.arg, named_type("MaybeObject"))

  def test_heap_object_root(self):
    layouts = extract_layouts("")
    root = layouts["HeapObject"]
    self.assertIsNone(root.base_cpp_name)
    self.assertEqual(root.base_size, 0)
    self.assertEqual(root.size, 4)
    (map_field,) = root.fields
    self.assertEqual(map_field.cpp_name, "map_")
    self.assertEqual(map_field.storage.arg, named_type("Map"))

  def test_serializes(self):
    layouts = extract_layouts("""
      class Cell : public HeapObject {
       public:
        TaggedMember<MaybeObject> maybe_value_;
      };
    """)
    text = serialize_document(
        layout_ir.Config(
            tagged_size=4,
            pointer_size=8,
            external_pointer_size=4,
            cpp_heap_pointer_size=4,
            trusted_pointer_size=4), list(layouts.values()))
    self.assertIn('"schema_version": 1', text)
    self.assertIn('"cpp_type": "TaggedMember<MaybeObject>"', text)
    self.assertNotIn('"position"', text)
    positions = serialize_positions(list(layouts.values()))
    self.assertIn('"maybe_value_"', positions)

  def test_raw_enum_and_atomic_fields(self):
    layouts = extract_layouts("""
      enum class FooKind : uint16_t { kA, kB };
      class Foo : public HeapObject {
       public:
        int32_t frame_size_;
        uint16_t parameter_count_;
        FooKind kind_;
        std::atomic<uint32_t> usage_;
      };
    """)
    foo = layouts["Foo"]
    kinds = [(f.cpp_name, f.storage.kind, f.storage.width) for f in foo.fields]
    self.assertEqual(kinds, [
        ("frame_size_", "int", 4),
        ("parameter_count_", "int", 2),
        ("kind_", "enum", 2),
        ("usage_", "int", 4),
    ])
    frame_size, parameter_count, kind, usage = foo.fields
    self.assertTrue(frame_size.storage.is_signed)
    self.assertFalse(parameter_count.storage.is_signed)
    self.assertFalse(usage.storage.is_signed)
    self.assertEqual(kind.storage.name, "FooKind")
    self.assertTrue(foo.fields[0].storage.is_signed)
    self.assertFalse(foo.fields[3].storage.is_signed)
    self.assertEqual(foo.fields[2].storage.name, "FooKind")

  def _uintptr_layouts(self, uintptr_definition):
    # A stand-in with the member-wrapper shape of UnalignedValueMember.
    # The Smi slot keeps the layout contiguous when the 8-byte members
    # need 8-byte alignment after the 4-byte HeapObject header.
    return extract_layouts(f"""
      {uintptr_definition}
      template <typename T>
      class UnalignedValueMember {{ T value_; }};
      class Foo : public HeapObject {{
       public:
        TaggedMember<Smi> slot_;
        UnalignedValueMember<uintptr_t> wrapped_;
        uintptr_t bare_;
      }};
    """)

  def test_uintptr_alias_wins_over_msvc_canonical_kind(self):
    # MSVC x64: uintptr_t canonicalizes to unsigned long long; the
    # alias must still be recognized as pointer-width.
    layouts = self._uintptr_layouts("using uintptr_t = unsigned long long;")
    _, wrapped, bare = layouts["Foo"].fields
    self.assertEqual(wrapped.storage.arg, named_type("uintptr_t"))
    self.assertEqual(bare.storage.pointer_width_alias, "uintptr_t")
    self.assertEqual(bare.storage.width, 8)

  def test_uintptr_alias_wins_over_ilp32_canonical_kind(self):
    # ILP32: uintptr_t canonicalizes to unsigned int. The width alone
    # would say uint32, which is why the alias is recorded by name.
    layouts = self._uintptr_layouts("using uintptr_t = unsigned int;")
    _, wrapped, bare = layouts["Foo"].fields
    self.assertEqual(wrapped.storage.arg, named_type("uintptr_t"))
    self.assertEqual(bare.storage.pointer_width_alias, "uintptr_t")
    self.assertEqual(bare.storage.width, 4)

  def test_no_torque_names_reach_the_layout(self):
    # The schema preserves C++ type names for consumers to map.
    layouts = extract_layouts("""
      using uintptr_t = unsigned long long;
      class UnalignedDoubleMember { double value_; };
      template <typename T>
      class UnalignedValueMember { T value_; };
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        UnalignedDoubleMember d_;
        UnalignedValueMember<uintptr_t> p_;
      };
    """)
    _, d, p = layouts["Foo"].fields
    self.assertEqual(d.storage.arg, named_type("double"))
    self.assertEqual(p.storage.arg, named_type("uintptr_t"))

  def test_fixed_width_ints_keep_canonical_mapping(self):
    layouts = extract_layouts("""
      using uint64_t = unsigned long long;
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        uint64_t u64_;
      };
    """)
    _, u64 = layouts["Foo"].fields
    self.assertEqual((u64.storage.width, u64.storage.is_signed), (8, False))
    self.assertIsNone(u64.storage.pointer_width_alias)

  def test_chained_alias_resolves_to_the_pointer_width_one(self):
    # Word reaches uintptr_t only through the desugaring chain.
    layouts = extract_layouts("""
      using uintptr_t = unsigned long long;
      using Word = uintptr_t;
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        Word bare_;
      };
    """)
    _, bare = layouts["Foo"].fields
    self.assertEqual(bare.storage.pointer_width_alias, "uintptr_t")

  def test_address_wins_over_the_alias_it_desugars_to(self):
    layouts = extract_layouts("""
      using uintptr_t = unsigned long long;
      using Address = uintptr_t;
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        Address bare_;
      };
    """)
    _, bare = layouts["Foo"].fields
    self.assertEqual(bare.storage.pointer_width_alias, "Address")

  def test_size_t_is_recorded_by_its_own_name(self):
    layouts = extract_layouts("""
      using size_t = unsigned long long;
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        size_t size_;
      };
    """)
    _, size = layouts["Foo"].fields
    self.assertEqual(size.storage.pointer_width_alias, "size_t")

  def test_unrecognized_long_has_no_alias(self):
    # No alias in the chain is pointer-width, so the member is
    # described by its width alone.
    layouts = extract_layouts("""
      class Foo : public HeapObject {
       public:
        TaggedMember<Smi> slot_;
        unsigned long raw_;
      };
    """)
    _, raw = layouts["Foo"].fields
    self.assertIsNone(raw.storage.pointer_width_alias)

  def test_union_weak_and_alias_template_flattening(self):
    layouts = extract_layouts("""
      class Bar : public HeapObject {
       public:
        TaggedMember<UnionOf<String, Undefined>> name_;
        TaggedMember<Weak<Map>> weak_map_;
        TaggedMember<UnionOf<String, MaybeWeak<Map>>> mixed_;
      };
    """)
    name, weak_map, mixed = layouts["Bar"].fields
    self.assertEqual(name.storage.arg.kind, "union")
    self.assertEqual([m.name for m in name.storage.arg.members],
                     ["String", "Undefined"])
    self.assertEqual(weak_map.storage.arg, named_type("Weak",
                                                      named_type("Map")))
    # MaybeWeak<Map> is flattened into the enclosing union.
    self.assertEqual(mixed.storage.arg.members,
                     (named_type("String"), named_type("Map"),
                      named_type("Weak", named_type("Map"))))

  def test_known_class_template_argument_names_the_template(self):
    layouts = extract_layouts("""
      template <typename T>
      class CppGCManaged : public HeapObject {};
      class Holder : public HeapObject {
       public:
        TaggedMember<CppGCManaged<Map>> wrapped_;
      };
    """)
    wrapped, = layouts["Holder"].fields
    # CppType names the template; consumers map it to a type of their
    # own (kClassTemplateNames in layout-loader.cc).
    self.assertEqual(wrapped.storage.arg, named_type("CppGCManaged"))
    self.assertEqual(wrapped.cpp_type, "TaggedMember<CppGCManaged<Map>>")

  def test_sandbox_pointer_members(self):
    layouts = extract_layouts("""
      class BytecodeArray;
      class Code;
      using CodePointerMember = TrustedPointerMember<Code, kTestCodeTag>;
      class Baz : public HeapObject {
       public:
        TrustedPointerMember<BytecodeArray, kTestBytecodeTag> bytecode_;
        ExternalPointerMember<kTestCallbackTag> callback_;
        ProtectedTaggedMember<BytecodeArray> protected_;
        CodePointerMember code_;
      };
    """)
    bytecode, callback, protected, code = layouts["Baz"].fields
    self.assertEqual(bytecode.storage.kind, "trusted_pointer")
    self.assertEqual(bytecode.storage.tag, "kTestBytecodeTag")
    self.assertEqual(bytecode.storage.arg, named_type("BytecodeArray"))
    self.assertEqual(callback.storage.kind, "external_pointer")
    self.assertEqual(callback.storage.tag, "kTestCallbackTag")
    self.assertEqual(protected.storage.kind, "protected_tagged")
    self.assertEqual(protected.storage.arg, named_type("BytecodeArray"))
    # The tag reference lives on the alias declaration.
    self.assertEqual(code.storage.tag, "kTestCodeTag")

  def test_fixed_array_and_struct_members(self):
    layouts = extract_layouts("""
      struct CoverageInfoSlot {
        int32_t start;
        int32_t end;
      };
      class Qux : public HeapObject {
       public:
        uint32_t bitset_[2];
        CoverageInfoSlot slot_;
      };
    """)
    bitset, slot = layouts["Qux"].fields
    self.assertEqual(bitset.array_extent, 2)
    self.assertEqual(bitset.size, 8)
    self.assertEqual(bitset.storage.kind, "int")
    self.assertEqual(slot.storage.kind, "struct")
    self.assertEqual(slot.storage.name, "CoverageInfoSlot")
    self.assertEqual(slot.storage.width, 8)

  def test_pointer_tag_ranges(self):
    layouts = extract_layouts("""
      constexpr ExternalPointerTagRange kCallbackRange(kTestCallbackTag);
      constexpr IndirectPointerTagRange kCodeRange(kTestBytecodeTag,
                                                  kTestCodeTag);
      class Callback : public HeapObject {
       public:
        ExternalPointerMember<kCallbackRange> callback_;
        TrustedPointerMember<HeapObject, kCodeRange> code_;
      };
    """)
    callback, code = layouts["Callback"].fields
    self.assertEqual(callback.storage.kind, "external_pointer")
    self.assertEqual(callback.storage.tag, "kCallbackRange")
    self.assertEqual(code.storage.kind, "trusted_pointer")
    self.assertEqual(code.storage.tag, "kCodeRange")

  def test_pointer_tags_ignore_unrelated_references(self):
    h = parse("""
      enum OtherPointerTag { kOtherTag };
      namespace other {
        enum ExternalPointerTag { kForeignTag };
        template <typename T> struct TagRange { T value; };
        constexpr TagRange<int> kForeignRange{0};
      }
      struct TagRangeLookalike { int value; };
      constexpr TagRangeLookalike kLookalike{0};
      constexpr TagRange<OtherPointerTag> kOtherRange(kOtherTag);
      template <auto... values> struct Probe {};
      class References {
       public:
        Probe<kTestCallbackTag, kOtherTag, other::kForeignTag,
              other::kForeignRange, kLookalike, kOtherRange> field;
      };
    """)
    fields = list(_find_class(h, "References").type.get_fields())
    self.assertEqual(
        layout_extract._pointer_tag_names(fields[0]), ["kTestCallbackTag"])

  def test_zero_size_padding_array_is_void_field(self):
    layouts = extract_layouts("""
      class Padded : public HeapObject {
       public:
        char padding_0_[0];
        uint32_t value_;
      };
    """)
    padding, value = layouts["Padded"].fields
    self.assertEqual(padding.cpp_name, "padding_0_")
    self.assertEqual(padding.size, 0)
    self.assertEqual(value.cpp_name, "value_")
    self.assertEqual(value.offset, 4)


class ConfigTest(unittest.TestCase):
  """extract_config reads each size from the alias V8 itself takes it
  from, so Config tracks the build configuration. Reading a type
  that merely resembles one is how trusted_pointer_size went wrong:
  IndirectPointerHandle is uint32_t in every configuration, while a
  trusted pointer slot is tagged when the sandbox is off."""

  _ALIASES = """
    using Tagged_t = unsigned int;
    using Address = unsigned long long;
    using ExternalPointer_t = unsigned long long;
    using CppHeapPointer_t = unsigned int;
    using TrustedPointer_t = unsigned short;
  """

  def test_reads_every_size_from_its_own_alias(self):
    h = parse(self._ALIASES)
    config = layout_extract.extract_config(h)
    self.assertEqual(
        (config.tagged_size, config.pointer_size, config.external_pointer_size,
         config.cpp_heap_pointer_size, config.trusted_pointer_size),
        (4, 8, 8, 4, 2))

  def test_a_missing_alias_is_fatal(self):
    with self.assertRaises(LayoutError) as ctx:
      layout_extract.extract_config(
          parse(self._ALIASES.replace("using TrustedPointer_t", "using X")))
    self.assertIn("TrustedPointer_t", str(ctx.exception))

  def test_a_class_member_does_not_shadow_the_namespace_alias(self):
    # v8-internal.h declares Internals::Tagged_t ahead of the namespace
    # alias; the alias index holds namespace-scope aliases only, so the
    # later one still wins.
    h = parse("""
      class Internals { public: using Tagged_t = unsigned long long; };
    """ + self._ALIASES)
    self.assertEqual(layout_extract.extract_config(h).tagged_size, 4)


class AnnotationTest(unittest.TestCase):

  def test_annotations_pass_through_uninterpreted(self):
    layouts = extract_layouts("""
      class Info : public HeapObject {
       public:
        V8_TQ_CONST V8_TQ_RELAXED TaggedMember<Smi> length_;
        V8_TQ_TYPE(SmiTagged<VariableProperties>) TaggedMember<Smi> props_;
        V8_TQ_NAME(other) uint32_t renamed_;
      };
    """)
    length, props, renamed = layouts["Info"].fields
    self.assertEqual([(a.name, a.arg) for a in length.annotations],
                     [("V8_TQ_CONST", None), ("V8_TQ_RELAXED", None)])
    self.assertEqual(props.storage.arg, named_type("Smi"))
    self.assertEqual([(a.name, a.arg) for a in props.annotations],
                     [("V8_TQ_TYPE", "SmiTagged<VariableProperties>")])
    self.assertEqual(renamed.cpp_name, "renamed_")
    self.assertEqual([(a.name, a.arg) for a in renamed.annotations],
                     [("V8_TQ_NAME", "other")])

  def test_annotation_argument_is_not_parsed(self):
    layouts = extract_layouts("""
      class Info : public HeapObject {
       public:
        V8_TQ_TYPE(iterator::IteratorRecord) TaggedMember<Smi> value_;
      };
    """)
    (value,) = layouts["Info"].fields
    self.assertEqual(value.storage.arg, named_type("Smi"))
    self.assertEqual([(a.name, a.arg) for a in value.annotations],
                     [("V8_TQ_TYPE", "iterator::IteratorRecord")])


class BaseChainTest(unittest.TestCase):

  _ARRAY_BASE = """
      class FakeArrayBase : public HeapObject {
       public:
        V8_TQ_CONST uint32_t length_;
      };
      template <typename Derived, typename ElementT, typename Super>
      class TaggedArrayBase : public Super {
       public:
        using ElementMemberT = TaggedMember<ElementT>;
      };
  """

  def test_flatten_through_template_base(self):
    layouts = extract_layouts(self._ARRAY_BASE + """
      class FakeArray
          : public TaggedArrayBase<FakeArray, Map, FakeArrayBase> {
        V8_TQ_TAIL_NAME(objects);
        V8_TQ_TAIL_LENGTH(length);
       public:
        TaggedMember<Map> objects_[0];
      };
    """)
    arr = layouts["FakeArray"]
    self.assertEqual(arr.base_cpp_name, "v8::internal::FakeArrayBase")
    self.assertEqual(arr.base_size, 8)
    self.assertEqual(arr.fields, ())
    self.assertIsNotNone(arr.tail)

  def test_member_in_template_base(self):
    layouts = extract_layouts(self._ARRAY_BASE + """
      template <typename Derived, typename Super>
      class CapacityBase : public Super {
       public:
        uint32_t capacity_;
      };
      class FakeTable : public CapacityBase<FakeTable, FakeArrayBase> {
       public:
        uint32_t elements_;
      };
    """)
    table = layouts["FakeTable"]
    self.assertEqual(table.base_cpp_name, "v8::internal::FakeArrayBase")
    self.assertEqual([(f.cpp_name, f.offset) for f in table.fields],
                     [("capacity_", 8), ("elements_", 12)])


class TailTest(unittest.TestCase):

  def test_tail(self):
    layouts = extract_layouts("""
      class Blob : public HeapObject {
        V8_TQ_TAIL_NAME(bytes);
        V8_TQ_TAIL_LENGTH(length);
       public:
        V8_TQ_CONST TaggedMember<Smi> length_;
        uint8_t flexible_array_member_data_[0];
      };
    """)
    blob = layouts["Blob"]
    self.assertEqual(blob.size, 8)
    self.assertEqual([(a.name, a.arg) for a in blob.annotations],
                     [("V8_TQ_TAIL_LENGTH", "length"),
                      ("V8_TQ_TAIL_NAME", "bytes")])
    tail = blob.tail
    self.assertEqual(tail.element_size, 1)
    self.assertEqual(tail.element_storage.kind, "int")
    self.assertEqual(tail.element_storage.width, 1)

  def test_tail_without_annotations_is_still_recorded(self):
    layouts = extract_layouts("""
      class Blob : public HeapObject {
       public:
        V8_TQ_CONST TaggedMember<Smi> length_;
        uint8_t flexible_array_member_data_[0];
      };
    """)
    blob = layouts["Blob"]
    self.assertIsNotNone(blob.tail)
    self.assertEqual(blob.annotations, ())
    self.assertEqual(blob.tail.element_size, 1)


class FailureModeTest(unittest.TestCase):

  def test_missing_indirect_pointer_handle_alias_is_fatal(self):
    h = parse(
        """
      class Holder : public HeapObject {
       public:
        std::atomic<RenamedIndirectPointerHandle> handle_;
      };
    """,
        prelude=_PRELUDE.replace("IndirectPointerHandle",
                                 "RenamedIndirectPointerHandle"))
    field = next(_find_class(h, "Holder").type.get_fields())
    with self.assertRaisesRegex(LayoutError,
                                "missing IndirectPointerHandle declaration"):
      layout_extract._Extractor(h, _V8_ROOT)._storage_type_of(field, field.type)

  def test_torque_generated_base_is_fatal(self):
    for base in ("TorqueGeneratedFoo<Foo, HeapObject>", "Intermediate<Foo>"):
      with self.subTest(base=base):
        with self.assertRaisesRegex(LayoutError,
                                    "unexpected Torque-generated base"):
          extract_layouts(f"""
            template <typename D, typename P>
            class TorqueGeneratedFoo : public P {{}};
            template <typename D>
            class Intermediate : public TorqueGeneratedFoo<D, HeapObject> {{}};
            class Foo : public {base} {{}};
          """)

  def test_unknown_class_template_argument_is_fatal(self):
    with self.assertRaisesRegex(LayoutError, "unsupported class template"):
      extract_layouts("""
        template <typename T>
        class Wrapper : public HeapObject {};
        class Holder : public HeapObject {
         public:
          TaggedMember<Wrapper<Map>> wrapped_;
        };
      """)

  def test_unknown_member_type_is_fatal(self):
    with self.assertRaises(LayoutError) as ctx:
      extract_layouts("""
        class Bad : public HeapObject {
         public:
          void* raw_pointer_;
        };
      """)
    self.assertIn("unsupported member type", str(ctx.exception))

  def test_unknown_member_wrapper_is_fatal(self):
    for header in ("src/objects/tagged-field.h", "src/objects/new-wrapper.h"):
      with self.subTest(header=header):
        with self.assertRaisesRegex(LayoutError, "unsupported member class"):
          extract_layouts(
              """
            class Bad : public HeapObject {
             public:
              MysteryMember mystery_;
            };
          """, {header: "class MysteryMember { unsigned int handle_; };"})

  def test_unknown_struct_is_fatal(self):
    for name in ("ScoreMember", "CoverageInfoSlot"):
      with self.subTest(name=name):
        with self.assertRaisesRegex(LayoutError, "unsupported member class"):
          extract_layouts(f"""
            namespace other {{ struct {name} {{ int value; }}; }}
            class Foo : public HeapObject {{
             public:
              other::{name} value_;
            }};
          """)

  def test_unknown_compression_scheme_is_fatal(self):
    # Falling through to a plain tagged slot would record the wrong
    # representation.
    with self.assertRaises(LayoutError) as ctx:
      extract_layouts("""
        class MysteryScheme;
        class Bad : public HeapObject {
         public:
          TaggedMember<Smi, MysteryScheme> slot_;
        };
      """)
    self.assertIn("unsupported compression scheme", str(ctx.exception))

  def test_unknown_compression_cage_is_fatal(self):
    with self.assertRaises(LayoutError) as ctx:
      extract_layouts("""
        class MysteryCage;
        class Bad : public HeapObject {
         public:
          TaggedMember<Smi, V8HeapCompressionSchemeImpl<MysteryCage>> slot_;
        };
      """)
    self.assertIn("unsupported compression cage", str(ctx.exception))

  def test_class_outside_hierarchy_is_fatal(self):
    with self.assertRaises(LayoutError) as ctx:
      h = parse("""
        class Orphan {
         public:
          uint32_t x_;
        };
      """)
      # Orphan does not inherit HeapObject and has no layout to export.
      layout_extract._Extractor(h, "/nonexistent-v8-root").extract_class_layout(
          "Orphan", _find_class(h, "Orphan"), {})
    self.assertIn("no ancestor with an exported layout", str(ctx.exception))

  def test_unknown_annotation_passes_through(self):
    # Export unknown annotations because Torque defines their meaning.
    layouts = extract_layouts("""
      class Bad : public HeapObject {
       public:
        [[clang::annotate("V8_TQ_BOGUS")]] uint32_t x_;
      };
    """)
    (x,) = layouts["Bad"].fields
    self.assertEqual([(a.name, a.arg) for a in x.annotations],
                     [("V8_TQ_BOGUS", None)])

  def test_malformed_annotation_payload_is_fatal(self):
    with self.assertRaises(LayoutError):
      extract_layouts("""
        class Bad : public HeapObject {
         public:
          [[clang::annotate("V8_TQ_TYPE(unclosed")]] uint32_t x_;
        };
      """)

  def test_repeated_annotation_is_fatal(self):
    with self.assertRaises(LayoutError):
      extract_layouts("""
        class Bad : public HeapObject {
         public:
          V8_TQ_NAME(a) V8_TQ_NAME(b) uint32_t x_;
        };
      """)


def _find_class(h, name):
  for cur in h.tu.cursor.walk_preorder():
    if cur.kind == cindex.CursorKind.CLASS_DECL and cur.spelling == name \
        and cur.is_definition():
      return cur
  raise AssertionError(f"class {name} not found")


if __name__ == "__main__":
  unittest.main()
