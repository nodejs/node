# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared libclang harness for the metagen pipeline.

Inheritance walking through templates plus V8_IT_* annotation
discovery. Shared between IT-mode harvesting (`cpp_hier.py`) and
layout-mode generation (the `metagen-layout` branch); keep
IT-emission concerns out of here.

This module references `clang.cindex` at import time, so
`clang_bootstrap.bootstrap()` must have run first -- metagen.py does
that before importing any consumer of this module.
"""

from __future__ import annotations

import clang.cindex as cindex

HEAP_OBJECT_ROOT = "HeapObject"


def class_annotations(cursor: cindex.Cursor) -> list[str]:
  """Return the `[[clang::annotate]]` payloads on a class's members.

  V8_IT_MARK expands to an artificial member alias, so the attribute
  hangs off that member, never the class head. Annotations on real
  members come through too (absl's inline-me, the blink GC plugin's),
  so callers match on the V8_IT_ prefix or a full payload.
  """
  out: list[str] = []
  for child in cursor.get_children():
    for grandchild in child.get_children():
      if grandchild.kind == cindex.CursorKind.ANNOTATE_ATTR:
        out.append(grandchild.spelling)
  return out


def _strip_ns(spelling: str) -> str:
  """Strip v8::internal:: namespace prefixes for readability."""
  return spelling.replace("v8::internal::", "").replace("v8::", "")


def has_v8_it_annotation(cursor: cindex.Cursor) -> bool:
  """Class carries any `[[clang::annotate("V8_IT_*")]]` marker."""
  return any(s.startswith("V8_IT_") for s in class_annotations(cursor))


# Cursor kinds we'll treat as "class-like definitions" for inheritance.
_CLASS_LIKE = (
    cindex.CursorKind.CLASS_DECL,
    cindex.CursorKind.STRUCT_DECL,
    cindex.CursorKind.CLASS_TEMPLATE,
    cindex.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION,
)

_TEMPLATE_PARAM_KINDS = (
    cindex.CursorKind.TEMPLATE_TYPE_PARAMETER,
    cindex.CursorKind.TEMPLATE_NON_TYPE_PARAMETER,
    cindex.CursorKind.TEMPLATE_TEMPLATE_PARAMETER,
)


def _template_params(cursor: cindex.Cursor) -> list[str]:
  return [
      ch.spelling
      for ch in cursor.get_children()
      if ch.kind in _TEMPLATE_PARAM_KINDS
  ]


def _resolve_type_to_class_name(t: cindex.Type) -> str | None:
  """Best-effort: pick the class name out of a libclang Type.

  Falls back to `type.spelling` when `get_declaration()` returns an
  unnamed cursor (the bare-template-parameter case where
  `Type.kind == UNEXPOSED`).
  """
  decl = t.get_declaration()
  name = decl.spelling
  if not name and t.kind == cindex.TypeKind.UNEXPOSED:
    name = t.spelling
  return name or None


def resolve_logical_base(
    cursor: cindex.Cursor,
    participating: set[str],
    templates: dict[str, list[cindex.Cursor]] | None = None,
    args_map: dict[str, cindex.Type] | None = None,
    seen: set | None = None,
    visited: set[cindex.Cursor] | None = None,
) -> str | None:
  """Walk inheritance through templates; return the name of the closest
  ancestor that is in `participating` (the set of classes the harvest
  will emit) or `HeapObject`. Returns None when no chain reaches
  HeapObject.

  Doubles as a discovery predicate: with `participating` empty, the
  result is HEAP_OBJECT_ROOT (chain exists) or None (no chain), which
  is exactly the "transitively inherits HeapObject" test that
  participation filtering uses.

  `visited`, when given, collects every declaration the walk consults.
  The caller turns those into the depfile: the answer depends on each
  of them, including on the ones that ended the walk without a hit.

  Algorithm sketch:
    1. Walk each CXX_BASE_SPECIFIER of `cursor`.
    2. If the base is a bare template parameter (e.g. `: public Super`)
       bound to a concrete type via `args_map`, substitute and recurse.
    3. If the base is HeapObject or a `participating` class, return it.
    4. Otherwise, recurse into the base class/template definition with
       a substitution map built from its template args, taken from the
       base's canonical type so defaulted arguments are bound too.
  """
  if seen is None:
    seen = set()
  if templates is None:
    templates = {}
  if args_map is None:
    args_map = {}

  def _visit(c: cindex.Cursor | None) -> None:
    if visited is not None and c is not None:
      visited.add(c)

  _visit(cursor)
  name = cursor.spelling
  sig = (name, frozenset((k, v.spelling) for k, v in args_map.items() if v))
  if sig in seen:
    return None
  seen.add(sig)

  for child in cursor.get_children():
    if child.kind != cindex.CursorKind.CXX_BASE_SPECIFIER:
      continue
    bt = child.type
    base_decl = bt.get_declaration()
    _visit(base_decl)
    base_decl_name = base_decl.spelling
    if not base_decl_name and bt.kind == cindex.TypeKind.UNEXPOSED:
      base_decl_name = bt.spelling

    # (1) Bare template parameter -> substitute.
    if base_decl_name in args_map:
      sub = args_map[base_decl_name]
      if sub is None:
        continue
      sub_name = _resolve_type_to_class_name(sub)
      if sub_name == HEAP_OBJECT_ROOT:
        return HEAP_OBJECT_ROOT
      if sub_name in participating:
        return sub_name
      sub_def = sub.get_declaration().get_definition() or sub.get_declaration()
      _visit(sub_def)
      if sub_def.kind in _CLASS_LIKE:
        result = resolve_logical_base(sub_def, participating, templates, {},
                                      seen, visited)
        if result is not None:
          return result
      continue

    # (2) Direct hit on HeapObject.
    if base_decl_name == HEAP_OBJECT_ROOT:
      return HEAP_OBJECT_ROOT

    # (3) Direct hit on a participating class.
    if base_decl_name in participating:
      return base_decl_name

    # (4) Walk into the base class/template body.
    # The canonical type, not the written one: a base spelled
    # `PrimitiveArrayBase<ByteArray, uint8_t>` names two arguments and
    # leaves the third, `Super`, at its FixedArrayBase default -- and
    # that one is the instance-type parent.
    canonical = bt.get_canonical()
    nargs = canonical.get_num_template_arguments()
    template_args = [
        canonical.get_template_argument_type(i) for i in range(max(0, nargs))
    ]
    candidates: list[cindex.Cursor] = []
    if nargs > 0:
      candidates.extend(templates.get(base_decl_name, []))
      for _t in candidates:
        _visit(_t)
    base_def = base_decl.get_definition() or base_decl
    _visit(base_def)
    if base_def.kind in _CLASS_LIKE:
      candidates.append(base_def)
    for cand in candidates:
      params = _template_params(cand)
      sub = dict(zip(params, template_args)) if params and template_args \
            else {}
      result = resolve_logical_base(cand, participating, templates, sub, seen,
                                    visited)
      if result is not None:
        return result

  return None


def get_base_name(cursor: cindex.Cursor) -> str | None:
  """Return the (namespace-stripped) name of the first direct base."""
  for child in cursor.get_children():
    if child.kind == cindex.CursorKind.CXX_BASE_SPECIFIER:
      return _strip_ns(child.type.get_declaration().spelling)
  return None
