# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Typed IR and JSON schema for object layouts defined in C++.

layout_extract.py produces these layouts from the parsed translation
unit and serializes them to layouts.json. They describe C++ storage,
member types, and the build configuration. Torque maps storage to field
types in layout-loader.cc; V8_TQ_* annotations pass through uninterpreted.

The dataclasses validate on construction and raise LayoutError on failure.
Serialization is deterministic; parse_document validates the emitted
schema independently of Torque's loader. Source positions are serialized
separately so moving declarations does not change layouts.json.

SCHEMA_VERSION is bumped on any change to the emitted shape; a consumer
rejects a version it does not know.
"""

from __future__ import annotations

import dataclasses
import json

SCHEMA_VERSION = 1

# Keep this set closed so unknown storage wrappers fail during extraction.
STORAGE_KINDS = frozenset({
    # `arg` holds the structured TaggedMember type argument.
    "tagged",
    "protected_tagged",
    # TrustedPointerMember<T, tag>: an indirect pointer handle; `tag`
    # names the IndirectPointerTag constant.
    "trusted_pointer",
    "external_pointer",
    "cpp_heap_pointer",
    # A BitFieldGroup member: `name` is the C++ struct name and `bits`
    # contains the fields in bit order.
    "bitfield",
    # Integral storage: `width` bytes, `signed` flag, and
    # `pointer_width_alias` when the member was written through one
    # (uintptr_t, size_t, ...), which its width alone does not say.
    "int",
    # An enum with integral storage; `name` is the unqualified C++ enum
    # name, `width`/`signed` describe the underlying type.
    "enum",
    "bool",
    "unaligned",
    # A plain nested struct member; `name` is the C++ struct type
    # name, `width` its size.
    "struct",
    "js_dispatch_handle",
})

ANNOTATION_PREFIX = "V8_TQ_"


class LayoutError(Exception):
  """Invalid layout data that must not be emitted. Always fatal."""


def _check(cond: bool, what: str) -> None:
  if not cond:
    raise LayoutError(what)


def _check_identifier(s: str, what: str) -> None:
  _check(
      isinstance(s, str) and s.replace("_", "a").isalnum() and s != "",
      f"{what}: not an identifier: {s!r}")


@dataclasses.dataclass(frozen=True)
class Config:
  """The build configuration the layouts were extracted under.

  Offsets and sizes are only valid for a matching configuration, and
  the JSON is generated per build directory, so a consumer must be
  able to check that it got the right one: a 64-bit document read by a
  32-bit consumer would otherwise describe wrong layouts without any
  error. Every size is read from the type that defines it in the parsed
  translation unit, not from a flag.
  """

  tagged_size: int  # sizeof(Tagged_t); < pointer_size iff compressed
  pointer_size: int  # sizeof(Address)
  external_pointer_size: int  # sizeof(ExternalPointer_t)
  cpp_heap_pointer_size: int  # sizeof(CppHeapPointer_t)
  trusted_pointer_size: int  # sizeof(TrustedPointer_t)

  def __post_init__(self) -> None:
    for name in ("tagged_size", "pointer_size", "external_pointer_size",
                 "cpp_heap_pointer_size", "trusted_pointer_size"):
      value = getattr(self, name)
      _check(
          isinstance(value, int) and not isinstance(value, bool) and
          value in (1, 2, 4, 8), f"config: {name} is not a valid size: "
          f"{value!r}")
    _check(self.tagged_size <= self.pointer_size,
           "config: tagged_size exceeds pointer_size")

  def to_json(self):
    return {
        "tagged_size": self.tagged_size,
        "pointer_size": self.pointer_size,
        "external_pointer_size": self.external_pointer_size,
        "cpp_heap_pointer_size": self.cpp_heap_pointer_size,
        "trusted_pointer_size": self.trusted_pointer_size,
    }

  @staticmethod
  def from_json(d) -> "Config":
    _check(isinstance(d, dict), f"config: expected object, got {d!r}")
    keys = {
        "tagged_size", "pointer_size", "external_pointer_size",
        "cpp_heap_pointer_size", "trusted_pointer_size"
    }
    _check_keys(d, keys, set(), "config")
    return Config(**{k: d[k] for k in keys})


@dataclasses.dataclass(frozen=True)
class Annotation:
  """One V8_TQ_* annotation as written on a C++ declaration.

  Metagen does not interpret annotations: it splits the name from the
  parenthesized argument and passes both through. What an annotation means,
  which declarations it may appear on and whether it takes an argument
  is defined by the consumer (for Torque, kAnnotations in layout-loader.cc),
  so an annotation can be added or changed without touching metagen or this
  schema.
  """

  name: str
  arg: str | None = None

  def __post_init__(self) -> None:
    _check(
        isinstance(self.name, str) and self.name.startswith(ANNOTATION_PREFIX),
        f"annotation name without the {ANNOTATION_PREFIX} prefix: "
        f"{self.name!r}")
    _check_identifier(self.name, "annotation name")
    if self.arg is not None:
      _check(
          isinstance(self.arg, str) and self.arg != "",
          f"{self.name}: empty argument")

  def to_json(self):
    d = {"name": self.name}
    if self.arg is not None:
      d["arg"] = self.arg
    return d

  @staticmethod
  def from_json(d) -> "Annotation":
    _check(isinstance(d, dict), f"annotation: expected object, got {d!r}")
    _check_keys(d, {"name"}, {"arg"}, "annotation")
    return Annotation(name=d["name"], arg=d.get("arg"))


def annotations_json(annotations: tuple["Annotation", ...]) -> list:
  return [a.to_json() for a in annotations]


def canonical_annotations(annotations, what: str) -> tuple["Annotation", ...]:
  """Sorted by name so the document is stable. A repeated annotation is
  rejected here regardless of its meaning."""
  out = tuple(sorted(annotations, key=lambda a: (a.name, a.arg or "")))
  names = [a.name for a in out]
  dupes = sorted({n for n in names if names.count(n) > 1})
  _check(not dupes, f"{what}: repeated annotation(s) {', '.join(dupes)}")
  return out


def annotations_from_json(d, what: str) -> tuple["Annotation", ...]:
  raw = d.get("annotations", ())
  _check(isinstance(raw, (list, tuple)), f"{what}: annotations is not a list")
  return canonical_annotations((Annotation.from_json(a) for a in raw), what)


@dataclasses.dataclass(frozen=True)
class CppType:
  """A C++ type argument: a named type with optional template
  arguments, or a flattened union of these.

  Names are always C++ names: `double`, `uintptr_t`, `Map`,
  `MaybeObject`. A consumer with its own names for primitives maps them
  itself.

  Structured rather than a string so a consumer builds its type
  representation directly instead of parsing C++ type syntax. The
  verbatim string is available too, on Field.cpp_type."""

  kind: str  # "name" | "union"
  name: str | None = None  # for "name"
  args: tuple[CppType, ...] = ()  # for "name": template arguments
  members: tuple[CppType, ...] = ()  # for "union": UnionOf<...> flattened
  # for "name": namespace qualification (iterator::IteratorRecord)
  namespaces: tuple[str, ...] = ()

  def __post_init__(self) -> None:
    if self.kind == "name":
      _check_identifier(self.name or "", "cpp type name")
      for ns in self.namespaces:
        _check_identifier(ns, "cpp type namespace")
      _check(not self.members, "cpp name type with union members")
    elif self.kind == "union":
      _check(self.name is None and not self.args, "cpp union with name/args")
      _check(len(self.members) >= 2, "cpp union with fewer than 2 members")
      _check(
          all(m.kind == "name" for m in self.members),
          "cpp union members must be named types")
    else:
      raise LayoutError(f"unknown cpp type kind: {self.kind!r}")

  def to_json(self):
    if self.kind == "name":
      d = {"kind": "name", "name": self.name}
      if self.namespaces:
        d["namespaces"] = list(self.namespaces)
      if self.args:
        d["args"] = [a.to_json() for a in self.args]
      return d
    return {"kind": "union", "members": [m.to_json() for m in self.members]}

  @staticmethod
  def from_json(d) -> "CppType":
    _check(isinstance(d, dict), f"cpp type: expected object, got {d!r}")
    kind = d.get("kind")
    if kind == "name":
      _check_keys(d, {"kind", "name"}, {"args", "namespaces"}, "cpp type")
      return CppType(
          kind="name",
          name=d["name"],
          args=tuple(CppType.from_json(a) for a in d.get("args", ())),
          namespaces=tuple(d.get("namespaces", ())))
    if kind == "union":
      _check_keys(d, {"kind", "members"}, set(), "cpp type")
      return CppType(
          kind="union",
          members=tuple(CppType.from_json(m) for m in d["members"]))
    raise LayoutError(f"unknown cpp type kind: {kind!r}")


def named_type(name: str, *args: CppType,
               namespaces: tuple[str, ...] = ()) -> CppType:
  return CppType(
      kind="name", name=name, args=tuple(args), namespaces=namespaces)


def union_type(*members: CppType) -> CppType:
  return CppType(kind="union", members=tuple(members))


@dataclasses.dataclass(frozen=True)
class BitFieldEntry:
  """One field of a "bitfield" storage: a base::BitField alias.

  `cpp_name` is the alias name; the consumer derives its own field name
  from it (Torque: IsCallableBit -> is_callable)."""

  cpp_name: str
  type: str
  offset: int
  num_bits: int

  def __post_init__(self) -> None:
    _check_identifier(self.cpp_name, "bit field name")
    _check_identifier(self.type, "bit field type")
    _check(self.num_bits > 0, f"bit field {self.cpp_name}: no bits")
    _check(self.offset >= 0, f"bit field {self.cpp_name}: negative offset")

  def to_json(self):
    return {
        "cpp_name": self.cpp_name,
        "type": self.type,
        "offset": self.offset,
        "num_bits": self.num_bits,
    }

  @staticmethod
  def from_json(d) -> "BitFieldEntry":
    _check(isinstance(d, dict), f"bit field: expected object, got {d!r}")
    _check_keys(d, {"cpp_name", "type", "offset", "num_bits"}, set(),
                "bit field")
    return BitFieldEntry(
        cpp_name=d["cpp_name"],
        type=d["type"],
        offset=d["offset"],
        num_bits=d["num_bits"])


@dataclasses.dataclass(frozen=True)
class StorageType:
  """A field's storage, one of STORAGE_KINDS."""

  kind: str
  # "tagged" / "protected_tagged" / "unaligned": the structured type
  # argument. "trusted_pointer": the pointee as CppType.
  arg: CppType | None = None
  # "trusted_pointer" / "external_pointer": the C++ tag constant name.
  tag: str | None = None
  # "int" / "enum": underlying width in bytes and signedness.
  width: int | None = None
  is_signed: bool | None = None
  # Preserve pointer-width aliases; their canonical width is ABI-specific.
  pointer_width_alias: str | None = None
  # C++ type name without namespaces; includes enclosing classes for bitfields.
  name: str | None = None
  # "bitfield": the group's fields, in declaration order, which is bit
  # order.
  bits: tuple[BitFieldEntry, ...] = ()

  def __post_init__(self) -> None:
    _check(self.kind in STORAGE_KINDS, f"unknown storage kind: {self.kind!r}")
    if self.kind in ("tagged", "protected_tagged", "unaligned"):
      _check(self.arg is not None, f"{self.kind} storage without type arg")
    if self.kind == "trusted_pointer":
      # Both absent for an object's own pointer-table entry (the
      # atomic IndirectPointerHandle member).
      _check((self.arg is None) == (self.tag is None),
             "trusted_pointer storage needs pointee and tag together")
    if self.kind == "external_pointer":
      _check(self.tag is not None, "external_pointer storage needs a tag")
    if self.kind in ("int", "enum"):
      _check(self.width in (1, 2, 4, 8), f"{self.kind} storage width invalid")
      _check(self.is_signed is not None, f"{self.kind} storage needs signness")
    if self.kind in ("enum", "struct"):
      _check_identifier(self.name or "", f"{self.kind} storage name")
    if self.kind == "struct":
      _check(self.width is not None and self.width > 0,
             "struct storage needs a size")
    if self.kind == "bitfield":
      _check(
          isinstance(self.name, str), "bitfield storage name must be a string")
      for part in (self.name or "").split("::"):
        _check_identifier(part, "bitfield storage name")
      _check(self.width in (1, 2, 4, 8), "bitfield storage width invalid")
      _check(bool(self.bits), "bitfield storage without fields")
      # A base::BitField chain starts at bit 0 and has no holes; a gap
      # means an alias is missing from the group.
      next_bit = 0
      for bit in self.bits:
        _check(
            bit.offset == next_bit,
            f"bitfield {self.name}: {bit.cpp_name} starts at {bit.offset}, "
            f"expected {next_bit}")
        next_bit += bit.num_bits
      _check(next_bit <= self.width * 8,
             f"bitfield {self.name}: {next_bit} bits exceed the storage")
    else:
      _check(not self.bits, f"{self.kind} storage with bit fields")
    if self.pointer_width_alias is not None:
      _check(self.kind == "int", "pointer_width_alias on non-int storage")
      _check_identifier(self.pointer_width_alias, "pointer_width_alias")

  def to_json(self):
    d = {"kind": self.kind}
    if self.name is not None:
      d["name"] = self.name
    if self.arg is not None:
      d["arg"] = self.arg.to_json()
    if self.tag is not None:
      d["tag"] = self.tag
    if self.width is not None:
      d["width"] = self.width
    if self.is_signed is not None:
      d["signed"] = self.is_signed
    if self.pointer_width_alias is not None:
      d["pointer_width_alias"] = self.pointer_width_alias
    if self.bits:
      d["bits"] = [b.to_json() for b in self.bits]
    return d

  @staticmethod
  def from_json(d) -> "StorageType":
    _check(isinstance(d, dict), f"storage type: expected object, got {d!r}")
    _check_keys(d, {"kind"}, {
        "name", "arg", "tag", "width", "signed", "pointer_width_alias", "bits"
    }, "storage type")
    arg = d.get("arg")
    return StorageType(
        kind=d.get("kind", ""),
        arg=CppType.from_json(arg) if arg is not None else None,
        tag=d.get("tag"),
        width=d.get("width"),
        is_signed=d.get("signed"),
        pointer_width_alias=d.get("pointer_width_alias"),
        name=d.get("name"),
        bits=tuple(BitFieldEntry.from_json(b) for b in d.get("bits", ())))


@dataclasses.dataclass(frozen=True)
class Field:
  """One fixed-offset field."""

  cpp_name: str  # C++ member name, e.g. "maybe_value_"
  # The C++ type as written; `storage` does not keep it.
  cpp_type: str
  offset: int  # byte offset from the start of the object
  size: int  # total byte size (element size * extent for arrays)
  storage: StorageType
  # Element count of a fixed-extent C++ array member. None for scalars.
  array_extent: int | None = None
  # The V8_TQ_* annotations written on the member, uninterpreted.
  annotations: tuple[Annotation, ...] = ()
  # Stored only in the positions document and excluded from equality.
  position: str | None = dataclasses.field(default=None, compare=False)

  def __post_init__(self) -> None:
    _check_identifier(self.cpp_name, "field cpp_name")
    _check(
        isinstance(self.cpp_type, str) and self.cpp_type != "",
        f"{self.cpp_name}: empty cpp_type")
    object.__setattr__(self, "annotations",
                       canonical_annotations(self.annotations, self.cpp_name))
    _check(self.offset >= 0, f"{self.cpp_name}: negative offset")
    # Zero only for the zero-length-array padding idiom.
    _check(self.size >= 0, f"{self.cpp_name}: negative size")
    if self.array_extent is not None:
      _check(self.array_extent > 0, f"{self.cpp_name}: invalid array extent")
      _check(self.size % self.array_extent == 0,
             f"{self.cpp_name}: size not a multiple of array extent")

  def to_json(self):
    d = {
        "cpp_name": self.cpp_name,
        "cpp_type": self.cpp_type,
        "offset": self.offset,
        "size": self.size,
        "storage": self.storage.to_json(),
    }
    if self.array_extent is not None:
      d["array_extent"] = self.array_extent
    if self.annotations:
      d["annotations"] = annotations_json(self.annotations)
    return d

  @staticmethod
  def from_json(d) -> "Field":
    _check(isinstance(d, dict), f"field: expected object, got {d!r}")
    _check_keys(d, {"cpp_name", "cpp_type", "offset", "size", "storage"},
                {"array_extent", "annotations"}, "field")
    return Field(
        cpp_name=d["cpp_name"],
        cpp_type=d["cpp_type"],
        offset=d["offset"],
        size=d["size"],
        storage=StorageType.from_json(d["storage"]),
        array_extent=d.get("array_extent"),
        annotations=annotations_from_json(d, d["cpp_name"]))


@dataclasses.dataclass(frozen=True)
class Tail:
  """A trailing variable-length section (one FLEXIBLE_ARRAY_MEMBER).

  ClassLayout.annotations describes how Torque represents the tail.
  Member annotations are stored here. The tail is recorded even without
  annotations.
  """

  cpp_name: str
  cpp_element_type: str  # see Field.cpp_type
  offset: int  # == ClassLayout.size, the fixed-header end
  element_size: int
  element_storage: StorageType
  # The V8_TQ_* annotations written on the flexible array member itself.
  annotations: tuple[Annotation, ...] = ()
  # See Field.position.
  position: str | None = dataclasses.field(default=None, compare=False)

  def __post_init__(self) -> None:
    _check_identifier(self.cpp_name, "tail cpp_name")
    _check(
        isinstance(self.cpp_element_type, str) and self.cpp_element_type != "",
        f"{self.cpp_name}: empty cpp_element_type")
    object.__setattr__(self, "annotations",
                       canonical_annotations(self.annotations, self.cpp_name))
    _check(self.offset >= 0, "tail: negative offset")
    _check(self.element_size > 0, "tail: non-positive element size")

  def to_json(self):
    d = {
        "cpp_name": self.cpp_name,
        "cpp_element_type": self.cpp_element_type,
        "offset": self.offset,
        "element_size": self.element_size,
        "element_storage": self.element_storage.to_json(),
    }
    if self.annotations:
      d["annotations"] = annotations_json(self.annotations)
    return d

  @staticmethod
  def from_json(d) -> "Tail":
    _check(isinstance(d, dict), f"tail: expected object, got {d!r}")
    _check_keys(d, {
        "cpp_name", "cpp_element_type", "offset", "element_size",
        "element_storage"
    }, {"annotations"}, "tail")
    return Tail(
        cpp_name=d["cpp_name"],
        cpp_element_type=d["cpp_element_type"],
        offset=d["offset"],
        element_size=d["element_size"],
        element_storage=StorageType.from_json(d["element_storage"]),
        annotations=annotations_from_json(d, d["cpp_name"]))


@dataclasses.dataclass(frozen=True)
class ClassLayout:
  """One exported class layout.

  `fields` holds only the fields this class introduces -- everything at
  offsets in [base_size, size), flattened across intermediate
  C++ bases without exported layouts. Inherited fields live on the base's own
  layout. Contiguity is enforced: V8_OBJECT bodies compile with -Wpadded
  as an error, so a gap can only mean an unmodeled member.
  """

  cpp_name: str  # fully qualified, e.g. "v8::internal::Cell"
  # None only for the hierarchy root, HeapObject.
  base_cpp_name: str | None
  base_size: int  # sizeof(base_cpp_name); 0 for the root
  size: int  # sizeof(class); for tailed classes the fixed-header size
  alignment: int
  fields: tuple[Field, ...]
  tail: Tail | None = None
  # The V8_TQ_* annotations written on the class itself, uninterpreted.
  annotations: tuple[Annotation, ...] = ()
  # See Field.position.
  position: str | None = dataclasses.field(default=None, compare=False)

  def __post_init__(self) -> None:
    _check("::" in self.cpp_name, f"{self.cpp_name}: cpp_name not qualified")
    object.__setattr__(self, "annotations",
                       canonical_annotations(self.annotations, self.cpp_name))
    if self.base_cpp_name is not None:
      _check("::" in self.base_cpp_name,
             f"{self.base_cpp_name}: base cpp_name not qualified")
      _check(self.base_size > 0, f"{self.cpp_name}: base_size must be > 0")
    else:
      _check(self.base_size == 0, f"{self.cpp_name}: root with base_size")
    _check(self.size >= self.base_size,
           f"{self.cpp_name}: smaller than its base")
    _check(self.alignment > 0 and (self.alignment & (self.alignment - 1)) == 0,
           f"{self.cpp_name}: alignment not a power of two")

    # Fields are ordered, in range, and contiguous from the base to the
    # end of the fixed header.
    pos = self.base_size
    for f in self.fields:
      _check(
          f.offset == pos, f"{self.cpp_name}.{f.cpp_name}: offset "
          f"{f.offset}, expected {pos} (gap or overlap)")
      pos += f.size
    _check(
        pos == self.size, f"{self.cpp_name}: fields end at {pos}, "
        f"class size is {self.size}")
    if self.tail is not None:
      _check(
          self.tail.offset == self.size,
          f"{self.cpp_name}: tail at {self.tail.offset}, header ends "
          f"at {self.size}")

    names = [f.cpp_name for f in self.fields]
    _check(
        len(names) == len(set(names)),
        f"{self.cpp_name}: duplicate field names")

  def to_json(self):
    d = {
        "cpp_name": self.cpp_name,
        "base": self.base_cpp_name,
        "base_size": self.base_size,
        "size": self.size,
        "alignment": self.alignment,
        "fields": [f.to_json() for f in self.fields],
    }
    if self.annotations:
      d["annotations"] = annotations_json(self.annotations)
    if self.tail is not None:
      d["tail"] = self.tail.to_json()
    return d

  @staticmethod
  def from_json(d) -> "ClassLayout":
    _check(isinstance(d, dict), f"class: expected object, got {d!r}")
    _check_keys(
        d, {"cpp_name", "base", "base_size", "size", "alignment", "fields"},
        {"tail", "annotations"}, "class")
    tail = d.get("tail")
    return ClassLayout(
        cpp_name=d["cpp_name"],
        base_cpp_name=d["base"],
        base_size=d["base_size"],
        size=d["size"],
        alignment=d["alignment"],
        fields=tuple(Field.from_json(f) for f in d["fields"]),
        tail=Tail.from_json(tail) if tail is not None else None,
        annotations=annotations_from_json(d, d["cpp_name"]))


def _check_keys(d: dict, required: set, optional: set, what: str) -> None:
  keys = set(d.keys())
  missing = required - keys
  _check(not missing, f"{what}: missing keys {sorted(missing)}")
  unknown = keys - required - optional
  _check(not unknown, f"{what}: unknown keys {sorted(unknown)}")


def canonical_order(classes: list[ClassLayout]) -> list[ClassLayout]:
  """The canonical emission order: a pre-order walk of the inheritance
  forest, siblings and roots by fully-qualified C++ name.

  A class whose base is absent from the set roots its own tree, so a
  partial harvest still orders. The order depends only on the layouts
  themselves, which is what lets parse_document check that a document
  is in canonical form.
  """
  names = [c.cpp_name for c in classes]
  if len(names) != len(set(names)):
    dupes = sorted({n for n in names if names.count(n) > 1})
    raise LayoutError("duplicate class layouts: " + ", ".join(dupes))
  known = set(names)
  children: dict[str, list[ClassLayout]] = {}
  roots = []
  for c in sorted(classes, key=lambda x: x.cpp_name):
    if c.base_cpp_name is not None and c.base_cpp_name in known:
      children.setdefault(c.base_cpp_name, []).append(c)
    else:
      roots.append(c)

  ordered = []
  stack = list(reversed(roots))
  while stack:
    c = stack.pop()
    ordered.append(c)
    stack.extend(reversed(children.get(c.cpp_name, ())))
  if len(ordered) != len(classes):
    unreached = sorted(known - {c.cpp_name for c in ordered})
    raise LayoutError("cycle in the base chain of: " + ", ".join(unreached))
  return ordered


def serialize_document(config: Config, classes: list[ClassLayout]) -> str:
  """Deterministic JSON for a set of class layouts.

  Classes emit in canonical_order; key order is the fixed to_json order.
  """
  _check(isinstance(config, Config), "document: config is not a Config")
  doc = {
      "schema_version": SCHEMA_VERSION,
      "config": config.to_json(),
      "classes": [c.to_json() for c in canonical_order(classes)],
  }
  return json.dumps(doc, indent=1) + "\n"


def serialize_positions(classes: list[ClassLayout]) -> str:
  """Deterministic positions document: C++ source positions for every
  class and member, keyed by cpp_name. Separate from the layout
  document so that only consumers that need positions (diagnostics,
  cross-references) read it, and the layout document does not change
  when declarations merely move."""
  entries = []
  for c in canonical_order(classes):
    _check(bool(c.position), f"{c.cpp_name}: no source position")
    members = {}
    for f in c.fields:
      _check(bool(f.position), f"{c.cpp_name}.{f.cpp_name}: no source position")
      members[f.cpp_name] = f.position
    if c.tail is not None:
      _check(
          bool(c.tail.position),
          f"{c.cpp_name}.{c.tail.cpp_name}: no source position")
      _check(c.tail.cpp_name not in members,
             f"{c.cpp_name}: tail name collides with a field")
      members[c.tail.cpp_name] = c.tail.position
    entries.append({
        "cpp_name": c.cpp_name,
        "position": c.position,
        "members": dict(sorted(members.items())),
    })
  doc = {"schema_version": SCHEMA_VERSION, "classes": entries}
  return json.dumps(doc, indent=1) + "\n"


def parse_document(text: str) -> tuple[Config, list[ClassLayout]]:
  """Parse and validate a serialized document.

  Torque's C++ loader reads the document independently; this parser
  pins the schema contract in tests and gives tools a validated
  reader.
  """
  try:
    doc = json.loads(text)
  except json.JSONDecodeError as e:
    raise LayoutError(f"not valid JSON: {e}") from e
  _check(isinstance(doc, dict), "document: expected object")
  _check_keys(doc, {"schema_version", "config", "classes"}, set(), "document")
  version = doc["schema_version"]
  _check(version == SCHEMA_VERSION,
         f"schema version {version!r} != supported {SCHEMA_VERSION}")
  config = Config.from_json(doc["config"])
  classes = [ClassLayout.from_json(c) for c in doc["classes"]]
  # Reject documents that are valid but not canonical.
  _check(
      serialize_document(config, classes) == text,
      "document is not in canonical serialized form")
  return config, classes
