# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Object layout extraction from the parsed translation unit.

Consumes the ParsedTU produced by cpp_hier.scan_cpp and produces
layout_ir.ClassLayout objects for declarations that represent V8
heap-object types (see represents_heap_object_type).

A class's exported fields are all data members at offsets in
[sizeof(base), sizeof(class)), where base is the nearest ancestor with
an exported layout. This includes members of intermediate template
bases such as TaggedArrayBase. V8 heap objects use
single, non-virtual inheritance, so a member's offset within its
declaring class equals its offset within the derived class; the
ClassLayout contiguity validation checks that assumption.

Every problem is fatal: an unknown member type, a missing offset, a
malformed annotation, or more than one flexible tail raises LayoutError
and metagen exits without output.
"""

from __future__ import annotations

import re

import clang.cindex as cindex

from metagen import extract
from metagen.layout_ir import (ANNOTATION_PREFIX, Annotation, BitFieldEntry,
                               ClassLayout, Config, Field, LayoutError, CppType,
                               StorageType, Tail, named_type, union_type)

# Class templates allowed as wrapper arguments. The record names the
# template and its payload, so a consumer can map an instantiation to a
# type of its own (for Torque, kClassTemplateNames in
# src/torque/layout-loader.cc). Must be kept in sync with that list.
_CLASS_TEMPLATE_ARGUMENTS = frozenset({"CppGCManaged"})

# Annotation arguments are not parsed here; Torque parses them.
_TQ_PAYLOAD_RE = re.compile(r"^(V8_TQ_[A-Za-z0-9_]+)(?:\((.*)\))?$")

_SIGNED_KINDS = frozenset({
    cindex.TypeKind.CHAR_S,
    cindex.TypeKind.SCHAR,
    cindex.TypeKind.SHORT,
    cindex.TypeKind.INT,
    cindex.TypeKind.LONG,
    cindex.TypeKind.LONGLONG,
})
_UNSIGNED_KINDS = frozenset({
    cindex.TypeKind.CHAR_U,
    cindex.TypeKind.UCHAR,
    cindex.TypeKind.USHORT,
    cindex.TypeKind.UINT,
    cindex.TypeKind.ULONG,
    cindex.TypeKind.ULONGLONG,
})

# Preserve pointer-width aliases because their canonical types are ABI-specific.
_POINTER_WIDTH_ALIASES = frozenset(
    {"intptr_t", "uintptr_t", "size_t", "ptrdiff_t", "Address"})

# C++ classes/structs represented as struct fields by layout consumers.
# Reject others so an unknown storage wrapper cannot fall through to a struct.
_STRUCT_TYPES = frozenset({
    "v8::internal::CoverageInfoSlot",
    "v8::internal::DescriptorArray::Entry",
    "v8::internal::DoubleStringCache::Entry",
    "v8::internal::JSIteratorHelperSimple::IteratorRecord",
    "v8::internal::JSValidIteratorWrapper::IteratorRecord",
    "v8::internal::ScopeInfo::PositionInfo",
    "v8::internal::WasmCodePointer",
})

_POINTER_TAG_TYPES = frozenset({
    "v8::internal::ExternalPointerTag",
    "v8::internal::IndirectPointerTag",
})


def _parse_annotations(payloads, where: str) -> tuple[Annotation, ...]:
  """The V8_TQ_ annotations among `payloads`, name split from argument.

  Anything not of the form V8_TQ_<name> or V8_TQ_<name>(arg) is
  malformed and fails here. Torque defines what the names mean.
  """
  out = []
  for payload in payloads:
    if not payload.startswith(ANNOTATION_PREFIX):
      continue
    m = _TQ_PAYLOAD_RE.match(payload)
    if m is None:
      raise LayoutError(f"{where}: malformed annotation payload {payload!r}")
    out.append(Annotation(name=m.group(1), arg=m.group(2)))
  names = [a.name for a in out]
  dupes = sorted({n for n in names if names.count(n) > 1})
  if dupes:
    raise LayoutError(f"{where}: repeated annotation(s) {', '.join(dupes)}")
  return tuple(sorted(out, key=lambda a: (a.name, a.arg or "")))


def _alias_annotations(cursor: cindex.Cursor) -> list[str]:
  """Annotation payloads on the class's alias declarations only.

  extract.class_annotations collects attributes from every member,
  which for layout classes also includes the per-field V8_TQ_
  annotations on data members. Class-level annotations (V8_TQ_TAIL_*)
  expand to `using ... = void` aliases, so restricting to
  TYPE_ALIAS_DECL children separates the two.
  """
  out: list[str] = []
  for child in cursor.get_children():
    if child.kind != cindex.CursorKind.TYPE_ALIAS_DECL:
      continue
    for grandchild in child.get_children():
      if grandchild.kind == cindex.CursorKind.ANNOTATE_ATTR:
        out.append(grandchild.spelling)
  return out


def _where(cursor: cindex.Cursor) -> str:
  loc = cursor.location
  if loc.file is None:
    return "<unknown>"
  return f"{loc.file.name}:{loc.line}"


def mark_visited(parsed, cursor: cindex.Cursor | None) -> None:
  if cursor is not None:
    parsed.visited.add(cursor)


def _decl_of(t: cindex.Type):
  decl = t.get_declaration()
  return decl if decl.spelling else None


def _bit_field_group_base(decl: cindex.Cursor) -> cindex.Type | None:
  """The BitFieldGroup specialization `decl` derives from, if any."""
  for child in decl.get_children():
    if child.kind != cindex.CursorKind.CXX_BASE_SPECIFIER:
      continue
    base = child.type.get_canonical()
    base_decl = _decl_of(base)
    if base_decl is not None and base_decl.spelling == "BitFieldGroup":
      return base
  return None


def _cpp_bit_field_struct_name(decl: cindex.Cursor) -> str:
  """The C++ name of a BitFieldGroup, including enclosing classes."""
  parts = []
  while decl is not None and decl.kind in (cindex.CursorKind.CLASS_DECL,
                                           cindex.CursorKind.STRUCT_DECL,
                                           cindex.CursorKind.CLASS_TEMPLATE):
    parts.append(decl.spelling)
    decl = decl.semantic_parent
  return "::".join(reversed(parts))


def _template_args(t: cindex.Type) -> list[cindex.Type]:
  """Type template arguments of a specialization, preferring the
  as-written (sugared) type so aliases like MaybeObject survive, and
  falling back to the canonical type."""
  n = t.get_num_template_arguments()
  if n <= 0:
    t = t.get_canonical()
    n = t.get_num_template_arguments()
  if n <= 0:
    return []
  args = []
  for i in range(n):
    arg = t.get_template_argument_type(i)
    # Non-type arguments come back as invalid types; skip them (the
    # callers that need them read DECL_REF_EXPR children instead).
    if arg.kind != cindex.TypeKind.INVALID:
      args.append(arg)
  return args


def _pointer_tag_names(field: cindex.Cursor) -> list[str]:
  """Names of the pointer-tag constants referenced in the field's
  declared type (e.g. kFooIndirectPointerTag, or a kFoo...TagRange
  constant). The reference can sit below an implicit conversion, so
  walk the whole subtree; the filter keeps unrelated references (array
  extents, defaults) out."""
  out = []
  for child in field.walk_preorder():
    if child.kind != cindex.CursorKind.DECL_REF_EXPR:
      continue
    ref = child.referenced
    if ref is None:
      continue
    if ref.kind == cindex.CursorKind.ENUM_CONSTANT_DECL:
      parent = ref.semantic_parent
      if (parent is not None and
          extract.qualified_name(parent) in _POINTER_TAG_TYPES):
        out.append(ref.spelling)
    elif ref.kind == cindex.CursorKind.VAR_DECL:
      tag_type = ref.type.get_canonical()
      type_decl = tag_type.get_declaration()
      if (extract.qualified_name(type_decl) == "v8::internal::TagRange" and
          tag_type.get_num_template_arguments() == 1 and extract.qualified_name(
              tag_type.get_template_argument_type(0).get_declaration())
          in _POINTER_TAG_TYPES):
        out.append(ref.spelling)
  # A tag can be referenced more than once through conversions.
  return list(dict.fromkeys(out))


class _Extractor:

  def __init__(self, parsed, v8_root: str):
    self.parsed = parsed
    self.v8_root = v8_root

  def _position(self, cursor: cindex.Cursor) -> str:
    pos = extract.position(cursor, self.v8_root)
    if pos is None:
      raise LayoutError(f"{cursor.spelling}: declaration without a location")
    return pos

  def _pointer_width_alias(self, t: cindex.Type) -> str | None:
    """The pointer-width C++ alias an integral member is written
    through, if any; the outermost one wins, so a member written
    Address does not report as uintptr_t. The width alone cannot tell,
    since on a 32-bit target it equals that of a plain uint32_t."""
    while True:
      decl = _decl_of(t)
      if decl is None or decl.kind not in (cindex.CursorKind.TYPEDEF_DECL,
                                           cindex.CursorKind.TYPE_ALIAS_DECL):
        return None
      mark_visited(self.parsed, decl)
      if decl.spelling in _POINTER_WIDTH_ALIASES:
        return decl.spelling
      t = decl.underlying_typedef_type

  def _cpp_int_type_of(self, t: cindex.Type) -> CppType:
    """Preserve known pointer-width aliases; name other integers by size
    and signedness using <cstdint> names.

    For example, uintptr_t stays uintptr_t even on a 32-bit target.
    An unsigned long becomes uint32_t or uint64_t, depending on its
    size in the target ABI.
    """
    alias = self._pointer_width_alias(t)
    if alias is not None:
      return named_type(alias)
    canonical = t.get_canonical()
    is_signed = canonical.kind in _SIGNED_KINDS
    return named_type(f"{'' if is_signed else 'u'}int"
                      f"{canonical.get_size() * 8}_t")

  def _cpp_type_of(self, t: cindex.Type) -> CppType:
    """The structured C++ type of a wrapper's type argument. Aliases
    keep their name (MaybeObject stays MaybeObject); Union/UnionOf
    flatten, which is what they mean in C++ (union.h forbids a union of
    unions); Weak<T> keeps its template shape."""
    decl = _decl_of(t)
    if decl is None:
      raise LayoutError(f"cannot resolve type {t.spelling!r}")
    mark_visited(self.parsed, decl)
    name = decl.spelling

    if decl.kind in (cindex.CursorKind.TYPEDEF_DECL,
                     cindex.CursorKind.TYPE_ALIAS_DECL):
      # Keep a namespace-scope alias like MaybeObject rather than
      # expanding it to its canonical union. A class-scoped alias
      # (SharedFunctionInfo::NameOrScopeInfoT) is not visible outside
      # its class, so expand it.
      parent = decl.semantic_parent
      if parent is not None and parent.kind in (
          cindex.CursorKind.CLASS_DECL, cindex.CursorKind.STRUCT_DECL,
          cindex.CursorKind.CLASS_TEMPLATE):
        return self._cpp_type_of(t.get_canonical())
      return named_type(name)

    if name in ("Union", "UnionOf"):
      # An alias template like MaybeWeak<T> resolves to Union's decl
      # but has fewer arguments than the flattened union; the canonical
      # type has them all.
      args = _template_args(t)
      if len(args) < 2:
        args = _template_args(t.get_canonical())
      # Sugared members can themselves be unions (UnionOf<Smi,
      # MaybeWeak<Map>, ...>); flatten the way UnionOf itself does,
      # keeping first occurrences.
      members = []
      for a in args:
        member = self._cpp_type_of(a)
        splice = member.members if member.kind == "union" else (member,)
        members.extend(m for m in splice if m not in members)
      if len(members) < 2:
        raise LayoutError(f"union type {t.spelling!r} with < 2 members")
      return union_type(*members)

    if name == "Weak":
      args = _template_args(t)
      if len(args) != 1:
        raise LayoutError(f"Weak type {t.spelling!r} without one argument")
      return named_type("Weak", self._cpp_type_of(args[0]))

    if decl.kind in (cindex.CursorKind.CLASS_DECL,
                     cindex.CursorKind.STRUCT_DECL,
                     cindex.CursorKind.CLASS_TEMPLATE):
      if t.get_canonical().get_num_template_arguments() > 0:
        if name not in _CLASS_TEMPLATE_ARGUMENTS:
          raise LayoutError(f"unsupported class template argument "
                            f"{t.spelling!r}; add {name} to "
                            f"_CLASS_TEMPLATE_ARGUMENTS and to each consumer")
        return named_type(name, self._payload_type(t))
      return named_type(name)

    if decl.kind == cindex.CursorKind.ENUM_DECL:
      return named_type(name)

    raise LayoutError(f"unsupported type argument {t.spelling!r} ({decl.kind})")

  def _payload_type(self, t: cindex.Type) -> CppType:
    """The sole argument of an allowed class template, named but not
    visited: a payload is a plain C++ type, not a heap object, so it has
    no layout of its own to harvest."""
    args = _template_args(t.get_canonical())
    if len(args) != 1:
      raise LayoutError(f"class template {t.spelling!r} without one argument")
    parts = args[0].get_canonical().spelling.split("::")
    return named_type(parts[-1], namespaces=tuple(parts[:-1]))

  def _bitfield_storage(self, field: cindex.Cursor, name: str,
                        group: cindex.Cursor, base: cindex.Type) -> StorageType:
    """The bit layout of a BitFieldGroup member: every type alias of
    the group is a base::BitField, read in declaration order (which is
    bit order) for its name, field type, shift and width."""
    bits = []
    for alias in group.get_children():
      if alias.kind != cindex.CursorKind.TYPE_ALIAS_DECL:
        continue
      bit = alias.underlying_typedef_type.get_canonical()
      decl = _decl_of(bit)
      if decl is None or decl.spelling != "BitField":
        raise LayoutError(f"{_where(field)}: {group.spelling}::"
                          f"{alias.spelling} is not a base::BitField "
                          f"({bit.spelling}); a BitFieldGroup may only "
                          f"contain base::BitField aliases")
      bits.append(
          BitFieldEntry(
              cpp_name=alias.spelling,
              type=self._bit_field_type(decl.get_template_argument_type(0)),
              offset=decl.get_template_argument_value(1),
              num_bits=decl.get_template_argument_value(2)))
    if not bits:
      raise LayoutError(f"{_where(field)}: bit field group "
                        f"{group.spelling!r} declares no bit fields")
    return StorageType(
        kind="bitfield",
        name=name,
        width=base.get_template_argument_type(1).get_size(),
        bits=tuple(bits))

  def _bit_field_type(self, t: cindex.Type) -> str:
    """The C++ name of one bit field's value type."""
    canonical = t.get_canonical()
    if canonical.kind == cindex.TypeKind.BOOL:
      return "bool"
    if canonical.kind == cindex.TypeKind.ENUM:
      decl = canonical.get_declaration()
      mark_visited(self.parsed, decl)
      return decl.spelling
    return self._cpp_int_type_of(t).name

  def _is_trusted_space_scheme(self, field: cindex.Cursor,
                               scheme: cindex.Type) -> bool:
    """Whether a TaggedMember's compression scheme is the trusted-space
    one, which makes the slot a protected pointer.

    With the sandbox the scheme is an alias for
    V8HeapCompressionSchemeImpl<TrustedCage>; without it, it is a
    distinct struct of its own (globals.h keeps it distinct so that
    is_same_v dispatch behaves the same either way). An unrecognized
    scheme is fatal: falling through to a plain tagged slot would
    record the wrong representation.
    """
    canonical = scheme.get_canonical()
    decl = _decl_of(canonical)
    if decl is None:
      raise LayoutError(f"{_where(field)}: cannot resolve compression "
                        f"scheme {scheme.spelling!r}")
    mark_visited(self.parsed, decl)
    if decl.spelling == "TrustedSpaceCompressionScheme":
      return True
    if decl.spelling == "V8HeapCompressionSchemeImpl":
      cages = _template_args(canonical)
      cage = _decl_of(cages[0]) if cages else None
      if cage is None:
        raise LayoutError(f"{_where(field)}: V8HeapCompressionSchemeImpl "
                          f"without a cage argument")
      mark_visited(self.parsed, cage)
      if cage.spelling in ("TrustedCage", "MainCage"):
        return cage.spelling == "TrustedCage"
      raise LayoutError(f"{_where(field)}: unsupported compression cage "
                        f"{cage.spelling!r}")
    raise LayoutError(f"{_where(field)}: unsupported compression scheme "
                      f"{decl.spelling!r}")

  def _member_tags(self, field: cindex.Cursor,
                   elem_type: cindex.Type) -> list[str]:
    """Pointer-tag constants for a tagged member wrapper. Normally
    referenced from the field's own declared type; when the member is
    declared through an alias (CodePointerMember), the reference lives
    on the alias declaration instead."""
    tags = _pointer_tag_names(field)
    if not tags:
      alias = _decl_of(elem_type)
      if alias is not None and alias.kind in (
          cindex.CursorKind.TYPEDEF_DECL, cindex.CursorKind.TYPE_ALIAS_DECL):
        mark_visited(self.parsed, alias)
        tags = _pointer_tag_names(alias)
    return tags

  def _unaligned_arg_type(self, field: cindex.Cursor,
                          arg: cindex.Type) -> CppType:
    """The C++ type of an UnalignedValueMember's argument: a float, an
    integer, or a named struct."""
    canonical = arg.get_canonical()
    if canonical.kind == cindex.TypeKind.DOUBLE:
      return named_type("double")
    if canonical.kind == cindex.TypeKind.FLOAT:
      return named_type("float")
    if canonical.kind in _SIGNED_KINDS or canonical.kind in _UNSIGNED_KINDS:
      return self._cpp_int_type_of(arg)
    decl = _decl_of(canonical)
    if decl is not None and decl.kind in (cindex.CursorKind.CLASS_DECL,
                                          cindex.CursorKind.STRUCT_DECL):
      mark_visited(self.parsed, decl)
      return named_type(decl.spelling)
    raise LayoutError(f"{_where(field)}: unsupported "
                      f"UnalignedValueMember<{arg.spelling}>")

  def _storage_type_of(self, field: cindex.Cursor,
                       elem_type: cindex.Type) -> StorageType:
    """Storage for one member (with any constant-array extent
    already peeled off by the caller)."""
    canonical = elem_type.get_canonical()
    decl = _decl_of(canonical)

    # A bare IndirectPointerHandle member (always atomic-wrapped in
    # practice) is an object's own pointer-table entry -- physically
    # the same slot a TrustedPointerMember holds, minus pointee and
    # tag.
    handle_alias = self.parsed.aliases.get("IndirectPointerHandle")
    if handle_alias is None:
      raise LayoutError(
          f"{_where(field)}: missing IndirectPointerHandle declaration; "
          "update trusted-pointer storage classification")
    written = _decl_of(elem_type)
    if written == handle_alias:
      mark_visited(self.parsed, written)
      return StorageType(kind="trusted_pointer")

    if decl is not None:
      base = _bit_field_group_base(decl)
      if base is not None:
        mark_visited(self.parsed, decl)
        return self._bitfield_storage(field, _cpp_bit_field_struct_name(decl),
                                      decl, base)

    if decl is not None and decl.kind in (cindex.CursorKind.CLASS_DECL,
                                          cindex.CursorKind.STRUCT_DECL):
      mark_visited(self.parsed, decl)
      name = decl.spelling
      if name == "atomic":
        # std::atomic<T> has T's size and alignment; the atomicity is
        # accessor semantics, not layout. Prefer the sugared argument
        # so aliases like IndirectPointerHandle stay recognizable.
        args = _template_args(elem_type) or _template_args(canonical)
        if len(args) != 1:
          raise LayoutError(f"{_where(field)}: atomic member without one "
                            f"type argument ({elem_type.spelling})")
        return self._storage_type_of(field, args[0])
      if name == "TaggedMember":
        # The written type usually has one argument and defaults the
        # compression scheme; the canonical type has both. Prefer the
        # sugared first argument so aliases survive.
        sugared = _template_args(elem_type)
        args = sugared if len(sugared) == 2 else _template_args(canonical)
        if len(args) != 2:
          raise LayoutError(f"{_where(field)}: TaggedMember with "
                            f"{len(args)} type arguments")
        value_arg = sugared[0] if sugared else args[0]
        arg_type = self._cpp_type_of(value_arg)
        if self._is_trusted_space_scheme(field, args[1]):
          return StorageType(kind="protected_tagged", arg=arg_type)
        return StorageType(kind="tagged", arg=arg_type)
      if name == "TrustedPointerMember":
        args = _template_args(elem_type) or _template_args(canonical)
        if not args:
          raise LayoutError(
              f"{_where(field)}: TrustedPointerMember without arguments")
        pointee = self._cpp_type_of(args[0])
        tags = self._member_tags(field, elem_type)
        if len(tags) != 1:
          raise LayoutError(
              f"{_where(field)}: expected exactly one tag reference on "
              f"TrustedPointerMember, found {tags}")
        return StorageType(kind="trusted_pointer", arg=pointee, tag=tags[0])
      if name == "ExternalPointerMember":
        tags = self._member_tags(field, elem_type)
        if len(tags) != 1:
          raise LayoutError(
              f"{_where(field)}: expected exactly one tag reference on "
              f"{name}, found {tags}")
        return StorageType(kind="external_pointer", tag=tags[0])
      if name == "CppHeapPointerMember":
        # Not tagged at the member: cpp_heap_wrappable slots share
        # embedder tag ranges, so the tag is a call-site argument.
        return StorageType(kind="cpp_heap_pointer")
      if name == "JSDispatchHandleMember":
        return StorageType(kind="js_dispatch_handle")
      if name == "UnalignedDoubleMember":
        return StorageType(kind="unaligned", arg=named_type("double"))
      if name == "UnalignedValueMember":
        args = _template_args(elem_type) or _template_args(canonical)
        if not args:
          raise LayoutError(
              f"{_where(field)}: UnalignedValueMember without arguments")
        return StorageType(
            kind="unaligned", arg=self._unaligned_arg_type(field, args[0]))
      if extract.qualified_name(decl) not in _STRUCT_TYPES:
        raise LayoutError(f"{_where(field)}: unsupported member class "
                          f"{extract.qualified_name(decl)!r} "
                          f"({elem_type.spelling})")
      # A plain nested struct (CoverageInfoSlot, DoubleStringCache's
      # Entry). The size is what matters for the layout; `name` is the
      # C++ struct name, which a consumer maps to its own struct type.
      return StorageType(kind="struct", name=name, width=canonical.get_size())

    if canonical.kind == cindex.TypeKind.ENUM:
      enum_decl = canonical.get_declaration()
      mark_visited(self.parsed, enum_decl)
      underlying = enum_decl.enum_type.get_canonical()
      width = underlying.get_size()
      return StorageType(
          kind="enum",
          name=enum_decl.spelling,
          width=width,
          is_signed=underlying.kind in _SIGNED_KINDS)

    if canonical.kind == cindex.TypeKind.BOOL:
      return StorageType(kind="bool")

    if canonical.kind in _SIGNED_KINDS or canonical.kind in _UNSIGNED_KINDS:
      width = canonical.get_size()
      is_signed = canonical.kind in _SIGNED_KINDS
      return StorageType(
          kind="int",
          width=width,
          is_signed=is_signed,
          pointer_width_alias=self._pointer_width_alias(elem_type))

    raise LayoutError(
        f"{_where(field)}: unsupported member type {elem_type.spelling!r}")

  def _member_annotations(self, cursor: cindex.Cursor) -> tuple:
    """The V8_TQ_ annotations written on a data member."""
    return _parse_annotations(
        (child.spelling
         for child in cursor.get_children()
         if child.kind == cindex.CursorKind.ANNOTATE_ATTR), _where(cursor))

  def _make_field(self, field: cindex.Cursor, offset: int) -> Field:
    t = field.type
    extent = None
    if t.kind == cindex.TypeKind.CONSTANTARRAY:
      extent = t.get_array_size()
      if extent == 0:
        raise LayoutError(
            f"{_where(field)}: {field.spelling}: zero-length array member "
            f"outside a flexible tail")
      elem_type = t.get_array_element_type()
    else:
      elem_type = t
    size = t.get_size()
    if size <= 0:
      raise LayoutError(f"{_where(field)}: {field.spelling}: no size")
    return Field(
        cpp_name=field.spelling,
        cpp_type=elem_type.spelling,
        position=self._position(field),
        offset=offset,
        size=size,
        storage=self._storage_type_of(field, elem_type),
        array_extent=extent,
        annotations=self._member_annotations(field))

  def _make_zero_field(self, field: cindex.Cursor, offset: int) -> Field:
    """A zero-length array member below sizeof, recorded with size 0."""
    return Field(
        cpp_name=field.spelling,
        cpp_type=field.type.get_array_element_type().spelling,
        position=self._position(field),
        offset=offset,
        size=0,
        storage=self._storage_type_of(field,
                                      field.type.get_array_element_type()),
        annotations=self._member_annotations(field))

  def _make_tail(self, field: cindex.Cursor, offset: int) -> Tail:
    elem_type = field.type.get_array_element_type()
    elem_size = elem_type.get_size()
    if elem_size <= 0:
      raise LayoutError(f"{_where(field)}: flexible tail element has no size")
    return Tail(
        cpp_name=field.spelling,
        cpp_element_type=elem_type.spelling,
        position=self._position(field),
        offset=offset,
        element_size=elem_size,
        element_storage=self._storage_type_of(field, elem_type),
        annotations=self._member_annotations(field))

  def _collect_members(self, class_type: cindex.Type, stop_base: str | None,
                       out: list) -> None:
    """Collect the fields of `class_type` and of every base between it
    and the base named `stop_base` (exclusive).

    For `template <typename T> class Base { T value_; }`, the type of
    `Base<int>` exposes `value_` as an int field through get_fields().
    Its declaration cursor's get_children() is empty, so walk the
    instantiated types with get_bases() and get_fields().

    `out` receives (offset_bytes, cursor) pairs; a flexible-array
    member is included with its zero-length array type.
    """
    for base in class_type.get_bases():
      canonical = base.type.get_canonical()
      base_decl = canonical.get_declaration()
      mark_visited(self.parsed, base_decl)
      base_name = base_decl.spelling or base.type.spelling
      if base_name.startswith("TorqueGenerated"):
        raise LayoutError(
            f"{_where(base_decl)}: unexpected Torque-generated base "
            f"{base.type.spelling!r}; heap object layouts must be defined in C++"
        )
      if stop_base is not None and base_name == stop_base:
        continue
      if base_decl.kind not in (cindex.CursorKind.CLASS_DECL,
                                cindex.CursorKind.STRUCT_DECL):
        raise LayoutError(
            f"{_where(base_decl)}: cannot resolve base {base.type.spelling!r}")
      # Single, non-virtual inheritance: the base subobject is at
      # offset 0, so member offsets within the base are valid within
      # the derived class too. ClassLayout's contiguity validation
      # checks this.
      self._collect_members(canonical, stop_base, out)
    for child in class_type.get_fields():
      offset_bits = child.get_field_offsetof()
      if offset_bits < 0:
        raise LayoutError(
            f"{_where(child)}: {child.spelling}: libclang reports no "
            f"offset (error {offset_bits})")
      # libclang reports offsets in bits; the layout stores 8-bit byte offsets.
      if offset_bits % 8 != 0:
        raise LayoutError(f"{_where(child)}: {child.spelling}: not "
                          f"byte-aligned")
      out.append((offset_bits // 8, child))

  def _class_annotations(self, cursor: cindex.Cursor) -> tuple:
    """Class annotations describing how Torque represents the tail."""
    return _parse_annotations(_alias_annotations(cursor), _where(cursor))

  def extract_class_layout(self, name: str, cursor: cindex.Cursor,
                           layout_cursors: dict) -> ClassLayout:
    mark_visited(self.parsed, cursor)
    size = cursor.type.get_size()
    if size <= 0:
      raise LayoutError(f"{_where(cursor)}: {name}: sizeof failed ({size})")
    align = cursor.type.get_align()
    if align <= 0:
      raise LayoutError(f"{_where(cursor)}: {name}: alignof failed ({align})")

    if name == extract.HEAP_OBJECT_ROOT:
      base_name = None
      base_size = 0
    else:
      base_name = extract.resolve_logical_base(
          cursor,
          set(layout_cursors.keys()),
          templates=self.parsed.templates_idx,
          visited=self.parsed.visited)
      if base_name is None or base_name not in layout_cursors:
        raise LayoutError(
            f"{_where(cursor)}: {name}: no ancestor with an exported layout")
      base_cursor = layout_cursors[base_name]
      base_size = base_cursor.type.get_size()
      if base_size <= 0:
        raise LayoutError(f"{name}: sizeof({base_name}) failed ({base_size})")

    members: list = []
    self._collect_members(cursor.type, base_name, members)
    # Sort by offset.
    members.sort(key=lambda pair: pair[0])

    ann = self._class_annotations(cursor)
    fields = []
    tail = None
    for offset, member in members:
      is_zero_array = (
          member.type.kind == cindex.TypeKind.CONSTANTARRAY and
          member.type.get_array_size() == 0)
      # A zero-length array at sizeof(class) is the flexible tail
      # (FLEXIBLE_ARRAY_MEMBER); anywhere else it is the zero-size
      # padding idiom (`char padding_[0]` under config guards).
      if is_zero_array and offset != size:
        fields.append(self._make_zero_field(member, offset))
        continue
      is_flexible_array_member = is_zero_array
      if is_flexible_array_member:
        if tail is not None:
          raise LayoutError(
              f"{_where(member)}: {name}: more than one flexible tail")
        tail = self._make_tail(member, offset)
      else:
        fields.append(self._make_field(member, offset))

    qualified = f"v8::internal::{name}"
    qualified_base = (f"v8::internal::{base_name}"
                      if base_name is not None else None)
    return ClassLayout(
        cpp_name=qualified,
        position=self._position(cursor),
        base_cpp_name=qualified_base,
        base_size=base_size,
        size=size,
        alignment=align,
        fields=tuple(fields),
        tail=tail,
        annotations=ann)


# Read configuration sizes from the aliases that define them in the parsed
# translation unit. Flags and similarly sized types are not reliable proxies.
_CONFIG_ALIASES = {
    "Tagged_t": "tagged_size",
    "Address": "pointer_size",
    "ExternalPointer_t": "external_pointer_size",
    "CppHeapPointer_t": "cpp_heap_pointer_size",
    "TrustedPointer_t": "trusted_pointer_size",
}


def extract_config(parsed) -> Config:
  """The build configuration of the parsed translation unit."""
  sizes: dict[str, int] = {}
  for name, field in _CONFIG_ALIASES.items():
    alias = parsed.aliases.get(name)
    if alias is None:
      continue
    size = alias.underlying_typedef_type.get_size()
    if size <= 0:
      raise LayoutError(f"{_where(alias)}: sizeof({name}) failed ({size})")
    mark_visited(parsed, alias)
    sizes[field] = size
  missing = sorted(set(_CONFIG_ALIASES.values()) - set(sizes))
  if missing:
    raise LayoutError("the translation unit declares no " + ", ".join(
        sorted(name for name, f in _CONFIG_ALIASES.items() if f in missing)) +
                      "; cannot record the build "
                      "configuration the layouts were extracted under")
  return Config(**sizes)


def represents_heap_object_type(cursor: cindex.Cursor) -> bool:
  """Whether the declaration represents a V8 heap-object type.

  Class templates are inheritance machinery rather than object types of
  their own.
  """
  return cursor.kind in (cindex.CursorKind.CLASS_DECL,
                         cindex.CursorKind.STRUCT_DECL)


def extract_layouts(parsed, v8_root: str) -> list[ClassLayout]:
  """Build layouts for every V8 heap-object type in the parsed TU."""
  layout_cursors = {
      name: cur
      for name, cur in parsed.heap_classes.items()
      if represents_heap_object_type(cur)
  }
  extractor = _Extractor(parsed, v8_root)
  layouts = []
  for name in sorted(layout_cursors):
    layouts.append(
        extractor.extract_class_layout(name, layout_cursors[name],
                                       layout_cursors))
  return layouts
