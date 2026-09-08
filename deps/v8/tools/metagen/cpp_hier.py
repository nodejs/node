# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Harvest the class hierarchy from C++ V8_OBJECT headers via libclang.

Produces `ClassInfo` records (see instance_types.py) for every class
that transitively inherits HeapObject in the input headers, sourced from the
`[[clang::annotate("V8_IT_*")]]` markers carried by aliases in each class
body. Feed the list into `instance_types.assign_instance_types` to drive IT
generation from the C++ source-of-truth.

The annotation set we surface:

  V8_IT_ABSTRACT          -> is_abstract
  V8_IT_REUSE_PARENT      -> has_same_instance_type_as_parent
  V8_IT_FIXED_VALUE(N)    -> constraints.value = N
  V8_IT_FLAG_BITS(N)      -> constraints.num_flags_bits = N
  V8_IT_ORDER(FIRST)      -> lowest_within_parent
  V8_IT_ORDER(LAST)       -> highest_within_parent
  V8_IT_NO_AUTO_CHECKER   -> no_auto_checker

Inheritance is resolved through templates by the shared harness in
`extract.py`.

This module references `clang.cindex` at import time, so
`clang_bootstrap.bootstrap()` must have run first -- metagen.py does
that before importing us.
"""

from __future__ import annotations

import dataclasses
import os
import re
import subprocess
import sys

import clang.cindex as cindex

from metagen import extract
from metagen.instance_types import ClassInfo, InstanceTypeConstraints

_V8_IT_ABSTRACT = "V8_IT_ABSTRACT"
_V8_IT_REUSE_PARENT = "V8_IT_REUSE_PARENT"
_V8_IT_OWN_TYPE = "V8_IT_OWN_TYPE"
_V8_IT_NO_AUTO_CHECKER = "V8_IT_NO_AUTO_CHECKER"
_V8_IT_NO_AUTO_DISPATCH = "V8_IT_NO_AUTO_DISPATCH"
_V8_IT_FIXED_VALUE_RE = re.compile(r"^V8_IT_FIXED_VALUE\((.+)\)$")
_V8_IT_FLAG_BITS_RE = re.compile(r"^V8_IT_FLAG_BITS\((.+)\)$")
_V8_IT_ORDER_RE = re.compile(r"^V8_IT_ORDER\((FIRST|LAST)\)$")
# Anchored, because the V8_OBJECTS_<NAME>_H_ include guards share a
# prefix with V8_OBJECT and would otherwise match.
_MARKER_MACRO_RE = re.compile(
    r"^(V8_IT_[A-Z_]+|V8_ABSTRACT_OBJECT"
    r"|V8_OBJECT(_END|_PUSH|_POP|_INNER_CLASS(_END)?)?)$")


@dataclasses.dataclass
class ScanResult:
  classes: list[ClassInfo]
  # Absolute paths of the files the harvest read a fact out of -- class
  # declarations, the bases and templates it resolved through, and the
  # marker definitions. Derived from the walk, not filtered from the
  # include list, so it stays correct when a declaration moves.
  provenance: list[str] = dataclasses.field(default_factory=list)


def _annotations(cursor: cindex.Cursor) -> list[str]:
  return extract.class_annotations(cursor)


def _has_annotation(cursor: cindex.Cursor, name: str) -> bool:
  return name in extract.class_annotations(cursor)


def _parse_it_int(text: str, cursor: cindex.Cursor, attr: str) -> int:
  try:
    return int(text.strip(), 0)
  except ValueError:
    loc = cursor.location
    where = f"{loc.file.name}:{loc.line}" if loc.file else "<unknown>"
    print(
        f"[metagen] {where}: {cursor.spelling}: malformed {attr} argument "
        f"{text.strip()!r}; expected an integer literal.",
        file=sys.stderr)
    sys.exit(1)


def _class_info_from_cursor(
    cursor: cindex.Cursor,
    v8_root: str,
    base: str | None = None,
) -> ClassInfo:
  c = InstanceTypeConstraints()
  is_abstract = False
  reuse_parent = False
  highest = False
  lowest = False
  no_auto_checker = False
  no_auto_dispatch = False
  for a in _annotations(cursor):
    if a == _V8_IT_ABSTRACT:
      is_abstract = True
    elif a == _V8_IT_REUSE_PARENT:
      reuse_parent = True
    elif a == _V8_IT_NO_AUTO_CHECKER:
      no_auto_checker = True
    elif a == _V8_IT_NO_AUTO_DISPATCH:
      no_auto_dispatch = True
    elif m := _V8_IT_FIXED_VALUE_RE.match(a):
      c.value = _parse_it_int(m.group(1), cursor, "V8_IT_FIXED_VALUE")
    elif m := _V8_IT_FLAG_BITS_RE.match(a):
      c.num_flags_bits = _parse_it_int(m.group(1), cursor, "V8_IT_FLAG_BITS")
    elif m := _V8_IT_ORDER_RE.match(a):
      if m.group(1) == "LAST":
        highest = True
      else:
        lowest = True

  if base is None:
    base = extract.get_base_name(cursor)

  # Diagnostic method detection: DECL_PRINTER(Name) / DECL_VERIFIER(Name)
  # expand to `void Name##Print(std::ostream&)` / `void Name##Verify
  # (Isolate*)` declarations (metagen.py parses with -DOBJECT_PRINT
  # -DVERIFY_HEAP so they are visible in every config).
  name = cursor.spelling
  printer_name = name + "Print"
  verifier_name = name + "Verify"
  has_printer = False
  has_verifier = False
  for ch in cursor.get_children():
    if ch.kind == cindex.CursorKind.CXX_METHOD:
      if ch.spelling == printer_name:
        has_printer = True
      elif ch.spelling == verifier_name:
        has_verifier = True

  return ClassInfo(
      name=name,
      base=base,
      position=_position(cursor, v8_root),
      is_abstract=is_abstract,
      has_same_instance_type_as_parent=reuse_parent,
      highest_within_parent=highest,
      lowest_within_parent=lowest,
      constraints=c,
      no_auto_checker=no_auto_checker,
      has_printer=has_printer,
      has_verifier=has_verifier,
      no_auto_dispatch=no_auto_dispatch,
  )


def _dump_parse_failure(v8_root: str,
                        driver_path: str,
                        parse_flags: list[str],
                        parse_cwd: str | None = None) -> None:
  """Surface libclang's failure on an unrecoverable TU parse.

  libclang's Python binding loses diagnostics when CXTranslationUnit is
  NULL (the from_source path raises TranslationUnitLoadError before
  returning a TU). The clang binary, given the same driver + flags via
  -fsyntax-only, prints the underlying error to stderr. Run it and
  forward stderr so the user actually sees what's wrong (typically a
  sysroot / target-triple / include-path issue) instead of an opaque
  "Error parsing translation unit."

  Best-effort: if the bundled clang is missing or the subprocess fails
  to spawn, we just log that fact and let the original exception
  propagate.
  """
  # libclang's argv mirrors the GCC-style clang driver, not clang-cl.
  # On Linux/Mac the Chromium toolchain ships clang directly; on
  # Windows it ships only clang-cl.exe -- invoke that with
  # --driver-mode=g++ to make it parse the same flag set libclang did.
  # Probe both <v8_root>/third_party/... and <v8_root>/../third_party/...
  # to handle Chromium subproject builds where the toolchain lives at
  # the chromium toplevel.
  clang_extra_args: list[str] = []
  if sys.platform == "win32":
    clang_rel = "third_party/llvm-build/Release+Asserts/bin/clang-cl.exe"
    clang_extra_args = ["--driver-mode=g++"]
  else:
    clang_rel = "third_party/llvm-build/Release+Asserts/bin/clang"
  clang_bin = ""
  for root in (v8_root, os.path.abspath(os.path.join(v8_root, os.pardir))):
    candidate = os.path.join(root, clang_rel)
    if os.path.isfile(candidate):
      clang_bin = candidate
      break
  print(
      "[metagen] libclang refused the TU; re-running via clang -fsyntax-only "
      "to surface diagnostics:",
      file=sys.stderr)
  print(f"[metagen]   clang: {clang_bin}", file=sys.stderr)
  print(f"[metagen]   driver: {driver_path}", file=sys.stderr)
  print(
      f"[metagen]   flags ({len(parse_flags)}): "
      f"{' '.join(parse_flags[:12])}{' ...' if len(parse_flags) > 12 else ''}",
      file=sys.stderr)
  if not clang_bin or not os.path.isfile(clang_bin):
    print(
        f"[metagen]   (clang binary not found; cannot re-run)", file=sys.stderr)
    return
  try:
    proc = subprocess.run(
        [clang_bin] + clang_extra_args + ["-fsyntax-only", driver_path] +
        parse_flags,
        cwd=parse_cwd or v8_root,
        capture_output=True,
        text=True,
        timeout=60,
    )
  except (OSError, subprocess.SubprocessError) as e:
    print(f"[metagen]   (subprocess failed: {e})", file=sys.stderr)
    return
  if proc.stderr:
    print("[metagen] clang stderr (first 80 lines):", file=sys.stderr)
    for line in proc.stderr.splitlines()[:80]:
      print(f"[metagen]   {line}", file=sys.stderr)
  print(f"[metagen] clang exited with {proc.returncode}", file=sys.stderr)


def scan_cpp(v8_root: str,
             driver_path: str,
             flags: list[str],
             parse_cwd: str | None = None) -> ScanResult:
  """Parse `driver_path` via libclang and return ClassInfo records.

  The driver's active direct includes are the headers to harvest. The order of
  returned records mirrors AST preorder, which equals (header inclusion order,
  source-position order within each header). That order is stable for a given
  driver + V8 tree.

  `parse_cwd`, when set, is chdir'd into around the libclang parse so
  the relative path-bearing flags from ninja's compile_commands.json
  (-I../../v8/src, --sysroot=../../build/..., ...) resolve the same way
  clang itself would resolve them in the build.
  """
  driver_path = os.path.abspath(driver_path)
  options = (
      cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
      | cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
  index = cindex.Index.create()
  parse_flags = list(flags)
  # libclang resolves relative path-bearing flags (-I, --sysroot, ...)
  # and stores file paths in source locations against the current
  # working directory. Chdir to parse_cwd so flags from ninja's
  # compile_commands.json (which are relative to the build dir)
  # resolve the way clang itself resolved them in the build, and so
  # the post-parse abspath() comparisons against `wanted` see the
  # same absolute paths libclang recorded.
  saved_cwd = os.getcwd() if parse_cwd else None
  if parse_cwd:
    os.chdir(parse_cwd)
  try:
    try:
      tu = index.parse(driver_path, args=parse_flags, options=options)
    except cindex.TranslationUnitLoadError:
      # libclang couldn't even start the parse -- typically a sysroot
      # / include-path / target-triple mismatch (Windows MSVC paths,
      # missing builtin headers, ...). At this point the TU is NULL
      # so we have no diagnostics to read via libclang. Re-run the
      # parse via the clang binary as a subprocess; its stderr carries
      # the actual error.
      _dump_parse_failure(v8_root, driver_path, parse_flags, parse_cwd)
      raise
    return _harvest_classes(tu, v8_root)
  finally:
    if saved_cwd is not None:
      os.chdir(saved_cwd)


def _position(cursor: cindex.Cursor, v8_root: str) -> str:
  """`file.h:line:col`, relative to v8_root so the harvest's output and
  its diagnostics are reproducible across checkouts."""
  loc = cursor.location
  fname = os.path.relpath(loc.file.name, v8_root) if loc.file else "<unknown>"
  return f"{fname}:{loc.line}:{loc.column}"


_SCOPE_KINDS = (cindex.CursorKind.NAMESPACE, cindex.CursorKind.CLASS_DECL,
                cindex.CursorKind.STRUCT_DECL, cindex.CursorKind.CLASS_TEMPLATE,
                cindex.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION)


def _qualified_name(cursor: cindex.Cursor) -> str:
  """Return `cursor`'s name with its namespaces and enclosing classes,
  e.g. "v8::internal::FixedArray::Iterator".

  The enclosing classes are what separates the four v8::internal classes
  named Iterator; the namespaces are what separates v8::Context from the
  object.
  """
  parts = [cursor.spelling]
  parent = cursor.semantic_parent
  while parent is not None and parent.kind != cindex.CursorKind.TRANSLATION_UNIT:
    if parent.kind in _SCOPE_KINDS:
      parts.append(parent.spelling)
    parent = parent.semantic_parent
  return "::".join(reversed(parts))


def _harvest_classes(tu, v8_root: str) -> ScanResult:
  # Never consume Clang's recovery AST. A missing declaration can corrupt a
  # base edge or field type while leaving enough cursors for generation to
  # appear successful, which is especially unsafe for object layout metadata.
  errors = [d for d in tu.diagnostics if d.severity >= cindex.Diagnostic.Error]
  if errors:
    for d in errors[:30]:
      print(
          f"[metagen] diag {d.severity} {d.location}: {d.spelling}",
          file=sys.stderr)
    print(
        f"[metagen] libclang reported {len(errors)} error diagnostic(s); "
        "refusing to harvest a recovery AST.",
        file=sys.stderr)
    sys.exit(1)

  # Pre-index every CLASS_TEMPLATE definition in the TU so the
  # inheritance walk can resolve template-instantiated base types
  # through partial specializations + parameter substitution. AND
  # collect the candidate set (CLASS_DECL / CLASS_TEMPLATE definitions
  # declared in input headers that reach HeapObject) in the same walk.
  #
  # Walking via `tu.cursor.walk_preorder()` over the full TU costs ~9s
  # on the V8 driver TU (~750k cursors, dominated by libclang's
  # ctypes-bound Cursor.__eq__ inside the visitor callback). Most of
  # those cursors are inside function declarations, typedefs, etc. --
  # places the harvest never cares about. Walk via an explicit
  # recursion that descends only into the containers that can hold
  # class definitions; this drops the cursor count by ~5x and the
  # AST-walk time by ~10x.
  _DESCEND_KINDS = frozenset((
      cindex.CursorKind.TRANSLATION_UNIT,
      cindex.CursorKind.NAMESPACE,
      cindex.CursorKind.CLASS_DECL,
      cindex.CursorKind.STRUCT_DECL,
      cindex.CursorKind.CLASS_TEMPLATE,
      cindex.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION,
      cindex.CursorKind.LINKAGE_SPEC,
      cindex.CursorKind.UNEXPOSED_DECL,
  ))
  _TEMPLATE_KINDS = (cindex.CursorKind.CLASS_TEMPLATE,
                     cindex.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION)
  _HARVEST_KINDS = (cindex.CursorKind.CLASS_DECL,
                    cindex.CursorKind.CLASS_TEMPLATE)
  templates_idx: dict[str, list[cindex.Cursor]] = {}
  raw_candidates: dict[str, cindex.Cursor] = {}
  # Cursors that lost an unqualified-name collision, for the check below.
  shadowed: dict[str, list[cindex.Cursor]] = {}

  def _walk(cur):
    k = cur.kind
    if k in _TEMPLATE_KINDS and cur.is_definition():
      templates_idx.setdefault(cur.spelling, []).append(cur)
    if k in _HARVEST_KINDS and cur.is_definition():
      # V8_IT_NO_AUTO_CHECKER normally excludes the class from the harvest.
      # Combined with V8_IT_OWN_TYPE, it instead keeps the class in the IT
      # tree while suppressing only its generated checker buckets.
      no_auto_checker = _has_annotation(cur, _V8_IT_NO_AUTO_CHECKER)
      own_type = _has_annotation(cur, _V8_IT_OWN_TYPE)
      if not no_auto_checker or own_type:
        loc = cur.location
        # The driver reaches far more than the objects -- the public API
        # headers among it -- and `raw_candidates` is keyed by unqualified
        # name, so v8::Context would claim the key of the object it shares
        # a name with and never be reconsidered. Select on the namespace:
        # the harvest wants v8::internal and nothing else.
        if _qualified_name(cur).startswith("v8::internal::"):
          if raw_candidates.setdefault(cur.spelling, cur) != cur:
            shadowed.setdefault(cur.spelling, []).append(cur)
    if k in _DESCEND_KINDS or k in _TEMPLATE_KINDS:
      for child in cur.get_children():
        _walk(child)

  _walk(tu.cursor)

  def _reaches_heap_object(cur: cindex.Cursor) -> bool:
    return cur.spelling == extract.HEAP_OBJECT_ROOT or (
        extract.resolve_logical_base(cur, frozenset(),
                                     templates=templates_idx) is not None)

  # `raw_candidates` is keyed by unqualified name, so two v8::internal
  # classes sharing one -- FixedArray::Iterator and BitVector::Iterator,
  # say -- collide, and whichever the walk reached first keeps the key.
  # That is only harmless while at most one of them is a heap object: if
  # the loser is, it silently leaves the harvest and its class gets no
  # instance type. Refuse to guess.
  # TODO(jgruber): key on the qualified name instead, which removes the
  # ambiguity rather than reporting it. That means teaching
  # resolve_logical_base to match bases on qualified names too, where the
  # names come from three sources at different qualification levels, so
  # it wants its own change.
  for name, losers in sorted(shadowed.items()):
    winner = raw_candidates[name]
    claimants = {}
    for cur in [winner] + losers:
      if _reaches_heap_object(cur):
        claimants.setdefault(_qualified_name(cur), cur)
    if not claimants:
      continue
    if len(claimants) > 1 or _qualified_name(winner) not in claimants:
      print(
          f"[metagen] `{name}` is claimed by more than one v8::internal "
          f"class that reaches {extract.HEAP_OBJECT_ROOT}:",
          file=sys.stderr)
      for qualified, cur in sorted(claimants.items()):
        print(
            f"[metagen]   {qualified} ({_position(cur, v8_root)})",
            file=sys.stderr)
      print(
          "[metagen] The harvest identifies classes by unqualified name "
          "and cannot tell them apart; rename one.",
          file=sys.stderr)
      sys.exit(1)

  # Inheritance test: keep raw_candidates that are HeapObject itself or
  # transitively reach HeapObject. `resolve_logical_base` with an
  # empty target set returns HEAP_OBJECT_ROOT iff a chain exists.
  # HeapObject itself has no base spec, so handle that explicitly.
  #
  # Collects every declaration the walks consult, including ones that
  # ruled a candidate out: a header that keeps a class out of the
  # harvest decides the output too.
  visited: set[cindex.Cursor] = set()
  candidates: dict[str, cindex.Cursor] = {
      name: cur
      for name, cur in raw_candidates.items()
      if name == extract.HEAP_OBJECT_ROOT or extract.resolve_logical_base(
          cur, frozenset(), templates=templates_idx, visited=visited)
      is not None
  }

  # Compute the leaf set. A candidate is a leaf if no other candidate
  # names it as the closest logical base. The base resolution here
  # uses the full candidate set so intermediate candidates count as
  # terminal ancestors; without that, a chain like
  # JSDeferredModuleNamespace -> JSModuleNamespace -> ... -> HeapObject
  # would skip past JSModuleNamespace and mark it as a leaf.
  candidate_names = set(candidates.keys())
  non_leaves: set[str] = set()
  for cur in candidates.values():
    p = extract.resolve_logical_base(
        cur, candidate_names, templates=templates_idx, visited=visited)
    if p is not None and p != extract.HEAP_OBJECT_ROOT:
      non_leaves.add(p)

  # Pass 2: participating = leaves OR annotated candidates. An
  # intermediate candidate that lacks any V8_IT_* annotation is pure
  # inheritance bookkeeping (e.g. an internal infrastructure class),
  # not an IT participant. V8_IT_OWN_TYPE is the opt-in for concrete
  # non-leaves that need their own _TYPE value; V8_IT_NO_AUTO_CHECKER
  # candidates were filtered out above.
  participating: dict[str, cindex.Cursor] = {
      name: cur
      for name, cur in candidates.items()
      if name not in non_leaves or extract.has_v8_it_annotation(cur)
  }

  # Resolve each participating class's base against the *participating*
  # set, not the candidates set: when an intermediate falls out of
  # participation (unannotated infrastructure class), the walker must
  # skip over it to find the closest still-participating ancestor.
  participating_names = set(participating.keys())
  classes: list[ClassInfo] = [
      _class_info_from_cursor(
          cur,
          v8_root=v8_root,
          base=extract.resolve_logical_base(
              cur,
              participating_names,
              templates=templates_idx,
              visited=visited),
      ) for cur in participating.values()
  ]

  # The V8_IT_* / V8_OBJECT markers are read from class declarations, so the
  # header defining them decides what every annotation means.
  for cursor in tu.cursor.get_children():
    if (cursor.kind == cindex.CursorKind.MACRO_DEFINITION and
        _MARKER_MACRO_RE.match(cursor.spelling)):
      visited.add(cursor)

  provenance: set[str] = set()
  # Prefix test rather than os.path.commonpath, which raises across
  # drives on Windows -- and `visited` reaches the toolchain and sysroot
  # declarations, which need not share the checkout's drive.
  root = os.path.abspath(v8_root) + os.sep
  for cursor in visited:
    loc = cursor.location
    if loc is None or loc.file is None:
      continue
    path = os.path.abspath(loc.file.name)
    if path.startswith(root):
      provenance.add(path)

  return ScanResult(classes=classes, provenance=sorted(provenance))
