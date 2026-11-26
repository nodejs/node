# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Static concurrency-hazard and MERGE_MODEL_STEP verification on Clang ASTs.

Enforces four invariants on ``BeginMergeInBackground`` and
``CompleteMergeInForeground``:

1. **Exact ordered marker contract**: The sequence of ``MERGE_MODEL_STEP*``
   markers in lexical order must match ``EXPECTED_BG_STEPS`` and
   ``EXPECTED_FG_STEPS`` exactly (rejecting missing, extra, duplicated, or
   swapped markers).
2. **Strict control-flow precedence (marker BEFORE hazard)**: Every shared-heap
   concurrency hazard (detected via general receiver/accessor taint tracking,
   not a narrow variable allowlist) must be preceded in the same control-flow
   path by a ``MERGE_MODEL_STEP*`` marker. Markers never cover statements
   preceding them, never leak across sibling blocks/branches/loops, and
   coverage is checked across ``Block``, ``If`` (including ``init``,
   ``cond_var``, ``cond``), ``For``, ``RangeFor``, ``While``, ``DoWhile``, and
   ``Switch``.
3. **No safepoint markers inside ``DisallowGarbageCollection``**: Any
   ``MERGE_MODEL_STEP_WITH_SAFEPOINT`` inside a lexical block with an active
   ``DisallowGarbageCollection`` RAII variable is rejected statically.
4. **Test-only macro hygiene**: Raw ``src/codegen/compiler.cc`` must define
   ``MERGE_MODEL_STEP`` via ``SYNCHRONIZATION_POINT_TEST_ONLY`` and
   ``MERGE_MODEL_STEP_WITH_SAFEPOINT`` via
   ``SYNCHRONIZATION_POINT_WITH_SAFEPOINT_TEST_ONLY`` so production release
   builds pay zero overhead.
"""

from __future__ import annotations

import re
from typing import Any, Dict, FrozenSet, List, Set, Tuple

from clang_frontend import ParsedCxxBundle
from errors import (
    ConcurrencySyncMismatchError,
    EXPECTED_BG_STEPS,
    EXPECTED_FG_STEPS,
)

# Initial symbols that refer to shared (cross-thread / cached-script) state or
# merge-task worklists holding pointers into shared state.
_INITIAL_SHARED_TAINT: FrozenSet[str] = frozenset({
    "old_script",
    "cached_script_",
    "old_script_",
    "forwarder",
    "toplevel_sfi_from_cached_script_",
    "new_compiled_data_for_cached_sfis_",
    "used_new_sfis_",
    "used_new_scope_infos_",
})

# Methods on shared containers/handles that are pure metadata or local helpers
# (not shared-heap synchronization hazards). Every OTHER method call on a
# tainted receiver is treated as hazardous by default.
_BENIGN_METHODS: FrozenSet[str] = frozenset({
    "infos",
    "ulength",
    "value",
    "ToHandleChecked",
    "HasAnythingToForward",
    "HasScopeInfoToUpdate",
    "rbegin",
    "rend",
    "begin",
    "end",
    "empty",
    "size",
    "is_null",
    "IsWeak",
    "IsSmi",
    "IsCleared",
    "GetHeapObjectAssumeWeak",
    "GetHeapObjectIfWeak",
    "function_literal_id",
    "HasDebugInfo",
    "NeedsSourcePositions",
})

# Shared-heap publish operations that MUST be immediately preceded by a
# dedicated MERGE_MODEL_STEP (cannot piggyback on an enclosing loop marker or
# share a single marker with a second publish).
_IMMEDIATE_PUBLISH_METHODS: FrozenSet[str] = frozenset({
    "CopyFrom",
    "set",
})

# Pre-existing upstream non-TEST_ONLY synchronization point in
# src/objects/shared-function-info-inl.h (commit 7fd662da8a5f); every other
# sync point (and even a different macro name such as *_WITH_SAFEPOINT) in the
# parsed merge files must be *_TEST_ONLY.
_UPSTREAM_NON_TEST_ONLY_SYNC_POINTS: FrozenSet[Tuple[
    str, str, str]] = frozenset({
        (
            "src/objects/shared-function-info-inl.h",
            "SYNCHRONIZATION_POINT",
            "BeforeGetOuterScopeInfo",
        ),
    })


def extract_ordered_markers(stmts: List[Dict[str, Any]]) -> List[str]:
  """Returns all ``MergeModelStep`` names in exact lexical/control-flow order."""
  found: List[str] = []

  def _walk(node: Any) -> None:
    if isinstance(node, list):
      for item in node:
        _walk(item)
      return
    if not isinstance(node, dict):
      return
    t = node.get("type")
    if t == "MergeModelStep":
      found.append(node["step"])
      return
    if t == "Block":
      _walk(node.get("body", []))
    elif t == "If":
      _walk(node.get("init"))
      _walk(node.get("cond_var"))
      _walk(node.get("body"))
      _walk(node.get("orelse"))
    elif t == "For":
      _walk(node.get("init"))
      _walk(node.get("cond_var"))
      _walk(node.get("body"))
    elif t in ("RangeFor", "While", "DoWhile"):
      _walk(node.get("cond_var"))
      _walk(node.get("body"))
    elif t == "Switch":
      _walk(node.get("init"))
      _walk(node.get("cond_var"))
      _walk(node.get("items", []))

  _walk(stmts)
  return found


def _stmt_references_tainted(stmt: Any, tainted: Set[str]) -> bool:
  if isinstance(stmt, list):
    return any(_stmt_references_tainted(s, tainted) for s in stmt)
  if not isinstance(stmt, dict):
    return False
  t = stmt.get("type")
  if t == "Decl":
    return _expr_references_tainted(stmt.get("value"), tainted)
  if t == "ExprStmt":
    return _expr_references_tainted(stmt.get("expr"), tainted)
  if t == "Return":
    return _expr_references_tainted(stmt.get("value"), tainted)
  if t == "Block":
    return _stmt_references_tainted(stmt.get("body", []), tainted)
  if t == "If":
    return (_stmt_references_tainted(stmt.get("init"), tainted) or
            _stmt_references_tainted(stmt.get("cond_var"), tainted) or
            _expr_references_tainted(stmt.get("cond"), tainted) or
            _stmt_references_tainted(stmt.get("body"), tainted) or
            _stmt_references_tainted(stmt.get("orelse"), tainted))
  if t == "For":
    return (_stmt_references_tainted(stmt.get("init"), tainted) or
            _stmt_references_tainted(stmt.get("cond_var"), tainted) or
            _expr_references_tainted(stmt.get("cond"), tainted) or
            _expr_references_tainted(stmt.get("step"), tainted) or
            _stmt_references_tainted(stmt.get("body"), tainted))
  if t == "RangeFor":
    return (_expr_references_tainted(stmt.get("iterable"), tainted) or
            _stmt_references_tainted(stmt.get("body"), tainted))
  if t in ("While", "DoWhile"):
    return (_stmt_references_tainted(stmt.get("cond_var"), tainted) or
            _expr_references_tainted(stmt.get("cond"), tainted) or
            _stmt_references_tainted(stmt.get("body"), tainted))
  if t == "Switch":
    return (_stmt_references_tainted(stmt.get("init"), tainted) or
            _stmt_references_tainted(stmt.get("cond_var"), tainted) or
            _expr_references_tainted(stmt.get("cond"), tainted) or
            _stmt_references_tainted(stmt.get("items", []), tainted))
  return False


def _expr_references_tainted(node: Any, tainted: Set[str]) -> bool:
  if not isinstance(node, dict):
    return False
  t = node.get("type")
  if t == "Name":
    return node.get("name") in tainted
  if t == "Member":
    if node.get("is_this_member") and node.get("member") in tainted:
      return True
    if node.get("member") in ("cached_sfi", "old_script_"):
      return True
    return _expr_references_tainted(node.get("base"), tainted)
  if t == "Call":
    callee = node.get("callee")
    if _expr_references_tainted(callee, tainted):
      return True
    return any(
        _expr_references_tainted(a, tainted) for a in node.get("args", []))
  if t == "TemplateCall":
    return any(
        _expr_references_tainted(a, tainted) for a in node.get("args", []))
  if t == "UnaryOp":
    return _expr_references_tainted(node.get("operand"), tainted)
  if t == "BinOp":
    return (_expr_references_tainted(node.get("left"), tainted) or
            _expr_references_tainted(node.get("right"), tainted))
  if t == "Assign":
    return (_expr_references_tainted(node.get("target"), tainted) or
            _expr_references_tainted(node.get("value"), tainted))
  if t == "Subscript":
    return (_expr_references_tainted(node.get("base"), tainted) or
            _expr_references_tainted(node.get("index"), tainted))
  if t == "Ternary":
    return (_expr_references_tainted(node.get("cond"), tainted) or
            _expr_references_tainted(node.get("then"), tainted) or
            _expr_references_tainted(node.get("else"), tainted))
  if t == "CastExpr":
    return _expr_references_tainted(node.get("expr"), tainted)
  if t == "Construct":
    return any(
        _expr_references_tainted(a, tainted) for a in node.get("args", []))
  if t == "InitList":
    return any(
        _expr_references_tainted(v, tainted) for v in node.get("values", []))
  if t == "Lambda":
    for cap in node.get("captures", []):
      if cap.get("name") in tainted or _expr_references_tainted(
          cap.get("init"), tainted):
        return True
    return _stmt_references_tainted(node.get("body", []), tainted)
  return False


def _collect_outparam_taints(node: Any, tainted: Set[str]) -> Set[str]:
  """Collects variable names tainted via ``&out`` parameters or inline assigns."""
  found: Set[str] = set()
  if not isinstance(node, dict):
    return found
  t = node.get("type")
  if t in ("Call", "TemplateCall"):
    callee = node.get("callee")
    args = node.get("args", [])
    out_vars: List[str] = []
    has_tainted_input = t == "Call" and _expr_references_tainted(
        callee, tainted)
    for a in args:
      if (isinstance(a, dict) and a.get("type") == "UnaryOp" and
          a.get("op") == "&" and isinstance(a.get("operand"), dict) and
          a["operand"].get("type") == "Name"):
        out_vars.append(a["operand"]["name"])
      elif _expr_references_tainted(a, tainted):
        has_tainted_input = True
    if has_tainted_input and out_vars:
      found.update(out_vars)
    if t == "Call":
      found.update(_collect_outparam_taints(callee, tainted | found))
    for a in args:
      found.update(_collect_outparam_taints(a, tainted | found))
    return found
  if t == "Assign":
    found.update(_collect_outparam_taints(node.get("value"), tainted))
    tgt = node.get("target")
    if (isinstance(tgt, dict) and tgt.get("type") == "Name" and
        _expr_references_tainted(node.get("value"), tainted | found)):
      found.add(tgt["name"])
    return found
  if t == "UnaryOp":
    return _collect_outparam_taints(node.get("operand"), tainted)
  if t == "BinOp":
    left_t = _collect_outparam_taints(node.get("left"), tainted)
    return left_t | _collect_outparam_taints(
        node.get("right"), tainted | left_t)
  if t == "Subscript":
    base_t = _collect_outparam_taints(node.get("base"), tainted)
    return base_t | _collect_outparam_taints(
        node.get("index"), tainted | base_t)
  if t == "Member":
    return _collect_outparam_taints(node.get("base"), tainted)
  if t == "Ternary":
    c_t = _collect_outparam_taints(node.get("cond"), tainted)
    return (c_t
            | _collect_outparam_taints(node.get("then"), tainted | c_t)
            | _collect_outparam_taints(node.get("else"), tainted | c_t))
  if t == "CastExpr":
    return _collect_outparam_taints(node.get("expr"), tainted)
  if t == "Construct":
    for a in node.get("args", []):
      found.update(_collect_outparam_taints(a, tainted | found))
    return found
  if t == "InitList":
    for v in node.get("values", []):
      found.update(_collect_outparam_taints(v, tainted | found))
    return found
  return found


def _compute_impure_forwarder_methods(
    fwd_methods: Dict[str, List[Any]]) -> FrozenSet[str]:
  """Interprocedurally finds ``ConstantPoolPointerForwarder`` methods touching ``old_script_``.

  Prevents any method listed in ``_BENIGN_METHODS`` (such as
  ``HasAnythingToForward`` or ``HasScopeInfoToUpdate``) from bypassing static
  marker-before-hazard checks if its body or transitive ``this->`` callees read
  or mutate ``old_script_``.
  """
  if not fwd_methods:
    return frozenset()

  direct_impure: Set[str] = set()
  callees: Dict[str, Set[str]] = {name: set() for name in fwd_methods}

  def _walk_callees(node: Any, out: Set[str]) -> None:
    if isinstance(node, list):
      for item in node:
        _walk_callees(item, out)
      return
    if not isinstance(node, dict):
      return
    if node.get("type") == "Call":
      c = node.get("callee")
      if isinstance(
          c, dict) and c.get("type") == "Member" and c.get("is_this_member"):
        mname = c.get("member", "")
        if mname in fwd_methods:
          out.add(mname)
    for v in node.values():
      if isinstance(v, (dict, list)):
        _walk_callees(v, out)

  for mname, overloads in fwd_methods.items():
    for pm in overloads:
      if _stmt_references_tainted(pm.body, {"old_script_"}):
        direct_impure.add(mname)
      _walk_callees(pm.body, callees[mname])

  impure = set(direct_impure)
  changed = True
  while changed:
    changed = False
    for mname, cs in callees.items():
      if mname not in impure and any(c in impure for c in cs):
        impure.add(mname)
        changed = True
  return frozenset(impure)


def _collect_stmt_hazards_in_lambda(
    stmts: Any,
    tainted: Set[str],
    benign_methods: FrozenSet[str] = _BENIGN_METHODS,
) -> List[Tuple[str, str]]:
  """Recursively collects all shared-heap hazards inside a lambda body."""
  out: List[Tuple[str, str]] = []
  local_taint = set(tainted)
  seq = stmts if isinstance(stmts, list) else [stmts]
  for stmt in seq:
    if not isinstance(stmt, dict):
      continue
    st = stmt.get("type")
    if st == "Decl":
      val = stmt.get("value")
      out.extend(_expr_hazards(val, local_taint, benign_methods))
      local_taint.update(_collect_outparam_taints(val, local_taint))
      if _expr_references_tainted(val, local_taint):
        local_taint.add(stmt["name"])
    elif st == "ExprStmt":
      expr = stmt.get("expr")
      out.extend(_expr_hazards(expr, local_taint, benign_methods))
      local_taint.update(_collect_outparam_taints(expr, local_taint))
    elif st == "Return":
      out.extend(_expr_hazards(stmt.get("value"), local_taint, benign_methods))
    elif st == "Block":
      out.extend(
          _collect_stmt_hazards_in_lambda(
              stmt.get("body", []), local_taint, benign_methods))
    elif st == "If":
      bt = set(local_taint)
      for part in (stmt.get("init"), stmt.get("cond_var")):
        if part is not None:
          out.extend(
              _collect_stmt_hazards_in_lambda([part], bt, benign_methods))
          if isinstance(part, dict) and part.get("type") == "Decl":
            bt.update(_collect_outparam_taints(part.get("value"), bt))
            if _expr_references_tainted(part.get("value"), bt):
              bt.add(part["name"])
      out.extend(_expr_hazards(stmt.get("cond"), bt, benign_methods))
      bt.update(_collect_outparam_taints(stmt.get("cond"), bt))
      out.extend(
          _collect_stmt_hazards_in_lambda(stmt.get("body"), bt, benign_methods))
      out.extend(
          _collect_stmt_hazards_in_lambda(
              stmt.get("orelse"), bt, benign_methods))
    elif st in ("For", "RangeFor", "While", "DoWhile", "Switch"):
      for k in ("init", "cond_var"):
        if stmt.get(k) is not None:
          out.extend(
              _collect_stmt_hazards_in_lambda(
                  stmt.get(k), local_taint, benign_methods))
      for k in ("cond", "step", "iterable"):
        if stmt.get(k) is not None:
          out.extend(_expr_hazards(stmt.get(k), local_taint, benign_methods))
      if st == "RangeFor" and _expr_references_tainted(
          stmt.get("iterable"), local_taint):
        local_taint.add(stmt["var"])
      out.extend(
          _collect_stmt_hazards_in_lambda(
              stmt.get("body"), local_taint, benign_methods))
      if st == "Switch":
        out.extend(
            _collect_stmt_hazards_in_lambda(
                stmt.get("items", []), local_taint, benign_methods))
  return out


def _expr_hazards(
    node: Any,
    tainted: Set[str],
    benign_methods: FrozenSet[str] = _BENIGN_METHODS,
) -> List[Tuple[str, str]]:
  """Returns ``(description, hazard_kind)`` pairs for hazards.

  ``hazard_kind`` is one of:
    * ``"immediate_publish"``: ``CopyFrom`` or ``old_script->infos()->set``
    * ``"shared_get"``: ``old_script->infos()->get``
    * ``"normal"``: any other method call on a tainted receiver / shared store
  """
  if not isinstance(node, dict):
    return []
  t = node.get("type")

  if t == "Call":
    callee = node.get("callee")
    args = node.get("args", [])
    hazards: List[Tuple[str, str]] = []
    if isinstance(callee, dict) and callee.get("type") == "Member":
      mname = callee.get("member", "")
      base = callee.get("base")
      is_this = bool(callee.get("is_this_member", False))
      if mname not in benign_methods:
        recv_tainted = (
            is_this or _expr_references_tainted(base, tainted) or
            (mname == "set_script" and
             any(_expr_references_tainted(a, tainted) for a in args)))
        # Skip `new_script->infos()->get/set` (thread-local deserialized script).
        if mname in ("get", "set") and _is_rooted_at_name(base, "new_script"):
          recv_tainted = False
        if recv_tainted:
          if mname in _IMMEDIATE_PUBLISH_METHODS:
            hkind = "immediate_publish"
          elif mname == "get":
            hkind = "shared_get"
          else:
            hkind = "normal"
          hazards.append((f"shared-heap call .{mname}()", hkind))
      hazards.extend(_expr_hazards(base, tainted, benign_methods))
    else:
      hazards.extend(_expr_hazards(callee, tainted, benign_methods))
    for a in args:
      hazards.extend(_expr_hazards(a, tainted, benign_methods))
    return hazards

  if t == "Assign":
    hazards = _expr_hazards(node.get("value"), tainted, benign_methods)
    tgt = node.get("target")
    if isinstance(tgt, dict) and tgt.get("type") == "Member":
      if tgt.get("is_this_member") and tgt.get("member") in (
          "toplevel_sfi_from_cached_script_",):
        hazards.append((f"shared-heap store this->{tgt['member']}", "normal"))
    hazards.extend(_expr_hazards(tgt, tainted, benign_methods))
    return hazards

  if t == "TemplateCall":
    out: List[Tuple[str, str]] = []
    for a in node.get("args", []):
      out.extend(_expr_hazards(a, tainted, benign_methods))
    return out
  if t == "UnaryOp":
    return _expr_hazards(node.get("operand"), tainted, benign_methods)
  if t == "BinOp":
    return _expr_hazards(node.get("left"), tainted,
                         benign_methods) + _expr_hazards(
                             node.get("right"), tainted, benign_methods)
  if t == "Subscript":
    return _expr_hazards(node.get("base"), tainted,
                         benign_methods) + _expr_hazards(
                             node.get("index"), tainted, benign_methods)
  if t == "Ternary":
    return (_expr_hazards(node.get("cond"), tainted, benign_methods) +
            _expr_hazards(node.get("then"), tainted, benign_methods) +
            _expr_hazards(node.get("else"), tainted, benign_methods))
  if t == "CastExpr":
    return _expr_hazards(node.get("expr"), tainted, benign_methods)
  if t == "Construct":
    out = []
    for a in node.get("args", []):
      out.extend(_expr_hazards(a, tainted, benign_methods))
    return out
  if t == "InitList":
    out = []
    for v in node.get("values", []):
      out.extend(_expr_hazards(v, tainted, benign_methods))
    return out
  if t == "Lambda":
    lam_taint = set(tainted)
    out = []
    for cap in node.get("captures", []):
      init = cap.get("init")
      if init is not None:
        out.extend(_expr_hazards(init, lam_taint, benign_methods))
        if _expr_references_tainted(init, lam_taint) and cap.get("name"):
          lam_taint.add(cap["name"])
    out.extend(
        _collect_stmt_hazards_in_lambda(
            node.get("body", []), lam_taint, benign_methods))
    return out
  return []


def _is_old_script_infos_get(node: Any) -> bool:
  """True iff ``node`` is strictly ``old_script->infos()->get(...)``."""
  if not isinstance(node, dict) or node.get("type") != "Call":
    return False
  callee = node.get("callee")
  if not (isinstance(callee, dict) and callee.get("type") == "Member" and
          callee.get("member") == "get"):
    return False
  return _is_rooted_at_name(callee.get("base"), "old_script")


def _is_rooted_at_name(node: Any, root_name: str) -> bool:
  while isinstance(node, dict):
    t = node.get("type")
    if t == "Name":
      return node.get("name") == root_name
    if t == "Member":
      node = node.get("base")
    elif t == "Call":
      node = node.get("callee")
    elif t == "UnaryOp":
      node = node.get("operand")
    else:
      return False
  return False


def _is_disallow_gc_decl(stmt: Dict[str, Any]) -> bool:
  if stmt.get("type") != "Decl":
    return False
  qt = stmt.get("qual_type", "")
  if "DisallowGarbageCollection" in qt or "DisallowSafepoints" in qt:
    return True
  val = stmt.get("value")
  if isinstance(val, dict) and val.get("type") == "Construct":
    ct = val.get("ctor_type", "")
    return "DisallowGarbageCollection" in ct or "DisallowSafepoints" in ct
  return False


def verify_hazard_markers(
    stmts: List[Dict[str, Any]],
    method_name: str,
    *,
    impure_methods: FrozenSet[str] = frozenset(),
) -> List[str]:
  """Verifies strict marker-before-hazard precedence & safepoint/no_gc safety.

  Returns a list of violation descriptions (empty list means clean).
  """
  benign_methods = _BENIGN_METHODS - impure_methods
  violations: List[str] = []
  seen_steps: Set[str] = set()

  def _report_hazards(
      hz_list: List[Tuple[str, str]],
      is_covered: bool,
      is_just_marked: bool,
      get_budget: int,
      loc_desc: str,
  ) -> int:
    if not hz_list:
      return get_budget
    unmarked = [desc for desc, _kind in hz_list if not is_covered]
    if unmarked:
      violations.append(
          f"{method_name} ({loc_desc}): "
          f"unmarked hazard(s) {unmarked} without preceding MERGE_MODEL_STEP")
      return get_budget
    unmarked_pub = [
        desc for desc, kind in hz_list
        if kind == "immediate_publish" and not is_just_marked
    ]
    if unmarked_pub:
      violations.append(
          f"{method_name} ({loc_desc}): "
          f"shared-heap publish hazard(s) {unmarked_pub} must be immediately "
          f"preceded by a dedicated MERGE_MODEL_STEP")
    shared_gets = [desc for desc, kind in hz_list if kind == "shared_get"]
    if shared_gets:
      if len(shared_gets) > get_budget:
        violations.append(
            f"{method_name} ({loc_desc}): "
            f"unmarked shared-heap slot read(s) {shared_gets} without a "
            f"dedicated preceding MERGE_MODEL_STEP")
        return 0
      return get_budget - len(shared_gets)
    return get_budget

  def _check_seq(
      seq: List[Dict[str, Any]],
      tainted: Set[str],
      covered: bool,
      in_no_gc: bool,
      shared_get_budget: int,
      path: str,
  ) -> None:
    local_taint = set(tainted)
    local_covered = covered
    local_no_gc = in_no_gc
    local_get_budget = shared_get_budget
    # Track whether the immediately preceding statement was a MergeModelStep so
    # a marker placed right before a `{ ... }` block, `for (...)` loop, or
    # shared-heap publish (`CopyFrom` / `old_script->infos()->set`) covers it
    # directly, while NEVER leaking across subsequent sibling blocks, loops, or
    # second publishes!
    just_marked = False

    for idx, stmt in enumerate(seq):
      if not isinstance(stmt, dict):
        continue
      stype = stmt.get("type")

      if stype == "MergeModelStep":
        step_name = stmt.get("step", "")
        seen_steps.add(step_name)
        if stmt.get("allow_safepoint", False) and local_no_gc:
          violations.append(
              f"{method_name} ({path}[{idx}]): MERGE_MODEL_STEP_WITH_SAFEPOINT"
              f"({step_name}) is inside a DisallowGarbageCollection scope")
        local_covered = True
        local_get_budget = 1
        just_marked = True
        continue

      if stype == "SyncPoint":
        if stmt.get("allow_safepoint", False) and local_no_gc:
          violations.append(
              f"{method_name} ({path}[{idx}]): SYNCHRONIZATION_POINT_WITH_SAFEPOINT"
              f"({stmt.get('name', '')}) is inside DisallowGarbageCollection")
        continue

      if stype == "Decl":
        if _is_disallow_gc_decl(stmt):
          local_no_gc = True
        val = stmt.get("value")
        # Ignore ONLY the post-merge final `maybe_toplevel_sfi =
        # old_script->infos()->get(kFunctionLiteralIdTopLevel)` at top level
        # after FG_RELEASE_BACKWARDS_START in CompleteMergeInForeground.
        is_fg_final_toplevel = (
            method_name == "CompleteMergeInForeground" and path == "root" and
            stmt.get("name") == "maybe_toplevel_sfi" and
            "FG_RELEASE_BACKWARDS_START" in seen_steps and
            _is_old_script_infos_get(val))
        if not is_fg_final_toplevel:
          local_get_budget = _report_hazards(
              _expr_hazards(val, local_taint, benign_methods),
              local_covered,
              just_marked,
              local_get_budget,
              f"{path}[{idx}] Decl {stmt.get('name')}",
          )
        local_taint.update(_collect_outparam_taints(val, local_taint))
        if _expr_references_tainted(val, local_taint):
          local_taint.add(stmt["name"])
        just_marked = False
        continue

      if stype == "ExprStmt":
        expr = stmt.get("expr")
        local_get_budget = _report_hazards(
            _expr_hazards(expr, local_taint, benign_methods),
            local_covered,
            just_marked,
            local_get_budget,
            f"{path}[{idx}] ExprStmt",
        )
        local_taint.update(_collect_outparam_taints(expr, local_taint))
        just_marked = False
        continue

      if stype == "Return":
        local_get_budget = _report_hazards(
            _expr_hazards(stmt.get("value"), local_taint, benign_methods),
            local_covered,
            just_marked,
            local_get_budget,
            f"{path}[{idx}] Return",
        )
        just_marked = False
        continue

      if stype == "Block":
        _check_seq(
            stmt.get("body", []),
            local_taint,
            local_covered,
            local_no_gc,
            1 if just_marked else 0,
            f"{path}[{idx}].block",
        )
        # Never allow a marker from before a `{ ... }` block to leak past it
        # into subsequent sibling blocks/statements (even if benign decls
        # intervened between the marker and the block).
        local_covered = False
        local_get_budget = 0
        just_marked = False
        continue

      if stype == "If":
        branch_taint = set(local_taint)
        init_s = stmt.get("init")
        if init_s is not None:
          _check_seq(
              [init_s],
              branch_taint,
              local_covered,
              local_no_gc,
              0,
              f"{path}[{idx}].if.init",
          )
          if isinstance(init_s, dict) and init_s.get("type") == "Decl":
            branch_taint.update(
                _collect_outparam_taints(init_s.get("value"), branch_taint))
            if _expr_references_tainted(init_s.get("value"), branch_taint):
              branch_taint.add(init_s["name"])
        cond_var = stmt.get("cond_var")
        if cond_var is not None:
          _check_seq(
              [cond_var],
              branch_taint,
              local_covered,
              local_no_gc,
              0,
              f"{path}[{idx}].if.cond_var",
          )
          if isinstance(cond_var, dict) and cond_var.get("type") == "Decl":
            branch_taint.update(
                _collect_outparam_taints(cond_var.get("value"), branch_taint))
            if _expr_references_tainted(cond_var.get("value"), branch_taint):
              branch_taint.add(cond_var["name"])
        cond_expr = stmt.get("cond")
        local_get_budget = _report_hazards(
            _expr_hazards(cond_expr, branch_taint, benign_methods),
            local_covered,
            just_marked,
            local_get_budget,
            f"{path}[{idx}] If.cond",
        )
        cond_out_taints = _collect_outparam_taints(cond_expr, branch_taint)
        branch_taint.update(cond_out_taints)
        local_taint.update(cond_out_taints)
        then_body = stmt.get("body")
        if then_body is not None:
          body_list = then_body["body"] if then_body.get(
              "type") == "Block" else [then_body]
          _check_seq(body_list, branch_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].then")
        orelse = stmt.get("orelse")
        if orelse is not None:
          else_list = orelse["body"] if orelse.get("type") == "Block" else [
              orelse
          ]
          _check_seq(else_list, branch_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].else")
        # Always reset coverage after a top-level `If` or a directly-marked
        # `If` so markers never leak from one phase to the next.
        if just_marked or path == "root":
          local_covered = False
          local_get_budget = 0
        just_marked = False
        continue

      if stype == "For":
        loop_taint = set(local_taint)
        loop_covered = just_marked or local_covered
        loop_get_budget = 1 if just_marked else 0
        init_s = stmt.get("init")
        if init_s is not None:
          init_list = init_s if isinstance(init_s, list) else [init_s]
          _check_seq(init_list, loop_taint, loop_covered, local_no_gc, 0,
                     f"{path}[{idx}].for.init")
          for is_ in init_list:
            if isinstance(is_, dict) and is_.get("type") == "Decl":
              loop_taint.update(
                  _collect_outparam_taints(is_.get("value"), loop_taint))
              if _expr_references_tainted(is_.get("value"), loop_taint):
                loop_taint.add(is_["name"])
        cond_var = stmt.get("cond_var")
        if cond_var is not None:
          _check_seq([cond_var], loop_taint, loop_covered, local_no_gc, 0,
                     f"{path}[{idx}].for.cond_var")
        for part_name in ("cond", "step"):
          _report_hazards(
              _expr_hazards(stmt.get(part_name), loop_taint, benign_methods),
              loop_covered,
              False,
              0,
              f"{path}[{idx}] For.{part_name}",
          )
          loop_taint.update(
              _collect_outparam_taints(stmt.get(part_name), loop_taint))
        body = stmt.get("body")
        if body is not None:
          body_list = body["body"] if body.get("type") == "Block" else [body]
          _check_seq(
              body_list,
              loop_taint,
              loop_covered,
              local_no_gc,
              loop_get_budget,
              f"{path}[{idx}].for.body",
          )
        # A marker before/inside a loop NEVER leaks past the loop!
        local_covered = False
        local_get_budget = 0
        just_marked = False
        continue

      if stype == "RangeFor":
        loop_taint = set(local_taint)
        _report_hazards(
            _expr_hazards(stmt.get("iterable"), loop_taint, benign_methods),
            local_covered,
            False,
            0,
            f"{path}[{idx}] RangeFor.iterable",
        )
        loop_taint.update(
            _collect_outparam_taints(stmt.get("iterable"), loop_taint))
        if _expr_references_tainted(stmt.get("iterable"), loop_taint):
          loop_taint.add(stmt["var"])
        body = stmt.get("body")
        if body is not None:
          body_list = body["body"] if body.get("type") == "Block" else [body]
          _check_seq(body_list, loop_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].rangefor.body")
        # Multiple sibling RangeFor loops right under BG_FORWARD_POINTERS or
        # FG_FORWARD_POINTERS share that phase marker ONLY inside the same
        # `if (forwarder.HasAnythingToForward())` block.
        if path == "root":
          local_covered = False
          local_get_budget = 0
        just_marked = False
        continue

      if stype in ("While", "DoWhile"):
        loop_taint = set(local_taint)
        cond_var = stmt.get("cond_var")
        if cond_var is not None:
          _check_seq([cond_var], loop_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].while.cond_var")
        _report_hazards(
            _expr_hazards(stmt.get("cond"), loop_taint, benign_methods),
            local_covered,
            False,
            0,
            f"{path}[{idx}] {stype}.cond",
        )
        loop_taint.update(
            _collect_outparam_taints(stmt.get("cond"), loop_taint))
        body = stmt.get("body")
        if body is not None:
          body_list = body["body"] if body.get("type") == "Block" else [body]
          _check_seq(body_list, loop_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].{stype.lower()}.body")
        local_covered = False
        local_get_budget = 0
        just_marked = False
        continue

      if stype == "Switch":
        sw_taint = set(local_taint)
        if stmt.get("init") is not None:
          _check_seq([stmt["init"]], sw_taint, local_covered, local_no_gc, 0,
                     f"{path}[{idx}].switch.init")
        if stmt.get("cond_var") is not None:
          _check_seq([stmt["cond_var"]], sw_taint, local_covered, local_no_gc,
                     0, f"{path}[{idx}].switch.cond_var")
        _report_hazards(
            _expr_hazards(stmt.get("cond"), sw_taint, benign_methods),
            local_covered,
            False,
            0,
            f"{path}[{idx}] Switch.cond",
        )
        sw_taint.update(_collect_outparam_taints(stmt.get("cond"), sw_taint))
        _check_seq(
            stmt.get("items", []), sw_taint, local_covered, local_no_gc, 0,
            f"{path}[{idx}].switch.items")
        local_covered = False
        local_get_budget = 0
        just_marked = False
        continue

  _check_seq(stmts, set(_INITIAL_SHARED_TAINT), False, False, 0, "root")
  return violations


def _strip_cxx_comments(src: str) -> str:
  src = re.sub(r"/\*.*?\*/", " ", src, flags=re.DOTALL)
  src = re.sub(r"//[^\n]*", "", src)
  return src


def check_macro_definitions(
    raw_compiler_cc: str,
    raw_sfi_inl: str = "",
    raw_sfi_cc: str = "",
) -> List[str]:
  """Verifies MERGE_MODEL_STEP macros & all parsed-file sync points are *_TEST_ONLY."""
  violations: List[str] = []
  # Strip backslashes/newlines inside #define blocks for clean checking.
  joined = re.sub(r"\\\n", " ", raw_compiler_cc)
  m_step = re.findall(r"^[ \t]*#define[ \t]+MERGE_MODEL_STEP\b[^\n]*", joined,
                      re.MULTILINE)
  m_safe = re.findall(
      r"^[ \t]*#define[ \t]+MERGE_MODEL_STEP_WITH_SAFEPOINT\b[^\n]*",
      joined,
      re.MULTILINE,
  )
  if not m_step:
    violations.append(
        "Missing #define MERGE_MODEL_STEP in src/codegen/compiler.cc")
  for line in m_step:
    if "SYNCHRONIZATION_POINT_TEST_ONLY" not in line:
      violations.append(
          f"MERGE_MODEL_STEP must use SYNCHRONIZATION_POINT_TEST_ONLY "
          f"(no production overhead): {line.strip()!r}")
  if not m_safe:
    violations.append(
        "Missing #define MERGE_MODEL_STEP_WITH_SAFEPOINT in src/codegen/compiler.cc"
    )
  for line in m_safe:
    if "SYNCHRONIZATION_POINT_WITH_SAFEPOINT_TEST_ONLY" not in line:
      violations.append(
          f"MERGE_MODEL_STEP_WITH_SAFEPOINT must use "
          f"SYNCHRONIZATION_POINT_WITH_SAFEPOINT_TEST_ONLY: {line.strip()!r}")

  # Also check EVERY direct SYNCHRONIZATION_POINT[_WITH_SAFEPOINT] invocation
  # across all three parsed V8 files (rejecting any non-*_TEST_ONLY sync point
  # except the single pre-existing upstream BeforeGetOuterScopeInfo).
  sync_re = re.compile(
      r"\b(SYNCHRONIZATION_POINT|SYNCHRONIZATION_POINT_WITH_SAFEPOINT)\s*\(([^)]*)\)"
  )
  for fname, content in (
      ("src/codegen/compiler.cc", raw_compiler_cc),
      ("src/objects/shared-function-info-inl.h", raw_sfi_inl),
      ("src/objects/shared-function-info.cc", raw_sfi_cc),
  ):
    if not content:
      continue
    cleaned = _strip_cxx_comments(re.sub(r"\\\n", " ", content))
    for match in sync_re.finditer(cleaned):
      macro_name = match.group(1)
      arg_str = match.group(2).strip()
      lit_m = re.match(r'^"([^"]*)"$', arg_str)
      sp_name = lit_m.group(1) if lit_m else arg_str
      if (fname, macro_name, sp_name) in _UPSTREAM_NON_TEST_ONLY_SYNC_POINTS:
        continue
      violations.append(
          f"{fname}: non-TEST_ONLY {macro_name}({arg_str}) is forbidden "
          f"(must use {macro_name}_TEST_ONLY for zero shipping overhead)")
  return violations


def _dedup_consecutive(seq: List[str]) -> List[str]:
  out: List[str] = []
  for item in seq:
    if not out or out[-1] != item:
      out.append(item)
  return out


def check_cxx_synchronization(
    bundle: ParsedCxxBundle,
    *,
    check_macros: bool = True,
) -> Dict[str, Any]:
  """Runs all static synchronization checks on ``bundle``; raises on mismatch."""
  bg_order = extract_ordered_markers(bundle.bg_stmts)
  fg_order = extract_ordered_markers(bundle.fg_stmts)

  errors: List[str] = []
  if tuple(bg_order) != EXPECTED_BG_STEPS:
    errors.append(
        f"BeginMergeInBackground MERGE_MODEL_STEP sequence mismatch:\n"
        f"  expected: {list(EXPECTED_BG_STEPS)}\n"
        f"  actual:   {bg_order}")

  # Only CL 2 (which checks `sfis_without_scope_info_.contains(i)` in
  # CompleteMergeInForeground) legitimately places FG_DETECT_CONCURRENT_SFI in
  # two adjacent mutually-exclusive branches; CL 1 and HEAD (CL 3) require an
  # exact 1:1 match with EXPECTED_FG_STEPS.
  allow_dup_fg_detect = "sfis_without_scope_info_.contains(" in bundle.raw_compiler_cc
  if allow_dup_fg_detect:
    fg_dedup = _dedup_consecutive(fg_order)
    bad_fg_counts = [
        m for m in EXPECTED_FG_STEPS
        if (m != "FG_DETECT_CONCURRENT_SFI" and fg_order.count(m) != 1) or
        (m == "FG_DETECT_CONCURRENT_SFI" and fg_order.count(m) not in (1, 2))
    ]
    if tuple(fg_dedup) != EXPECTED_FG_STEPS or bad_fg_counts:
      errors.append(
          f"CompleteMergeInForeground MERGE_MODEL_STEP sequence mismatch:\n"
          f"  expected: {list(EXPECTED_FG_STEPS)}\n"
          f"  actual:   {fg_order}")
  elif tuple(fg_order) != EXPECTED_FG_STEPS:
    errors.append(
        f"CompleteMergeInForeground MERGE_MODEL_STEP sequence mismatch:\n"
        f"  expected: {list(EXPECTED_FG_STEPS)}\n"
        f"  actual:   {fg_order}")

  impure_fwd = _compute_impure_forwarder_methods(bundle.fwd_methods)
  bg_hz = verify_hazard_markers(
      bundle.bg_stmts, "BeginMergeInBackground", impure_methods=impure_fwd)
  fg_hz = verify_hazard_markers(
      bundle.fg_stmts, "CompleteMergeInForeground", impure_methods=impure_fwd)
  errors.extend(bg_hz)
  errors.extend(fg_hz)

  if check_macros:
    errors.extend(
        check_macro_definitions(
            bundle.raw_compiler_cc,
            bundle.raw_sfi_inl,
            bundle.raw_sfi_cc,
        ))

  if errors:
    raise ConcurrencySyncMismatchError("\n".join(errors))

  return {
      "bg_markers": bg_order,
      "fg_markers": fg_order,
      "total_markers": len(bg_order) + len(fg_order),
  }
