# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Instance-type IR, constraint solver, and macro emission.

The solver is a direct port of Torque's
`src/torque/instance-type-generator.cc`. We mirror its names, control
flow, data layout, and tie-breaking so the emitted macros are
comparable against Torque's reference emission (metagen.py --check;
comment-insensitive since the two generators emit different source
links).

Takes the flat `ClassInfo` list produced by `cpp_hier`, builds and
solves the IT tree, and emits:

  - `TORQUE_ASSIGNED_INSTANCE_TYPES` -- FIRST_/LAST_/_TYPE constants
  - `TORQUE_ASSIGNED_INSTANCE_TYPE_LIST` -- flat list of concrete _TYPEs
  - `INSTANCE_TYPE_LIST_{SINGLE,MULTIPLE,RANGE}` -- bucket lists that
    drive auto-emission of `IsX` predicates (instance-type-checker.h),
    `InstanceTypeTraits` declarations (instance-type-inl.h), and heap
    snapshot labels (heap-snapshot-generator.cc). The buckets are a
    pure function of the solved tree; the IT enum is the source of
    truth. There is no FULLY_DEFINED vs ONLY_DECLARED partition here --
    that distinction is about whether Torque emits a `TqClass` debug
    reader, a layout-availability concern owned by Torque.
  - `HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST` -- the Print/Verify dispatch
    cases for objects-printer.cc / objects-debug.cc, derived from
    detected Name##Print / Name##Verify declarations (see
    emit_dispatch_macro).

Mapped pieces:
  Torque                                | Python
  --------------------------------------|---------------------------
  InstanceTypeConstraints (constants.h) | InstanceTypeConstraints
  ClassType (subset)                    | ClassInfo
  InstanceTypeTree                      | InstanceTypeTree
  BuildInstanceTypeTree                 | build_instance_type_tree
  PropagateInstanceTypeConstraints      | propagate_constraints
  SolveInstanceTypeConstraints          | solve_constraints
  CompareUnconstrainedTypes             | _unconstrained_sort_key
  PrintInstanceTypes                    | emit_enum_macros +
                                          emit_bucket_macros
  GenerateInstanceTypes                 | (driven by metagen.py)
"""

from __future__ import annotations

import dataclasses
import sys

INT_MAX = (1 << 31) - 1
INT_MIN = -(1 << 31)


@dataclasses.dataclass
class InstanceTypeConstraints:
  """Matches torque's InstanceTypeConstraints struct (constants.h)."""
  value: int = -1  # @apiExposedInstanceTypeValue(N)
  num_flags_bits: int = -1  # @reserveBitsInInstanceType(N)


@dataclasses.dataclass
class ClassInfo:
  """One class participating in IT generation, with the subset of
  metadata the solver needs. Field names track Torque's `ClassType` /
  `InstanceTypeConstraints` so the solver can mirror the C++ logic
  line-for-line. Populated by the libclang harvest (`cpp_hier.py`)."""
  name: str
  base: str | None
  # Source position -- "file.h:line:col" (1-based). Diagnostics only;
  # deliberately absent from the emitted output (see the note above
  # _EnumEmission).
  position: str
  is_abstract: bool
  has_same_instance_type_as_parent: bool
  highest_within_parent: bool
  lowest_within_parent: bool
  constraints: InstanceTypeConstraints
  # V8_IT_NO_AUTO_CHECKER: omit this class from the checker bucket lists.
  # The class still participates in enum emission when combined with
  # V8_IT_OWN_TYPE.
  no_auto_checker: bool = False
  # Class declares Name##Print(std::ostream&) / Name##Verify(Isolate*)
  # (the harvest parses with -DOBJECT_PRINT -DVERIFY_HEAP so DECL_PRINTER
  # / DECL_VERIFIER expand). Drives the diagnostic-dispatch emission.
  has_printer: bool = False
  has_verifier: bool = False
  # V8_IT_NO_AUTO_DISPATCH: dispatched by a hand-written case in
  # objects-printer.cc / objects-debug.cc instead of the generated list.
  no_auto_dispatch: bool = False


@dataclasses.dataclass
class InstanceTypeTree:
  """Matches torque's InstanceTypeTree struct."""
  cls: ClassInfo
  children: list["InstanceTypeTree"] = dataclasses.field(default_factory=list)
  start: int = INT_MAX
  end: int = INT_MIN
  value: int = -1
  num_values: int = 0
  num_own_values: int = 0


def build_instance_type_tree(
    classes: list[ClassInfo]) -> InstanceTypeTree | None:
  """Assemble every ClassType into a single-rooted tree.

    Mirrors Torque's BuildInstanceTypeTree, which walks GlobalContext's
    type aliases. We walk the scanned ClassInfo list instead. If the
    scan picked up two classes with the same name we keep the first,
    matching Torque's "already encountered" check.
    """
  by_name: dict[str, InstanceTypeTree] = {}
  for c in classes:
    if c.name in by_name:
      continue
    by_name[c.name] = InstanceTypeTree(cls=c)

  root: InstanceTypeTree | None = None
  for node in by_name.values():
    parent_name = node.cls.base
    parent = by_name.get(parent_name) if parent_name else None
    # Torque's GetSuperClass returns nullptr for non-class parents
    # (type aliases like StrongTagged). Treat unknown names the same
    # way -- the node becomes a root candidate.
    if parent is None:
      if root is not None:
        print(
            f"[itypes] error: multiple roots: {root.cls.name} "
            f"({root.cls.position}) and {node.cls.name} "
            f"({node.cls.position}). The second class would be silently "
            f"dropped from IT generation; check that its base class is "
            f"reachable in the participating set.",
            file=sys.stderr)
        sys.exit(1)
      root = node
    else:
      parent.children.append(node)
  return root


def propagate_constraints(root: InstanceTypeTree) -> None:
  """Port of PropagateInstanceTypeConstraints.

    Post-order walk: child ranges roll up into parent start/end/num_values,
    then the root's own constraints are applied on top.
    """
  for child in root.children:
    propagate_constraints(child)
    if child.start < root.start:
      root.start = child.start
    if child.end > root.end:
      root.end = child.end
    root.num_values += child.num_values

  constraints = root.cls.constraints
  if not root.cls.is_abstract and not root.cls.has_same_instance_type_as_parent:
    root.num_own_values = 1
  root.num_values += root.num_own_values

  if constraints.num_flags_bits != -1:
    # Flag-class: subclasses share its bits, don't get their own values.
    root.children.clear()
    root.num_values = 1 << constraints.num_flags_bits
    root.num_own_values = root.num_values
    root.start = 0
    root.end = root.num_values - 1

  if constraints.value != -1:
    if root.num_own_values != 1:
      print(
          f"[itypes] error: explicit instance-type value on "
          f"abstract class {root.cls.name}",
          file=sys.stderr)
      sys.exit(1)
    root.value = constraints.value
    if constraints.value < root.start:
      root.start = constraints.value
    if constraints.value > root.end:
      root.end = constraints.value


def _select_own_values(root: InstanceTypeTree, start_value: int) -> int:
  """Port of SelectOwnValues."""
  if root.value == -1:
    root.value = start_value
  elif root.value < start_value:
    print(
        f"[itypes] error: failed to assign instance type "
        f"{root.value} to {root.cls.name} (start={start_value}) "
        f"({root.cls.position})",
        file=sys.stderr)
    sys.exit(1)
  return root.value + root.num_own_values


def _unconstrained_sort_key(node: InstanceTypeTree) -> tuple:
  """Port of CompareUnconstrainedTypes.

    Torque sorts DESCENDING by num_values, then ASCENDING by name.
    Python's sort key is naturally ascending, so invert num_values.
    """
  return (-node.num_values, node.cls.name)


def solve_constraints(
    root: InstanceTypeTree,
    start_value: int,
    destination: list[InstanceTypeTree],
) -> int:
  """Port of SolveInstanceTypeConstraints."""
  if root.start < start_value:
    print(
        f"[itypes] error: failed to assign instance type "
        f"{root.start} to {root.cls.name} (start={start_value}) "
        f"({root.cls.position})",
        file=sys.stderr)
    sys.exit(1)

  # Split children into four buckets, matching Torque exactly.
  lowest_child: InstanceTypeTree | None = None
  highest_child: InstanceTypeTree | None = None
  # Torque's std::multimap<int, ...> keeps insertion order among
  # equal keys but sorts by key ascending. Python sorting is stable,
  # so we collect then sort by start.
  constrained_children: list[InstanceTypeTree] = []
  # Torque's std::map<ITT*, ..., CompareUnconstrainedTypes> keeps keys
  # in descending-num_values / ascending-name order. We sort
  # explicitly below.
  unconstrained_children: list[InstanceTypeTree] = []

  for child in root.children:
    if child.cls.highest_within_parent:
      if highest_child is not None:
        print(
            f"[itypes] error: two highest children of "
            f"{root.cls.name}: {highest_child.cls.name}, "
            f"{child.cls.name}",
            file=sys.stderr)
        sys.exit(1)
      if child.cls.lowest_within_parent:
        print(
            f"[itypes] error: class both highest and lowest: "
            f"{child.cls.name}",
            file=sys.stderr)
        sys.exit(1)
      highest_child = child
    elif child.cls.lowest_within_parent:
      if lowest_child is not None:
        print(
            f"[itypes] error: two lowest children of "
            f"{root.cls.name}: {lowest_child.cls.name}, "
            f"{child.cls.name}",
            file=sys.stderr)
        sys.exit(1)
      lowest_child = child
    elif child.start > child.end:
      unconstrained_children.append(child)
    else:
      constrained_children.append(child)
  root.children = []

  constrained_children.sort(key=lambda n: n.start)
  unconstrained_children.sort(key=_unconstrained_sort_key)

  own_type_pending = root.num_own_values > 0

  if lowest_child is not None:
    start_value = solve_constraints(lowest_child, start_value, root.children)

  for constrained in constrained_children:
    # Try placing own type before the constrained child.
    if own_type_pending:
      fits_before = ((root.value != -1 and root.value < constrained.start) or
                     (root.value == -1 and
                      start_value + root.num_own_values <= constrained.start))
      if fits_before:
        start_value = _select_own_values(root, start_value)
        own_type_pending = False

    # Greedy: biggest unconstrained first, as long as it fits.
    remaining: list[InstanceTypeTree] = []
    for uc in unconstrained_children:
      if uc.num_values + start_value <= constrained.start:
        start_value = solve_constraints(uc, start_value, root.children)
      else:
        remaining.append(uc)
    unconstrained_children = remaining

    start_value = solve_constraints(constrained, start_value, root.children)

  if own_type_pending:
    start_value = _select_own_values(root, start_value)
    own_type_pending = False

  for uc in unconstrained_children:
    start_value = solve_constraints(uc, start_value, root.children)

  if highest_child is not None:
    start_value = solve_constraints(highest_child, start_value, root.children)

  root.end = start_value - 1
  root.start = start_value if not root.children else root.children[0].start
  if root.value != -1 and root.value < root.start:
    root.start = root.value
  root.num_values = root.end - root.start + 1

  if root.num_values > 0:
    destination.append(root)
  return start_value


def assign_instance_types(classes: list[ClassInfo]) -> InstanceTypeTree | None:
  """Top-level entry point. Matches torque's AssignInstanceTypes."""
  root = build_instance_type_tree(classes)
  if root is None:
    return None
  propagate_constraints(root)
  destination: list[InstanceTypeTree] = []
  solve_constraints(root, 0, destination)
  return destination[0] if destination else None


# ---------------------------------------------------------------------------
# Output emission (matches src/torque/instance-type-generator.cc
# PrintInstanceTypes + GenerateInstanceTypes).
# ---------------------------------------------------------------------------


def capify_with_underscores(s: str) -> str:
  """Port of src/torque/utils.cc CapifyStringWithUnderscores."""
  result: list[str] = []
  js_position = s.find("JS")
  previous_was_lower_or_digit = False
  for index, current in enumerate(s):
    if ((previous_was_lower_or_digit and current.isupper()) or
        (js_position != -1 and index == js_position + 2)):
      result.append("_")
    if current in (".", "-"):
      result.append("_")
      previous_was_lower_or_digit = False
      continue
    result.append(current.upper())
    previous_was_lower_or_digit = current.islower() or current.isdigit()
  return "".join(result)


# Emission carries NO source-position comments (Torque's emission has
# them): instance-types.h sits in the near-global include set, and
# position-bearing comments would change its contents (and thus rebuild
# the world, past the write-if-changed check) on every line shift in
# any harvested header.


@dataclasses.dataclass
class _EnumEmission:
  assigned_types: str = ""  # TORQUE_ASSIGNED_INSTANCE_TYPES body
  values_list: str = ""  # TORQUE_ASSIGNED_INSTANCE_TYPE_LIST body


def _emit_enum_walk(
    root: InstanceTypeTree,
    indent: str,
    out: _EnumEmission,
) -> None:
  type_name = capify_with_underscores(root.cls.name) + "_TYPE"
  inner_indent = indent

  # Every type gets FIRST_/LAST_ range markers, aliasing the sole value
  # for single-instance-type classes. Generated CSA references the range
  # markers unconditionally (Torque's GetClassInstanceTypeRange), so the
  # single-vs-range decision in DownCastForTorqueClass is made against
  # this header's values at C++ compile time instead of being baked in
  # at Torque codegen time from Torque's own (possibly divergent) tree.
  out.assigned_types += f"{indent}V(FIRST_{type_name}, {root.start}) \\\n"
  if root.num_values > 1:
    inner_indent += "  "
  if root.num_own_values == 1:
    out.assigned_types += f"{inner_indent}V({type_name}, {root.value}) \\\n"
    out.values_list += f"  V({type_name}) \\\n"

  for child in root.children:
    _emit_enum_walk(child, inner_indent, out)

  # LAST_* marker is omitted for flag types (num_own_values > 1 means
  # @reserveBitsInInstanceType(N) is active).
  if root.num_own_values <= 1:
    out.assigned_types += f"{indent}V(LAST_{type_name}, {root.end}) \\\n"


def emit_enum_macros(root: InstanceTypeTree | None) -> str:
  """Emit the IT enum macro definitions
  (TORQUE_ASSIGNED_INSTANCE_TYPES, TORQUE_ASSIGNED_INSTANCE_TYPE_LIST)
  as a single string ending with a trailing blank line.
  """
  e = _EnumEmission()
  if root is not None:
    _emit_enum_walk(root, "  ", e)

  lines: list[str] = []
  lines.append("// Instance types for all classes except for those that use "
               "InstanceType as flags.")
  lines.append("#define TORQUE_ASSIGNED_INSTANCE_TYPES(V) \\")
  lines.append(e.assigned_types.rstrip("\n"))
  lines.append("")

  lines.append("// Instance types for all classes except for those that use")
  lines.append("// InstanceType as flags.")
  lines.append("#define TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(V) \\")
  lines.append(e.values_list.rstrip("\n"))
  lines.append("")
  return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Bucket emission for auto-generated instance-type consumers.
#
# Bucket placement signal: SINGLE vs MULTIPLE vs RANGE, derived from
# the tree's `num_values` and `num_own_values`:
#   - num_own_values == 1 and num_values == 1: SINGLE
#   - num_own_values == 1 and num_values > 1: MULTIPLE
#   - num_own_values == 0 and num_values > 1: RANGE
# ---------------------------------------------------------------------------


@dataclasses.dataclass
class _BucketsEmission:
  single: str = ""
  multiple: str = ""
  range_: str = ""


def _emit_buckets_walk(
    root: InstanceTypeTree,
    out: _BucketsEmission,
    is_root: bool = False,
) -> None:
  type_name = capify_with_underscores(root.cls.name) + "_TYPE"

  if root.num_own_values == 1 and not root.cls.no_auto_checker:
    line = f"  V({root.cls.name}, {type_name}) \\\n"
    if root.num_values == 1:
      out.single += line
    else:
      out.multiple += line

  for child in root.children:
    _emit_buckets_walk(child, out)

  if root.num_values > 1 and not root.cls.no_auto_checker:
    # Matches torque's check: "if root->type->GetSuperClass() != nullptr".
    # We approximate GetSuperClass() returning non-null with "not the
    # tree root" (the tree root's GetSuperClass is nullptr). Emitted
    # for flag classes (num_own_values > 1) too -- the range macro
    # gets a {FIRST,LAST}_<NAME>_TYPE pair even though the assigned-
    # types macro omits the explicit LAST_<NAME>_TYPE in that case.
    if not is_root:
      out.range_ += (f"  V({root.cls.name}, FIRST_{type_name}, "
                     f"LAST_{type_name}) \\\n")


def emit_bucket_macros(root: InstanceTypeTree | None) -> str:
  """Emit the bucket macro definitions as a single string ending
  with a trailing blank line."""
  e = _BucketsEmission()
  if root is not None:
    _emit_buckets_walk(root, e, is_root=True)

  lines: list[str] = []

  def block(label_comment: list[str], macro: str, body: str) -> None:
    for lc in label_comment:
      lines.append(lc)
    lines.append(f"#define {macro}(V) \\")
    lines.append(body.rstrip("\n"))
    lines.append("")

  block([
      "// Pairs of (ClassName, INSTANCE_TYPE) for classes whose instance",
      "// type is unique to them (no subclasses share it)."
  ], "INSTANCE_TYPE_LIST_SINGLE", e.single)
  block([
      "// Pairs of (ClassName, INSTANCE_TYPE) for classes that have their",
      "// own instance type and whose subclasses share that instance type",
      "// (e.g. JSObject owns JS_OBJECT_TYPE; concrete leaves with the",
      "// same IT are entered via INSTANCE_TYPE_LIST_SINGLE)."
  ], "INSTANCE_TYPE_LIST_MULTIPLE", e.multiple)
  block([
      "// Triples of (ClassName, FIRST_TYPE, LAST_TYPE) for classes that",
      "// span a contiguous range of instance types (typically abstract",
      "// intermediates whose concrete subclasses get their own ITs)."
  ], "INSTANCE_TYPE_LIST_RANGE", e.range_)

  return "\n".join(lines) + "\n"


def _emit_dispatch_walk(root: InstanceTypeTree, out: list[str]) -> None:
  if (root.num_own_values == 1 and root.cls.has_printer and
      root.cls.has_verifier and not root.cls.no_auto_dispatch):
    type_name = capify_with_underscores(root.cls.name) + "_TYPE"
    out.append(f"  V({root.cls.name}, {type_name}) \\\n")
  for child in root.children:
    _emit_dispatch_walk(child, out)


def emit_dispatch_macro(root: InstanceTypeTree | None) -> str:
  """Emit HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST: pairs of
  (ClassName, INSTANCE_TYPE) for the switches in
  HeapObject::HeapObjectPrint (objects-printer.cc) and
  HeapObject::HeapObjectVerify (objects-debug.cc).

  A class is included iff it has its own instance-type value and
  declares both Name##Print(std::ostream&) and Name##Verify(Isolate*)
  (typically via DECL_PRINTER / DECL_VERIFIER), unless it carries
  V8_IT_NO_AUTO_DISPATCH -- those classes are dispatched by
  hand-written cases in the two switches, and a generated entry would
  produce a duplicate case label.
  """
  entries: list[str] = []
  if root is not None:
    _emit_dispatch_walk(root, entries)

  lines: list[str] = []
  lines.append("// Pairs of (ClassName, INSTANCE_TYPE) for the HeapObject")
  lines.append("// diagnostic dispatch: the switches in "
               "HeapObject::HeapObjectPrint")
  lines.append("// (objects-printer.cc) and HeapObject::HeapObjectVerify")
  lines.append("// (objects-debug.cc). Included iff the class has its own")
  lines.append("// instance-type value, declares both Name##Print and")
  lines.append("// Name##Verify, and does not carry V8_IT_NO_AUTO_DISPATCH")
  lines.append("// (hand-written dispatch cases).")
  lines.append("#define HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST(V) \\")
  lines.append("".join(entries).rstrip("\n"))
  lines.append("")
  return "\n".join(lines) + "\n"
