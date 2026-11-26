# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Fail-closed C++20 AST interpreter and per-bundle V8 runtime binder.

Executes Clang-parsed ASTs from ``clang_frontend.py`` directly against
``v8_heap_model.py`` without global monkey-patching. Fixes every C++ semantic
defect found in the audit:

* Block-level lexical ``ScopeStack`` with deterministic LIFO RAII destruction
  (``__cxx_exit__``) across normal exit, ``return``, ``break``, ``continue``,
  and exceptions.
* Full C++ compound assignments (``+=``, ``-=``, ``*=``, ``/=``, ``%=``,
  ``&=``, ``|=``, ``^=``, ``<<=``, ``>>=``) on locals, members, subscripts,
  dereferences, and references.
* C++ truncating ``/`` and ``%`` and fixed-width signed/unsigned integer
  wrapping.
* Value-copy initialization for containers/structs (``__cxx_copy__``),
  by-reference ``RangeFor``, capture-respecting ``Lambda`` closures,
  prefix/postfix ``++``/``--`` on integers and iterators, ``Switch`` with
  fallthrough, ``DoWhile``, ``While``, ``For`` (expr init + ``cond_var``),
  ``If`` (``init`` + ``cond_var``), ``OffsetOf``, and ``TypeTrait``.
* Strict identifier lookup (no ``name.endswith("_")`` heuristic, no
  ``globals()`` fallback) and arity/type overload resolution for parsed V8
  methods.
"""

from __future__ import annotations

import re
from typing import Any, Callable, Dict, List, Optional, Tuple

from clang_frontend import ParsedCxxBundle, ParsedMethod
from errors import (
    CxxDcheckFailure,
    CxxHarnessError,
    GcInNoGcScopeError,
    UnsupportedCxxAstError,
)
from v8_heap_model import (
    AcquireLoadTag,
    BackgroundMergeState,
    BackgroundMergeTask,
    Builtin,
    BytecodeArray,
    ConstantPoolPointerForwarder,
    CxxUnorderedMap,
    CxxUnorderedSet,
    CxxVector,
    CxxVectorIterator,
    DisallowGarbageCollection,
    DisallowHeapAllocation,
    DisallowSafepoints,
    FeedbackMetadata,
    FixedArray,
    HandleBox,
    HandleScope,
    HeapObject,
    Isolate,
    IsolateForSandbox,
    LocalHandleScope,
    LocalHeap,
    LocalIsolate,
    MakeWeak,
    MaybeObject,
    MemoryOrder,
    NullTagged,
    ObjectBoilerplateDescription,
    PersistentHandles,
    READ_ONLY_ROOTS,
    RawTaggedRef,
    RelaxedLoadTag,
    RelaxedStoreTag,
    ReleaseStoreTag,
    SFI_AGE_SIZE,
    SFI_OFFSETS,
    SFI_TOTAL_SIZE,
    SKIP_WRITE_BARRIER,
    ScopeInfo,
    ScopeType,
    Script,
    SharedFunctionInfo,
    Smi,
    StdMemoryOrder,
    String,
    THE_HOLE,
    UNDEFINED,
    UNINITIALIZED_HOLE,
    UPDATE_WRITE_BARRIER,
    V8Flags,
    V8Heap,
    cast_cxx_type,
    cxx_copy_value,
    cxx_handle,
    cxx_memcmp,
    is_cxx_type,
    kAcquireLoad,
    kFunctionLiteralIdTopLevel,
    kInvalidInfoId,
    kMaxInt,
    kNoSharedNameSentinel,
    kRelaxedLoad,
    kRelaxedStore,
    kReleaseStore,
    normalize_cxx_type_name,
    try_cast_cxx_type,
    unwrap_for_gc,
    unwrap_raw,
)

# ---------------------------------------------------------------------------
# Control-flow signals & LValue / Scope / Closure primitives
# ---------------------------------------------------------------------------


class _ReturnSignal(Exception):
  __slots__ = ("value",)

  def __init__(self, value: Any = None):
    self.value = value


class _BreakSignal(Exception):
  pass


class _ContinueSignal(Exception):
  pass


class _LValue:
  """Mutable storage location (variable cell, member, subscript, or deref)."""

  __slots__ = ("getter", "setter", "qual_type", "releaser")

  def __init__(
      self,
      getter: Callable[[], Any],
      setter: Callable[[Any], None],
      qual_type: str = "",
      releaser: Optional[Callable[[], Any]] = None,
  ):
    self.getter = getter
    self.setter = setter
    self.qual_type = qual_type
    self.releaser = releaser

  def get(self) -> Any:
    return self.getter()

  def set(self, val: Any) -> None:
    self.setter(val)

  def release(self) -> Any:
    if self.releaser is not None:
      return self.releaser()
    val = self.getter()
    self.setter(None)
    return val


class _VarCell:
  """One lexical variable binding (value cell or reference alias)."""

  __slots__ = ("name", "value", "qual_type", "ref_target")

  def __init__(
      self,
      name: str,
      value: Any,
      qual_type: str = "",
      ref_target: Optional[_LValue] = None,
  ):
    self.name = name
    self.value = value
    self.qual_type = qual_type
    self.ref_target = ref_target

  def get(self) -> Any:
    if self.ref_target is not None:
      return self.ref_target.get()
    return self.value

  def set(self, val: Any) -> None:
    coerced = coerce_cxx_scalar(val, self.qual_type)
    if self.ref_target is not None:
      self.ref_target.set(coerced)
    else:
      self.value = coerced

  def as_lvalue(self) -> _LValue:
    return _LValue(self.get, self.set, self.qual_type)


class _PtrCell:
  """Models C++ pointer ``&lvalue`` (supports ``*p`` read/write and out-params)."""

  __slots__ = ("lvalue",)

  def __init__(self, lvalue: _LValue):
    self.lvalue = lvalue

  def get(self) -> Any:
    return self.lvalue.get()

  def set(self, val: Any) -> None:
    self.lvalue.set(val)

  def deref(self) -> Any:
    return self.lvalue.get()


class _ScopeFrame:
  """Single lexical block scope tracking declared cells in declaration order."""

  __slots__ = ("cells", "decl_order")

  def __init__(self):
    self.cells: Dict[str, _VarCell] = {}
    self.decl_order: List[_VarCell] = []

  def declare(
      self,
      name: str,
      value: Any,
      qual_type: str = "",
      ref_target: Optional[_LValue] = None,
  ) -> _VarCell:
    cell = _VarCell(name, value, qual_type=qual_type, ref_target=ref_target)
    self.cells[name] = cell
    self.decl_order.append(cell)
    return cell

  def exit_raii(self) -> None:
    # Destroy local objects in strict C++ reverse declaration order (LIFO).
    for cell in reversed(self.decl_order):
      if cell.ref_target is None:
        obj = cell.value
        exit_fn = getattr(type(obj), "__cxx_exit__", None)
        if exit_fn is not None:
          exit_fn(obj)


class _ScopeStack:
  """Lexical scope stack for a single function/lambda invocation."""

  def __init__(self, initial_frames: Optional[List[_ScopeFrame]] = None):
    self.frames: List[_ScopeFrame] = list(
        initial_frames) if initial_frames else []

  def push(self) -> _ScopeFrame:
    f = _ScopeFrame()
    self.frames.append(f)
    return f

  def pop(self) -> None:
    f = self.frames.pop()
    f.exit_raii()

  def declare(
      self,
      name: str,
      value: Any,
      qual_type: str = "",
      ref_target: Optional[_LValue] = None,
  ) -> _VarCell:
    return self.frames[-1].declare(
        name, value, qual_type=qual_type, ref_target=ref_target)

  def lookup_cell(self, name: str) -> Optional[_VarCell]:
    for f in reversed(self.frames):
      if name in f.cells:
        return f.cells[name]
    return None


class _CxxStruct:
  """Generic value-copyable POD struct for user-defined C++ structs in snippets."""

  def __init__(self, type_name: str, fields: Optional[Dict[str, Any]] = None):
    self._type_name = type_name
    if fields:
      for k, v in fields.items():
        setattr(self, k, cxx_copy_value(v))

  def __cxx_copy__(self) -> "_CxxStruct":
    fields = {k: v for k, v in self.__dict__.items() if k != "_type_name"}
    return _CxxStruct(self._type_name, fields)

  def __repr__(self) -> str:
    return f"CxxStruct({self._type_name})"


class _CxxClosure:
  """Executable C++ lambda with isolated capture frame (by-val vs by-ref)."""

  def __init__(
      self,
      interpreter: "CxxAstInterpreter",
      node: Dict[str, Any],
      capture_frame: _ScopeFrame,
      this_obj: Any,
  ):
    self.interpreter = interpreter
    self.params: List[Dict[str, Any]] = node["params"]
    self.body: List[Dict[str, Any]] = node["body"]
    self.capture_frame = capture_frame
    self.this_obj = this_obj

  def __call__(self,
               *args: Any,
               arg_lvalues: Optional[List[Optional[_LValue]]] = None) -> Any:
    return self.interpreter.invoke_closure(self, list(args), arg_lvalues)


# ---------------------------------------------------------------------------
# C++ integer wrapping & truncating arithmetic helpers
# ---------------------------------------------------------------------------

_INT_TYPE_SPECS: Dict[str, Tuple[int, bool]] = {
    "uint8_t": (8, False),
    "unsigned char": (8, False),
    "int8_t": (8, True),
    "signed char": (8, True),
    "char": (8, True),
    "uint16_t": (16, False),
    "unsigned short": (16, False),
    "int16_t": (16, True),
    "short": (16, True),
    "uint32_t": (32, False),
    "unsigned int": (32, False),
    "unsigned": (32, False),
    "int32_t": (32, True),
    "int": (32, True),
    "uint64_t": (64, False),
    "unsigned long": (64, False),
    "unsigned long long": (64, False),
    "size_t": (64, False),
    "__size_t": (64, False),
    "uintptr_t": (64, False),
    "Address": (64, False),
    "int64_t": (64, True),
    "long": (64, True),
    "long long": (64, True),
    "intptr_t": (64, True),
    "ptrdiff_t": (64, True),
}


def _strip_cv_ref(qual_type: str) -> str:
  t = qual_type.strip()
  for prefix in ("const ", "volatile ", "constexpr "):
    while t.startswith(prefix):
      t = t[len(prefix):].strip()
  for suffix in (" const", " volatile", "&"):
    while t.endswith(suffix):
      t = t[:-len(suffix)].strip()
  for ns in ("std::", "v8::internal::", "i::"):
    if t.startswith(ns):
      t = t[len(ns):]
  return t


def coerce_cxx_scalar(val: Any, qual_type: str) -> Any:
  """Applies fixed-width C++ signed/unsigned wrapping or bool conversion."""
  if not qual_type:
    return val
  base = _strip_cv_ref(qual_type)
  if base == "bool":
    if isinstance(val, (int, bool)):
      return bool(val)
    return val
  spec = _INT_TYPE_SPECS.get(base)
  if spec is not None and isinstance(val, (int, bool)):
    bits, signed = spec
    mask = (1 << bits) - 1
    masked = int(val) & mask
    if signed and masked >= (1 << (bits - 1)):
      return masked - (1 << bits)
    return masked
  return val


def cxx_trunc_div(a: Any, b: Any) -> Any:
  """C++ division: truncates toward zero for integers (not Python floor div)."""
  if isinstance(a, float) or isinstance(b, float):
    return a / b
  ai, bi = int(a), int(b)
  if bi == 0:
    raise CxxDcheckFailure("Integer division by zero")
  q = abs(ai) // abs(bi)
  return -q if (ai < 0) ^ (bi < 0) else q


def cxx_trunc_mod(a: Any, b: Any) -> int:
  """C++ remainder: sign matches dividend (``a - (a / b) * b``)."""
  ai, bi = int(a), int(b)
  if bi == 0:
    raise CxxDcheckFailure("Integer modulo by zero")
  return ai - cxx_trunc_div(ai, bi) * bi


def cxx_truthy(val: Any) -> bool:
  if val is None or isinstance(val, NullTagged):
    return False
  if isinstance(val, HandleBox):
    return not val.is_null()
  if isinstance(val, RawTaggedRef):
    return not val.is_null()
  if isinstance(val, Smi):
    return val.value() != 0
  return bool(val)


def _apply_binary_op(op: str,
                     left: Any,
                     right: Any,
                     qual_type: str = "") -> Any:
  if op == "+":
    res = left + right
  elif op == "-":
    res = left - right
  elif op == "*":
    res = left * right
  elif op == "/":
    res = cxx_trunc_div(left, right)
  elif op == "%":
    res = cxx_trunc_mod(left, right)
  elif op == "<<":
    res = int(left) << int(right)
  elif op == ">>":
    res = int(left) >> int(right)
  elif op == "&":
    res = int(left) & int(right)
  elif op == "|":
    res = int(left) | int(right)
  elif op == "^":
    res = int(left) ^ int(right)
  elif op == "==":
    return left == right
  elif op == "!=":
    return left != right
  elif op == "<":
    return left < right
  elif op == "<=":
    return left <= right
  elif op == ">":
    return left > right
  elif op == ">=":
    return left >= right
  else:
    raise UnsupportedCxxAstError(f"Unsupported binary operator: {op!r}")
  return coerce_cxx_scalar(res, qual_type)


# ---------------------------------------------------------------------------
# Explicit C++ Runtime Symbol Registry (ZERO globals() fallback!)
# ---------------------------------------------------------------------------


def _make_runtime_registry(
    runtime: Optional["BoundMergeRuntime"]) -> Dict[str, Any]:
  """Builds the strict C++ symbol table visible to interpreted functions."""

  def _dcheck(cond: Any, msg: str = "DCHECK failed") -> None:
    if not cxx_truthy(cond):
      raise CxxDcheckFailure(f"C++ {msg}")

  def _dcheck_eq(a: Any, b: Any) -> None:
    if not (a == b):
      raise CxxDcheckFailure(f"C++ DCHECK_EQ failed: {a!r} != {b!r}")

  def _dcheck_ne(a: Any, b: Any) -> None:
    if not (a != b):
      raise CxxDcheckFailure(f"C++ DCHECK_NE failed: {a!r} == {b!r}")

  def _dcheck_le(a: Any, b: Any) -> None:
    if not (a <= b):
      raise CxxDcheckFailure(f"C++ DCHECK_LE failed: {a!r} > {b!r}")

  def _dcheck_lt(a: Any, b: Any) -> None:
    if not (a < b):
      raise CxxDcheckFailure(f"C++ DCHECK_LT failed: {a!r} >= {b!r}")

  def _dcheck_ge(a: Any, b: Any) -> None:
    if not (a >= b):
      raise CxxDcheckFailure(f"C++ DCHECK_GE failed: {a!r} < {b!r}")

  def _dcheck_gt(a: Any, b: Any) -> None:
    if not (a > b):
      raise CxxDcheckFailure(f"C++ DCHECK_GT failed: {a!r} <= {b!r}")

  def _dcheck_implies(a: Any, b: Any) -> None:
    if cxx_truthy(a) and not cxx_truthy(b):
      raise CxxDcheckFailure("C++ DCHECK_IMPLIES failed")

  def _unreachable(*_args: Any) -> None:
    raise CxxDcheckFailure("C++ UNREACHABLE() reached")

  def _builtin_expect(expr: Any, _expected: Any = 0) -> Any:
    return expr

  def _get_read_only_roots(*_args: Any) -> Any:
    return READ_ONLY_ROOTS

  def _get_current_isolate_for_sandbox() -> IsolateForSandbox:
    return IsolateForSandbox(runtime.isolate if runtime else None)

  def _print(*_args: Any) -> None:
    pass

  reg: Dict[str, Any] = {
      # Assertions & builtins
      "DCHECK":
          lambda c: _dcheck(c, "DCHECK failed"),
      "CHECK":
          lambda c: _dcheck(c, "CHECK failed"),
      "SBXCHECK":
          lambda c: _dcheck(c, "SBXCHECK failed"),
      "DCHECK_EQ":
          _dcheck_eq,
      "CHECK_EQ":
          _dcheck_eq,
      "DCHECK_NE":
          _dcheck_ne,
      "CHECK_NE":
          _dcheck_ne,
      "DCHECK_LE":
          _dcheck_le,
      "CHECK_LE":
          _dcheck_le,
      "DCHECK_LT":
          _dcheck_lt,
      "CHECK_LT":
          _dcheck_lt,
      "DCHECK_GE":
          _dcheck_ge,
      "CHECK_GE":
          _dcheck_ge,
      "DCHECK_GT":
          _dcheck_gt,
      "CHECK_GT":
          _dcheck_gt,
      "DCHECK_IMPLIES":
          _dcheck_implies,
      "CHECK_IMPLIES":
          _dcheck_implies,
      "UNREACHABLE":
          _unreachable,
      "__builtin_expect":
          _builtin_expect,
      "Print":
          _print,
      "PrintF":
          _print,
      "memcmp":
          cxx_memcmp,
      # V8 runtime helpers
      "handle":
          cxx_handle,
      "MakeWeak":
          MakeWeak,
      "GetReadOnlyRoots":
          _get_read_only_roots,
      "GetCurrentIsolateForSandbox":
          _get_current_isolate_for_sandbox,
      # Type predicate free functions
      "IsScopeInfo":
          lambda v: is_cxx_type(v, "ScopeInfo"),
      "IsSharedFunctionInfo":
          lambda v: is_cxx_type(v, "SharedFunctionInfo"),
      "IsBytecodeArray":
          lambda v: is_cxx_type(v, "BytecodeArray"),
      "IsFeedbackMetadata":
          lambda v: is_cxx_type(v, "FeedbackMetadata"),
      "IsFixedArray":
          lambda v: is_cxx_type(v, "FixedArray"),
      "IsObjectBoilerplateDescription":
          lambda v: is_cxx_type(v, "ObjectBoilerplateDescription"),
      "IsSmi":
          lambda v: is_cxx_type(v, "Smi"),
      "IsTheHole":
          lambda v: is_cxx_type(v, "TheHole"),
      "IsUndefined":
          lambda v: is_cxx_type(v, "Undefined"),
      "IsUninitializedHole":
          lambda v: is_cxx_type(v, "UninitializedHole"),
      "IsString":
          lambda v: is_cxx_type(v, "String"),
      "IsScript":
          lambda v: is_cxx_type(v, "Script"),
      # Constants, tags, and enums
      "kAcquireLoad":
          kAcquireLoad,
      "kReleaseStore":
          kReleaseStore,
      "kRelaxedLoad":
          kRelaxedLoad,
      "kRelaxedStore":
          kRelaxedStore,
      "SKIP_WRITE_BARRIER":
          SKIP_WRITE_BARRIER,
      "UPDATE_WRITE_BARRIER":
          UPDATE_WRITE_BARRIER,
      "kFunctionLiteralIdTopLevel":
          kFunctionLiteralIdTopLevel,
      "kMaxInt":
          kMaxInt,
      "kInvalidInfoId":
          kInvalidInfoId,
      "kNoSharedNameSentinel":
          kNoSharedNameSentinel,
      "SCRIPT_SCOPE":
          ScopeType.SCRIPT_SCOPE,
      "FUNCTION_SCOPE":
          ScopeType.FUNCTION_SCOPE,
      "MODULE_SCOPE":
          ScopeType.MODULE_SCOPE,
      "EVAL_SCOPE":
          ScopeType.EVAL_SCOPE,
      "BLOCK_SCOPE":
          ScopeType.BLOCK_SCOPE,
      "WITH_SCOPE":
          ScopeType.WITH_SCOPE,
      "kNotStarted":
          BackgroundMergeState.kNotStarted,
      "kPendingBackgroundWork":
          BackgroundMergeState.kPendingBackgroundWork,
      "kPendingForegroundWork":
          BackgroundMergeState.kPendingForegroundWork,
      "kDone":
          BackgroundMergeState.kDone,
      # Namespace/class objects for qualified lookups
      "Smi":
          Smi,
      "Builtin":
          Builtin,
      "ScopeType":
          ScopeType,
      "Script":
          Script,
      "SharedFunctionInfo":
          SharedFunctionInfo,
      "BackgroundMergeTask":
          BackgroundMergeTask,
      "std":
          StdMemoryOrder,
      # Flags & legacy static var for --git-rev origin/main
      "v8_flags":
          runtime.flags if runtime else V8Flags(),
      "force_gc_during_next_merge_for_testing_":
          False,
  }
  if runtime is not None:
    reg["VerifyCodeMerge"] = runtime.call_verify_code_merge
    reg["VerifyConstantPoolEntry"] = runtime.call_verify_constant_pool_entry
  return reg


# ---------------------------------------------------------------------------
# CxxAstInterpreter
# ---------------------------------------------------------------------------


class CxxAstInterpreter:
  """Interprets Clang-converted C++ ASTs with full block-scoped RAII semantics."""

  def __init__(
      self,
      runtime: Optional["BoundMergeRuntime"] = None,
      *,
      ilp_eval_order: str = "ltr",
      extra_functions: Optional[Dict[str, ParsedMethod]] = None,
  ):
    if ilp_eval_order not in ("ltr", "rtl"):
      raise ValueError(f"Unsupported ilp_eval_order: {ilp_eval_order!r}")
    self.runtime = runtime
    self.ilp_eval_order = ilp_eval_order
    self.registry = _make_runtime_registry(runtime)
    self.extra_functions: Dict[str, ParsedMethod] = dict(extra_functions or {})

  def execute_method(
      self,
      method: ParsedMethod,
      *,
      this_obj: Any = None,
      args: Optional[List[Any]] = None,
      arg_lvalues: Optional[List[Optional[_LValue]]] = None,
      extra_locals: Optional[Dict[str, Any]] = None,
  ) -> Any:
    return self.execute_statements(
        method.body,
        params=method.params,
        this_obj=this_obj,
        args=args or [],
        arg_lvalues=arg_lvalues,
        extra_locals=extra_locals,
        method_name=method.name,
    )

  def execute_statements(
      self,
      stmts: List[Dict[str, Any]],
      *,
      params: Optional[List[Tuple[str, str, bool]]] = None,
      this_obj: Any = None,
      args: Optional[List[Any]] = None,
      arg_lvalues: Optional[List[Optional[_LValue]]] = None,
      extra_locals: Optional[Dict[str, Any]] = None,
      method_name: str = "",
  ) -> Any:
    scopes = _ScopeStack()
    scopes.push()
    try:
      if params:
        a_list = args or []
        lv_list = arg_lvalues or [None] * len(a_list)
        for idx, (ptype, pname, is_ref) in enumerate(params):
          if idx >= len(a_list):
            scopes.declare(
                pname, self._default_param_value(ptype), qual_type=ptype)
            continue
          if is_ref and idx < len(lv_list) and lv_list[idx] is not None:
            scopes.declare(
                pname, None, qual_type=ptype, ref_target=lv_list[idx])
          else:
            val = coerce_cxx_scalar(cxx_copy_value(a_list[idx]), ptype)
            scopes.declare(pname, val, qual_type=ptype)
      if extra_locals:
        for k, v in extra_locals.items():
          scopes.declare(k, v)
      try:
        self._exec_stmt_list(stmts, scopes, this_obj)
      except _ReturnSignal as ret:
        return ret.value
      return None
    finally:
      scopes.pop()

  def invoke_closure(
      self,
      closure: _CxxClosure,
      args: List[Any],
      arg_lvalues: Optional[List[Optional[_LValue]]] = None,
  ) -> Any:
    scopes = _ScopeStack(initial_frames=[closure.capture_frame])
    scopes.push()
    try:
      lv_list = arg_lvalues or [None] * len(args)
      for idx, pinfo in enumerate(closure.params):
        if idx >= len(args):
          break
        if isinstance(pinfo, dict):
          pname = pinfo["name"]
          ptype = pinfo.get("qual_type", "")
          is_ref = bool(pinfo.get("is_ref", False))
        else:
          ptype, pname, is_ref = pinfo
        if is_ref and idx < len(lv_list) and lv_list[idx] is not None:
          scopes.declare(pname, None, qual_type=ptype, ref_target=lv_list[idx])
        else:
          val = coerce_cxx_scalar(cxx_copy_value(args[idx]), ptype)
          scopes.declare(pname, val, qual_type=ptype)
      try:
        self._exec_stmt_list(closure.body, scopes, closure.this_obj)
      except _ReturnSignal as ret:
        return ret.value
      return None
    finally:
      scopes.pop()

  # --- Statements ---

  def _exec_stmt_list(self, stmts: List[Dict[str, Any]], scopes: _ScopeStack,
                      this_obj: Any) -> None:
    for stmt in stmts:
      self._exec_stmt(stmt, scopes, this_obj)

  def _exec_block(self, block: Optional[Dict[str, Any]], scopes: _ScopeStack,
                  this_obj: Any) -> None:
    if block is None:
      return
    scopes.push()
    try:
      if block.get("type") == "Block":
        self._exec_stmt_list(block["body"], scopes, this_obj)
      else:
        self._exec_stmt(block, scopes, this_obj)
    finally:
      scopes.pop()

  def _exec_stmt(self, stmt: Dict[str, Any], scopes: _ScopeStack,
                 this_obj: Any) -> None:
    stype = stmt.get("type")

    if stype == "Block":
      self._exec_block(stmt, scopes, this_obj)
      return

    if stype == "Decl":
      self._exec_decl(stmt, scopes, this_obj)
      return

    if stype == "ExprStmt":
      self._eval_expr(stmt["expr"], scopes, this_obj)
      return

    if stype == "MergeModelStep":
      self._handle_merge_step(stmt["step"],
                              bool(stmt.get("allow_safepoint", False)))
      return

    if stype == "SyncPoint":
      self._handle_sync_point(stmt["name"],
                              bool(stmt.get("allow_safepoint", False)))
      return

    if stype == "If":
      scopes.push()
      try:
        if stmt.get("init") is not None:
          self._exec_stmt(stmt["init"], scopes, this_obj)
        if stmt.get("cond_var") is not None:
          self._exec_stmt(stmt["cond_var"], scopes, this_obj)
        cond_val = self._eval_expr(stmt["cond"], scopes, this_obj)
        if cxx_truthy(cond_val):
          self._exec_block(stmt.get("body"), scopes, this_obj)
        elif stmt.get("orelse") is not None:
          orelse = stmt["orelse"]
          if orelse.get("type") == "If":
            self._exec_stmt(orelse, scopes, this_obj)
          else:
            self._exec_block(orelse, scopes, this_obj)
      finally:
        scopes.pop()
      return

    if stype == "For":
      scopes.push()
      try:
        if stmt.get("init") is not None:
          init_s = stmt["init"]
          if isinstance(init_s, list):
            self._exec_stmt_list(init_s, scopes, this_obj)
          else:
            self._exec_stmt(init_s, scopes, this_obj)
        while True:
          scopes.push()
          try:
            if stmt.get("cond_var") is not None:
              self._exec_stmt(stmt["cond_var"], scopes, this_obj)
            if stmt.get("cond") is not None:
              if not cxx_truthy(
                  self._eval_expr(stmt["cond"], scopes, this_obj)):
                break
            try:
              self._exec_block(stmt.get("body"), scopes, this_obj)
            except _ContinueSignal:
              pass
          except _BreakSignal:
            break
          finally:
            scopes.pop()
          if stmt.get("step") is not None:
            self._eval_expr(stmt["step"], scopes, this_obj)
      finally:
        scopes.pop()
      return

    if stype == "RangeFor":
      iterable = self._eval_expr(stmt["iterable"], scopes, this_obj)
      var_name = stmt["var"]
      var_type = stmt.get("var_type", "")
      is_ref = bool(stmt.get("is_ref", False))
      # Support CxxVector / Python list indexing for true by-ref mutation.
      if isinstance(iterable, (CxxVector, list)):
        length = len(iterable)
        for idx in range(length):
          scopes.push()
          try:
            if is_ref:

              def _make_lv(cont=iterable, i=idx, qt=var_type):
                return _LValue(
                    lambda: cont[i],
                    lambda v: cont.__setitem__(i, coerce_cxx_scalar(v, qt)),
                    qt,
                )

              scopes.declare(
                  var_name, None, qual_type=var_type, ref_target=_make_lv())
            else:
              val = coerce_cxx_scalar(cxx_copy_value(iterable[idx]), var_type)
              scopes.declare(var_name, val, qual_type=var_type)
            try:
              self._exec_block(stmt.get("body"), scopes, this_obj)
            except _ContinueSignal:
              pass
          except _BreakSignal:
            break
          finally:
            scopes.pop()
      else:
        for elem in iterable:
          scopes.push()
          try:
            val = elem if is_ref else coerce_cxx_scalar(
                cxx_copy_value(elem), var_type)
            scopes.declare(var_name, val, qual_type=var_type)
            try:
              self._exec_block(stmt.get("body"), scopes, this_obj)
            except _ContinueSignal:
              pass
          except _BreakSignal:
            break
          finally:
            scopes.pop()
      return

    if stype == "While":
      while True:
        scopes.push()
        try:
          if stmt.get("cond_var") is not None:
            self._exec_stmt(stmt["cond_var"], scopes, this_obj)
          if not cxx_truthy(self._eval_expr(stmt["cond"], scopes, this_obj)):
            break
          try:
            self._exec_block(stmt.get("body"), scopes, this_obj)
          except _ContinueSignal:
            pass
        except _BreakSignal:
          break
        finally:
          scopes.pop()
      return

    if stype == "DoWhile":
      while True:
        try:
          self._exec_block(stmt.get("body"), scopes, this_obj)
        except _ContinueSignal:
          pass
        except _BreakSignal:
          break
        if not cxx_truthy(self._eval_expr(stmt["cond"], scopes, this_obj)):
          break
      return

    if stype == "Switch":
      scopes.push()
      try:
        if stmt.get("init") is not None:
          self._exec_stmt(stmt["init"], scopes, this_obj)
        if stmt.get("cond_var") is not None:
          self._exec_stmt(stmt["cond_var"], scopes, this_obj)
        switch_val = self._eval_expr(stmt["cond"], scopes, this_obj)
        items = stmt.get("items", [])
        start_idx: Optional[int] = None
        default_idx: Optional[int] = None
        for idx, item in enumerate(items):
          itype = item.get("type")
          if itype == "CaseLabel":
            cval = self._eval_expr(item["value"], scopes, this_obj)
            if cval == switch_val:
              start_idx = idx
              break
          elif itype == "DefaultLabel":
            default_idx = idx
        if start_idx is None:
          start_idx = default_idx
        if start_idx is not None:
          scopes.push()
          try:
            for item in items[start_idx:]:
              if item.get("type") in ("CaseLabel", "DefaultLabel"):
                continue
              self._exec_stmt(item, scopes, this_obj)
          except _BreakSignal:
            pass
          finally:
            scopes.pop()
      finally:
        scopes.pop()
      return

    if stype == "Break":
      raise _BreakSignal()

    if stype == "Continue":
      raise _ContinueSignal()

    if stype == "Return":
      val = (
          self._eval_expr(stmt["value"], scopes, this_obj)
          if stmt.get("value") is not None else None)
      raise _ReturnSignal(val)

    raise UnsupportedCxxAstError(f"Unsupported C++ statement type: {stype!r}")

  def _exec_decl(self, stmt: Dict[str, Any], scopes: _ScopeStack,
                 this_obj: Any) -> None:
    name = stmt["name"]
    qual_type = stmt.get("qual_type", "")
    is_ref = bool(stmt.get("is_ref", False))
    val_node = stmt.get("value")

    if is_ref:
      if val_node is None:
        raise CxxHarnessError(
            f"Reference declaration {name!r} without initializer")
      try:
        lv = self._eval_lvalue(val_node, scopes, this_obj)
      except CxxHarnessError:
        if re.search(r"\bconst\b", qual_type) or "&&" in qual_type:
          val = self._eval_expr(val_node, scopes, this_obj)
          val = coerce_cxx_scalar(cxx_copy_value(val), qual_type)
          scopes.declare(name, val, qual_type=qual_type)
          return
        raise
      scopes.declare(name, None, qual_type=qual_type, ref_target=lv)
      return

    if val_node is None:
      val = self._default_construct_type(qual_type)
    else:
      val = self._eval_expr(val_node, scopes, this_obj)
      val = coerce_cxx_scalar(cxx_copy_value(val), qual_type)
    scopes.declare(name, val, qual_type=qual_type)

  def _default_param_value(self, qual_type: str) -> Any:
    pt = normalize_cxx_type_name(qual_type)
    if pt == "WriteBarrierMode":
      return UPDATE_WRITE_BARRIER
    if pt == "IsolateForSandbox":
      return IsolateForSandbox(self.runtime.isolate if self.runtime else None)
    if pt == "AcquireLoadTag":
      return kAcquireLoad
    if pt == "ReleaseStoreTag":
      return kReleaseStore
    if pt == "RelaxedLoadTag":
      return kRelaxedLoad
    if pt == "RelaxedStoreTag":
      return kRelaxedStore
    return self._default_construct_type(qual_type)

  def _default_construct_type(self, qual_type: str) -> Any:
    base = _strip_cv_ref(qual_type)
    if base.startswith("std::unordered_map") or base.startswith(
        "unordered_map"):
      return CxxUnorderedMap()
    if base.startswith("std::unordered_set") or base.startswith(
        "unordered_set"):
      return CxxUnorderedSet()
    if base.startswith("std::vector") or base.startswith("vector"):
      return CxxVector()
    if base.startswith("Tagged<"):
      return NullTagged(base[len("Tagged<"):-1])
    if base.startswith(
        ("Handle<", "DirectHandle<", "IndirectHandle<", "MaybeHandle<")):
      return HandleBox(None, is_maybe=base.startswith("MaybeHandle<"))
    if base == "bool":
      return False
    if base in _INT_TYPE_SPECS:
      return 0
    if base in ("float", "double"):
      return 0.0
    if base == "DisallowGarbageCollection":
      return DisallowGarbageCollection(self.runtime.heap)
    if base == "DisallowHeapAllocation":
      return DisallowHeapAllocation(self.runtime.heap)
    if base == "DisallowSafepoints":
      return DisallowSafepoints(self.runtime.heap)
    if base and not base.endswith("*") and base not in ("void", "auto"):
      return _CxxStruct(base)
    return None

  # --- Sync / Model Step Hooks ---

  def _handle_merge_step(self, step_name: str, allow_safepoint: bool) -> None:
    if self.runtime is None:
      return
    heap = self.runtime.heap
    mc = heap.mem_ctx
    if allow_safepoint:
      tid = mc.active_thread.tid
      if heap.no_gc_depth.get(tid, 0) > 0:
        raise GcInNoGcScopeError(
            f"MERGE_MODEL_STEP_WITH_SAFEPOINT({step_name}) executed on thread "
            f"{tid!r} inside DisallowGarbageCollection (depth={heap.no_gc_depth[tid]})"
        )
    if not mc.in_observer_callback:
      mc.in_observer_callback = True
      try:
        mc.observer.on_merge_step(step_name, allow_safepoint)
      finally:
        mc.in_observer_callback = False

  def _handle_sync_point(self, sp_name: str, allow_safepoint: bool) -> None:
    if self.runtime is None:
      return
    heap = self.runtime.heap
    mc = heap.mem_ctx
    if allow_safepoint:
      tid = mc.active_thread.tid
      if heap.no_gc_depth.get(tid, 0) > 0:
        raise GcInNoGcScopeError(
            f"SYNCHRONIZATION_POINT_WITH_SAFEPOINT({sp_name}) executed on "
            f"thread {tid!r} inside DisallowGarbageCollection")
    if not mc.in_observer_callback:
      mc.in_observer_callback = True
      try:
        mc.observer.on_sync_point(sp_name, allow_safepoint)
      finally:
        mc.in_observer_callback = False

  # --- LValues & Expressions ---

  def _eval_lvalue(self, node: Dict[str, Any], scopes: _ScopeStack,
                   this_obj: Any) -> _LValue:
    ntype = node.get("type")
    if ntype == "Name":
      name = node["name"]
      cell = scopes.lookup_cell(name)
      if cell is not None:
        return cell.as_lvalue()
      if this_obj is not None and hasattr(this_obj, name):
        return _LValue(
            lambda o=this_obj, n=name: getattr(o, n),
            lambda v, o=this_obj, n=name: setattr(o, n, v),
            node.get("qual_type", ""),
        )
      raise CxxHarnessError(f"Unbound C++ lvalue identifier: {name!r}")

    if ntype == "Member":
      base_obj = (
          this_obj if node.get("is_this_member") else self._eval_expr(
              node["base"], scopes, this_obj))
      if node.get("is_arrow") and isinstance(
          base_obj, (CxxVectorIterator, HandleBox, RawTaggedRef, _PtrCell)):
        base_obj = base_obj.deref()
      mname = node["member"]
      qt = node.get("qual_type", "")
      if base_obj is None:
        raise CxxHarnessError(f"Member access .{mname} on None")

      def _get_member(o: Any = base_obj, m: str = mname) -> Any:
        try:
          return getattr(o, m)
        except AttributeError as exc:
          raise UnsupportedCxxAstError(
              f"Unsupported C++ member access .{m} on {type(o).__name__}"
          ) from exc

      def _set_member(v: Any,
                      o: Any = base_obj,
                      m: str = mname,
                      q: str = qt) -> None:
        try:
          setattr(o, m, coerce_cxx_scalar(v, q))
        except AttributeError as exc:
          raise UnsupportedCxxAstError(
              f"Unsupported C++ member assignment .{m} on {type(o).__name__}"
          ) from exc

      return _LValue(
          _get_member,
          _set_member,
          qt,
          getattr(base_obj, f"_release_{mname}", None),
      )

    if ntype == "Subscript":
      base_obj = self._eval_expr(node["base"], scopes, this_obj)
      idx = self._eval_expr(node["index"], scopes, this_obj)
      qt = node.get("qual_type", "")
      return _LValue(
          lambda o=base_obj, i=idx: o[i],
          lambda v, o=base_obj, i=idx, q=qt: o.__setitem__(
              i, coerce_cxx_scalar(v, q)),
          qt,
      )

    if ntype == "UnaryOp" and node.get("op") == "*":
      ptr = self._eval_expr(node["operand"], scopes, this_obj)
      if isinstance(ptr, _PtrCell):
        return ptr.lvalue
      if isinstance(ptr, HandleBox):
        return _LValue(
            ptr.deref,
            lambda _v: (_ for _ in ()).throw(
                CxxHarnessError("Cannot assign through dereferenced HandleBox")
            ),
        )
      if isinstance(ptr, CxxVectorIterator):
        vec = ptr._vec
        ridx = (len(vec._data) - 1 - ptr._idx) if ptr._reverse else ptr._idx
        return _LValue(
            lambda v=vec, i=ridx: v[i],
            lambda val, v=vec, i=ridx: v.__setitem__(i, val),
        )
      raise CxxHarnessError(f"Cannot form lvalue from dereference of {ptr!r}")

    raise CxxHarnessError(
        f"Expression is not an assignable C++ lvalue: {node!r}")

  def _eval_pair_ordered(
      self,
      left_node: Dict[str, Any],
      right_node: Dict[str, Any],
      scopes: _ScopeStack,
      this_obj: Any,
  ) -> Tuple[Any, Any]:
    if self.ilp_eval_order == "rtl":
      r = self._eval_expr(right_node, scopes, this_obj)
      l = self._eval_expr(left_node, scopes, this_obj)
      return l, r
    l = self._eval_expr(left_node, scopes, this_obj)
    r = self._eval_expr(right_node, scopes, this_obj)
    return l, r

  def _eval_args_ordered(
      self, arg_nodes: List[Dict[str, Any]], scopes: _ScopeStack,
      this_obj: Any) -> Tuple[List[Any], List[Optional[_LValue]]]:
    n = len(arg_nodes)
    vals: List[Any] = [None] * n
    lvs: List[Optional[_LValue]] = [None] * n
    order = range(n - 1, -1, -1) if self.ilp_eval_order == "rtl" else range(n)
    for idx in order:
      an = arg_nodes[idx]
      if an.get("type") in ("Name", "Member",
                            "Subscript") or (an.get("type") == "UnaryOp" and
                                             an.get("op") == "*"):
        try:
          lv = self._eval_lvalue(an, scopes, this_obj)
          lvs[idx] = lv
          vals[idx] = lv.get()
          continue
        except CxxHarnessError:
          pass
      vals[idx] = self._eval_expr(an, scopes, this_obj)
    return vals, lvs

  def _eval_expr(self, node: Dict[str, Any], scopes: _ScopeStack,
                 this_obj: Any) -> Any:
    ntype = node.get("type")

    if ntype == "Literal":
      return coerce_cxx_scalar(node["value"], node.get("qual_type", ""))

    if ntype == "This":
      if this_obj is None:
        raise CxxHarnessError("Used 'this' outside a method")
      return this_obj

    if ntype == "Name":
      name = node["name"]
      cell = scopes.lookup_cell(name)
      if cell is not None:
        return cell.get()
      if this_obj is not None and hasattr(this_obj, name):
        return getattr(this_obj, name)
      if name in self.registry:
        return self.registry[name]
      if name in self.extra_functions:
        fn_pm = self.extra_functions[name]
        return lambda *a, _pm=fn_pm: self.execute_method(_pm, args=list(a))
      raise CxxHarnessError(f"Unbound C++ identifier: {name!r}")

    if ntype == "QualifiedName":
      qual = node["qualifier"]
      name = node["name"]
      if qual in ("v8::internal", "i", ""):
        if name in self.registry:
          return self.registry[name]
      if qual in self.registry:
        container = self.registry[qual]
        if hasattr(container, name):
          return getattr(container, name)
      # Support nested qualifiers like BackgroundMergeTask::State::kDone
      parts = qual.split("::")
      if parts and parts[0] in self.registry:
        cur = self.registry[parts[0]]
        for p in parts[1:]:
          if hasattr(cur, p):
            cur = getattr(cur, p)
        if hasattr(cur, name):
          return getattr(cur, name)
      if name in self.registry:
        return self.registry[name]
      raise CxxHarnessError(f"Unbound C++ qualified name: {qual}::{name}")

    if ntype == "Member":
      base_obj = (
          this_obj if node.get("is_this_member") else self._eval_expr(
              node["base"], scopes, this_obj))
      if node.get("is_arrow") and isinstance(
          base_obj, (CxxVectorIterator, HandleBox, RawTaggedRef, _PtrCell)):
        base_obj = base_obj.deref()
      mname = node["member"]
      if base_obj is None:
        raise CxxHarnessError(f"Member access .{mname} on None")
      try:
        return getattr(base_obj, mname)
      except AttributeError as exc:
        raise UnsupportedCxxAstError(
            f"Unsupported C++ member access .{mname} on {type(base_obj).__name__}"
        ) from exc

    if ntype == "Subscript":
      base_obj, idx = self._eval_pair_ordered(node["base"], node["index"],
                                              scopes, this_obj)
      return base_obj[idx]

    if ntype == "InitList":
      return [
          self._eval_expr(v, scopes, this_obj) for v in node.get("values", [])
      ]

    if ntype == "Construct":
      return self._eval_construct(node, scopes, this_obj)

    if ntype == "CastExpr":
      val = self._eval_expr(node["expr"], scopes, this_obj)
      return coerce_cxx_scalar(val, node.get("target_type", ""))

    if ntype == "OffsetOf":
      rec = normalize_cxx_type_name(node["record"])
      fld = node["field"]
      if rec == "SharedFunctionInfo" and fld in SFI_OFFSETS:
        return SFI_OFFSETS[fld]
      raise UnsupportedCxxAstError(f"Unsupported offsetof({rec}, {fld})")

    if ntype == "TypeTrait":
      trait = node["trait"]
      arg_t = normalize_cxx_type_name(node["arg_type"])
      if trait == "sizeof":
        if arg_t == "SharedFunctionInfo":
          return SFI_TOTAL_SIZE
        spec = _INT_TYPE_SPECS.get(arg_t)
        if spec is not None:
          return spec[0] // 8
        if arg_t == "bool":
          return 1
        return 8
      raise UnsupportedCxxAstError(f"Unsupported type trait: {trait}({arg_t})")

    if ntype == "Assign":
      return self._eval_assign(node, scopes, this_obj)

    if ntype == "UnaryOp":
      return self._eval_unary(node, scopes, this_obj)

    if ntype == "BinOp":
      return self._eval_binop(node, scopes, this_obj)

    if ntype == "Ternary":
      cond = self._eval_expr(node["cond"], scopes, this_obj)
      if cxx_truthy(cond):
        return self._eval_expr(node["then"], scopes, this_obj)
      return self._eval_expr(node["else"], scopes, this_obj)

    if ntype == "TemplateCall":
      return self._eval_template_call(node, scopes, this_obj)

    if ntype == "Call":
      return self._eval_call(node, scopes, this_obj)

    if ntype == "Lambda":
      return self._eval_lambda(node, scopes, this_obj)

    if ntype == "MergeModelStep":
      self._handle_merge_step(node["step"],
                              bool(node.get("allow_safepoint", False)))
      return None

    if ntype == "SyncPoint":
      self._handle_sync_point(node["name"],
                              bool(node.get("allow_safepoint", False)))
      return None

    raise UnsupportedCxxAstError(f"Unsupported C++ expression type: {ntype!r}")

  def _eval_construct(self, node: Dict[str, Any], scopes: _ScopeStack,
                      this_obj: Any) -> Any:
    ctor_type = node.get("ctor_type", "")
    base = ctor_type.split("<")[0].strip()
    args, arg_lvs = self._eval_args_ordered(
        node.get("args", []), scopes, this_obj)

    # RAII scopes & V8 constructors
    if base == "DisallowGarbageCollection":
      return DisallowGarbageCollection(self.runtime.heap)
    if base == "DisallowHeapAllocation":
      return DisallowHeapAllocation(self.runtime.heap)
    if base == "DisallowSafepoints":
      return DisallowSafepoints(self.runtime.heap)
    if base == "HandleScope":
      return HandleScope(args[0])
    if base == "LocalHandleScope":
      return LocalHandleScope(args[0])
    if base == "ConstantPoolPointerForwarder":
      return ConstantPoolPointerForwarder(args[0], args[1])
    if base == "PersistentHandles":
      return PersistentHandles(args[0] if args else None)
    if base == "IsolateForSandbox":
      return IsolateForSandbox(args[0] if args else None)
    if base == "AcquireLoadTag":
      return kAcquireLoad
    if base == "ReleaseStoreTag":
      return kReleaseStore
    if base == "RelaxedLoadTag":
      return kRelaxedLoad
    if base == "RelaxedStoreTag":
      return kRelaxedStore
    if base in ("std::unordered_map", "unordered_map"):
      return cxx_copy_value(args[0]) if args else CxxUnorderedMap()
    if base in ("std::unordered_set", "unordered_set"):
      return cxx_copy_value(args[0]) if args else CxxUnorderedSet()
    if base in ("std::vector", "vector"):
      if not args:
        return CxxVector()
      if len(args) == 1 and isinstance(args[0], CxxVector):
        return args[0].__cxx_copy__()
      if len(args) == 1 and isinstance(args[0], list):
        return CxxVector(args[0])
      if len(args) == 2 and isinstance(args[0], int):
        return CxxVector([args[1]] * args[0])
      return CxxVector(args)
    if base in ("Handle", "DirectHandle", "IndirectHandle", "MaybeHandle"):
      if not args:
        return HandleBox(None, is_maybe=(base == "MaybeHandle"))
      if len(args) == 1:
        a0 = args[0]
        if isinstance(a0, HandleBox):
          return a0
        if a0 is None or isinstance(a0, NullTagged):
          return HandleBox(None, is_maybe=(base == "MaybeHandle"))
        return HandleBox(a0, is_maybe=(base == "MaybeHandle"))
      if len(args) == 2:
        return cxx_handle(args[0], args[1])
    if base in ("Tagged", "TaggedImpl", "StrongTaggedBase"):
      if not args:
        inner = ctor_type[ctor_type.find("<") +
                          1:-1] if "<" in ctor_type else "Object"
        return NullTagged(inner)
      return args[0]
    if base in ("std::unique_ptr", "unique_ptr"):
      if not args:
        return None
      # Models std::unique_ptr(std::unique_ptr&&): transfers ownership from the
      # moved-from lvalue without destroying the transferred object.
      if arg_lvs and arg_lvs[0] is not None:
        return arg_lvs[0].release()
      return args[0]
    if len(args) == 1 and node.get("is_copy_or_conv"):
      return cxx_copy_value(args[0])
    if not args:
      return self._default_construct_type(ctor_type)
    raise UnsupportedCxxAstError(
        f"Unsupported C++ constructor: {ctor_type!r} with {len(args)} args")

  def _eval_assign(self, node: Dict[str, Any], scopes: _ScopeStack,
                   this_obj: Any) -> Any:
    op = node["op"]
    lv = self._eval_lvalue(node["target"], scopes, this_obj)
    rhs = self._eval_expr(node["value"], scopes, this_obj)
    if op == "=":
      val = cxx_copy_value(rhs)
      lv.set(val)
      return lv.get()
    # Compound assignment: evaluate target lvalue ONCE, apply exact binary op!
    bin_op = op[:-1]  # "+=" -> "+", "<<=" -> "<<", etc.
    cur = lv.get()
    new_val = _apply_binary_op(bin_op, cur, rhs, lv.qual_type)
    lv.set(new_val)
    return lv.get()

  def _eval_unary(self, node: Dict[str, Any], scopes: _ScopeStack,
                  this_obj: Any) -> Any:
    op = node["op"]
    is_postfix = bool(node.get("is_postfix", False))

    if op == "&":
      lv = self._eval_lvalue(node["operand"], scopes, this_obj)
      return _PtrCell(lv)

    if op in ("++", "--"):
      delta = 1 if op == "++" else -1
      lv = self._eval_lvalue(node["operand"], scopes, this_obj)
      cur = lv.get()
      if isinstance(cur, CxxVectorIterator):
        if is_postfix:
          old_copy = cur.__cxx_copy__()
          cur.advance(delta)
          return old_copy
        cur.advance(delta)
        return cur
      old_val = cur
      new_val = coerce_cxx_scalar(cur + delta, lv.qual_type or
                                  node.get("qual_type", ""))
      lv.set(new_val)
      return old_val if is_postfix else lv.get()

    val = self._eval_expr(node["operand"], scopes, this_obj)
    if op == "!":
      return not cxx_truthy(val)
    if op == "bool":
      return cxx_truthy(val)
    if op == "-":
      return coerce_cxx_scalar(-val, node.get("qual_type", ""))
    if op == "+":
      return +val
    if op == "~":
      return coerce_cxx_scalar(~int(val), node.get("qual_type", ""))
    if op == "*":
      if isinstance(val, _PtrCell):
        return val.deref()
      if isinstance(val, HandleBox):
        return val.deref()
      if isinstance(val, CxxVectorIterator):
        return val.deref()
      raise CxxHarnessError(
          f"Cannot dereference value of type {type(val).__name__}: {val!r}")

    raise UnsupportedCxxAstError(f"Unsupported unary operator: {op!r}")

  def _eval_binop(self, node: Dict[str, Any], scopes: _ScopeStack,
                  this_obj: Any) -> Any:
    op = node["op"]
    # Strictly sequenced short-circuit operators (per C++17+).
    if op == "&&":
      left = self._eval_expr(node["left"], scopes, this_obj)
      if not cxx_truthy(left):
        return False
      return cxx_truthy(self._eval_expr(node["right"], scopes, this_obj))
    if op == "||":
      left = self._eval_expr(node["left"], scopes, this_obj)
      if cxx_truthy(left):
        return True
      return cxx_truthy(self._eval_expr(node["right"], scopes, this_obj))
    if op == ",":
      self._eval_expr(node["left"], scopes, this_obj)
      return self._eval_expr(node["right"], scopes, this_obj)

    left, right = self._eval_pair_ordered(node["left"], node["right"], scopes,
                                          this_obj)
    return _apply_binary_op(op, left, right, node.get("qual_type", ""))

  def _eval_template_call(self, node: Dict[str, Any], scopes: _ScopeStack,
                          this_obj: Any) -> Any:
    name = node["name"]
    targ = node["template_arg"]
    args, _ = self._eval_args_ordered(node["args"], scopes, this_obj)
    if name == "Is":
      return is_cxx_type(args[0], targ)
    if name in ("Cast", "TrustedCast", "SbxCast"):
      return cast_cxx_type(args[0], targ)
    if name == "TryCast":
      return try_cast_cxx_type(args[0], targ, args[1])
    raise UnsupportedCxxAstError(f"Unsupported template call: {name}<{targ}>")

  def _eval_call(self, node: Dict[str, Any], scopes: _ScopeStack,
                 this_obj: Any) -> Any:
    callee_node = node["callee"]
    arg_nodes = node.get("args", [])

    # Special-case DCHECK/CHECK macros so we can report failing expressions cleanly
    # and preserve C++ short-circuiting for DCHECK_IMPLIES!
    if callee_node.get("type") == "Name":
      fname = callee_node["name"]
      if fname in ("DCHECK_IMPLIES", "CHECK_IMPLIES") and len(arg_nodes) == 2:
        a = self._eval_expr(arg_nodes[0], scopes, this_obj)
        if not cxx_truthy(a):
          return None
        b = self._eval_expr(arg_nodes[1], scopes, this_obj)
        if not cxx_truthy(b):
          raise CxxDcheckFailure(f"C++ {fname} failed")
        return None

    # Method call on an object (with BoundMergeRuntime dispatch for parsed V8 methods!).
    if callee_node.get("type") == "Member":
      recv = (
          this_obj if callee_node.get("is_this_member") else self._eval_expr(
              callee_node["base"], scopes, this_obj))
      if callee_node.get("is_arrow") and isinstance(
          recv, (CxxVectorIterator, _PtrCell)):
        recv = recv.deref()
      mname = callee_node["member"]
      args, arg_lvs = self._eval_args_ordered(arg_nodes, scopes, this_obj)
      return self._dispatch_method_call(recv, mname, args, arg_lvs)

    # Free function or closure call.
    args, arg_lvs = self._eval_args_ordered(arg_nodes, scopes, this_obj)
    if callee_node.get(
        "type") == "Name" and callee_node["name"] in self.extra_functions:
      pm = self.extra_functions[callee_node["name"]]
      return self.execute_method(pm, args=args, arg_lvalues=arg_lvs)

    fn = self._eval_expr(callee_node, scopes, this_obj)
    if isinstance(fn, _CxxClosure):
      return fn(*args, arg_lvalues=arg_lvs)
    if not callable(fn):
      raise CxxHarnessError(
          f"Callee {callee_node!r} evaluated to non-callable {fn!r}")
    return fn(*args)

  def _dispatch_method_call(
      self,
      recv: Any,
      mname: str,
      args: List[Any],
      arg_lvs: List[Optional[_LValue]],
  ) -> Any:
    # HandleBox->Method(...) validates handle & unwraps target (except HandleBox's own methods).
    if isinstance(
        recv,
        HandleBox) and mname not in ("is_null", "ToHandleChecked", "ToHandle"):
      recv = recv.deref()

    # RawTaggedRef->Method(...) validates moving-GC epoch & publication!
    if isinstance(recv,
                  RawTaggedRef) and mname not in ("is_null", "ptr", "address"):
      recv = recv.deref()

    # Check if BoundMergeRuntime has a Clang-parsed C++ method for this receiver!
    if self.runtime is not None:
      if isinstance(recv, SharedFunctionInfo):
        pm = self.runtime.resolve_sfi_method(mname, args)
        if pm is not None:
          self._notify_method_enter("SharedFunctionInfo", mname, recv)
          ret = self.execute_method(
              pm, this_obj=recv, args=args, arg_lvalues=arg_lvs)
          self._notify_method_return("SharedFunctionInfo", mname, recv, ret)
          return ret
      elif isinstance(recv, ConstantPoolPointerForwarder):
        pm = self.runtime.resolve_forwarder_method(mname, args)
        if pm is not None:
          self._notify_method_enter("ConstantPoolPointerForwarder", mname, recv)
          ret = self.execute_method(
              pm, this_obj=recv, args=args, arg_lvalues=arg_lvs)
          self._notify_method_return("ConstantPoolPointerForwarder", mname,
                                     recv, ret)
          return ret

    if not hasattr(recv, mname):
      raise CxxHarnessError(
          f"No method {mname!r} on {type(recv).__name__} ({recv!r})")
    method = getattr(recv, mname)
    return method(*args)

  def _notify_method_enter(self, class_name: str, method_name: str,
                           recv: Any) -> None:
    if self.runtime is None:
      return
    mc = self.runtime.heap.mem_ctx
    if not mc.in_observer_callback:
      mc.in_observer_callback = True
      try:
        mc.observer.on_method_enter(class_name, method_name, recv)
      finally:
        mc.in_observer_callback = False

  def _notify_method_return(self, class_name: str, method_name: str, recv: Any,
                            ret_val: Any) -> None:
    if self.runtime is None:
      return
    mc = self.runtime.heap.mem_ctx
    if not mc.in_observer_callback:
      mc.in_observer_callback = True
      try:
        mc.observer.on_method_return(class_name, method_name, recv, ret_val)
      finally:
        mc.in_observer_callback = False

  def _eval_lambda(self, node: Dict[str, Any], scopes: _ScopeStack,
                   this_obj: Any) -> _CxxClosure:
    cap_frame = _ScopeFrame()
    for cap in node.get("captures", []):
      if cap.get("is_this"):
        continue
      cname = cap["name"]
      is_ref = bool(cap.get("by_ref", cap.get("is_ref", False)))
      init_expr = cap.get("init")
      if is_ref:
        if init_expr is not None:
          lv = self._eval_lvalue(init_expr, scopes, this_obj)
        else:
          cell = scopes.lookup_cell(cname)
          if cell is None:
            raise CxxHarnessError(f"Lambda by-ref capture {cname!r} not found")
          lv = cell.as_lvalue()
        cap_frame.declare(cname, None, ref_target=lv)
      else:
        val = (
            self._eval_expr(init_expr, scopes, this_obj)
            if init_expr is not None else scopes.lookup_cell(cname).get())
        cap_frame.declare(cname, cxx_copy_value(val))
    return _CxxClosure(self, node, cap_frame, this_obj)


# ---------------------------------------------------------------------------
# Overload Resolution & Isolated Per-Bundle Runtime (Zero Global Monkey-Patching!)
# ---------------------------------------------------------------------------


def _matches_overload(pm: ParsedMethod, args: List[Any]) -> int:
  """Returns match score (>0) if ``args`` fit ``pm``, else 0 (arity + type check)."""
  # Allow trailing defaulted params when C++ default args were stripped by Clang.
  if len(args) > len(pm.params):
    return 0
  # Require exact arity unless extra params are defaulted tag/WriteBarrierMode/Isolate.
  if len(args) < len(pm.params):
    for ptype, _pname, _is_ref in pm.params[len(args):]:
      pt = normalize_cxx_type_name(ptype)
      if pt not in ("WriteBarrierMode", "IsolateForSandbox", "AcquireLoadTag",
                    "ReleaseStoreTag", "RelaxedLoadTag", "RelaxedStoreTag"):
        return 0

  score = 100 - (len(pm.params) - len(args))
  for arg, (ptype, _pname, _is_ref) in zip(args, pm.params):
    pt = normalize_cxx_type_name(ptype)
    raw = unwrap_for_gc(arg)
    if pt == "AcquireLoadTag":
      if not isinstance(arg, AcquireLoadTag):
        return 0
      score += 20
    elif pt == "ReleaseStoreTag":
      if not isinstance(arg, ReleaseStoreTag):
        return 0
      score += 20
    elif pt == "RelaxedLoadTag":
      if not isinstance(arg, RelaxedLoadTag):
        return 0
      score += 20
    elif pt == "RelaxedStoreTag":
      if not isinstance(arg, RelaxedStoreTag):
        return 0
      score += 20
    elif pt == "MaybeObject":
      if isinstance(raw, MaybeObject):
        score += 20
      else:
        return 0
    elif pt == "SharedFunctionInfo":
      if isinstance(raw, SharedFunctionInfo):
        score += 20
      elif raw is not None:
        return 0
    elif pt == "ScopeInfo":
      if isinstance(raw, ScopeInfo):
        score += 20
      elif raw is not None:
        return 0
    elif pt == "BytecodeArray":
      if isinstance(raw, BytecodeArray):
        score += 20
      elif raw is not None:
        return 0
    elif pt == "FeedbackMetadata":
      if isinstance(raw, FeedbackMetadata):
        score += 20
      elif raw is not None:
        return 0
    elif pt == "ObjectBoilerplateDescription":
      if isinstance(raw, ObjectBoilerplateDescription):
        score += 20
      elif raw is not None:
        return 0
    elif pt == "FixedArray" or pt == "TrustedFixedArray":
      if isinstance(raw, FixedArray):
        score += 15
      elif raw is not None:
        return 0
    elif pt == "HeapObject":
      if isinstance(raw, HeapObject):
        score += 5
      elif isinstance(raw, MaybeObject):
        return 0
    elif pt == "Object":
      score += 1
  return score


def _select_best_overload(candidates: List[ParsedMethod],
                          args: List[Any]) -> Optional[ParsedMethod]:
  best_pm: Optional[ParsedMethod] = None
  best_score = 0
  for pm in candidates:
    sc = _matches_overload(pm, args)
    if sc > best_score:
      best_score = sc
      best_pm = pm
  return best_pm


class BoundMergeRuntime:
  """Isolated execution runtime binding one ``ParsedCxxBundle`` to one ``V8Heap``.

  Eliminates all global class monkey-patching so multiple bundles (e.g.
  mutated vs baseline in ``TEST 6``, or ``--git-rev`` comparisons) never
  interfere with each other.
  """

  def __init__(
      self,
      bundle: ParsedCxxBundle,
      heap: V8Heap,
      isolate: Isolate,
      local_isolate: LocalIsolate,
      *,
      flags: Optional[V8Flags] = None,
      ilp_eval_order: str = "ltr",
  ):
    self.bundle = bundle
    self.heap = heap
    self.isolate = isolate
    self.local_isolate = local_isolate
    self.flags = flags or V8Flags(verify_code_merge=False)
    self.interpreter = CxxAstInterpreter(self, ilp_eval_order=ilp_eval_order)

  def resolve_sfi_method(self, name: str,
                         args: List[Any]) -> Optional[ParsedMethod]:
    cands = self.bundle.sfi_methods.get(name)
    if not cands:
      return None
    return _select_best_overload(cands, args)

  def resolve_forwarder_method(self, name: str,
                               args: List[Any]) -> Optional[ParsedMethod]:
    cands = self.bundle.fwd_methods.get(name)
    if not cands:
      return None
    return _select_best_overload(cands, args)

  def call_sfi_method(self, sfi: Any, name: str, *args: Any) -> Any:
    """Calls a Clang-parsed ``SharedFunctionInfo`` method directly."""
    return self.interpreter._dispatch_method_call(sfi, name, list(args),
                                                  [None] * len(args))

  def call_verify_constant_pool_entry(self, entry: Any, script: Any,
                                      verify_scope_info_fn: Any) -> None:
    cands = self.bundle.vcm_helpers.get("VerifyConstantPoolEntry")
    if not cands:
      raise CxxHarnessError(
          "VerifyConstantPoolEntry not present in parsed bundle")
    pm = cands[0]
    self.interpreter.execute_method(
        pm, args=[entry, script, verify_scope_info_fn])

  def call_verify_code_merge(self, isolate: Isolate,
                             script_handle: HandleBox) -> None:
    if not self.bundle.vcm_stmts:
      raise CxxHarnessError("VerifyCodeMerge not present in parsed bundle")
    self.interpreter.execute_statements(
        self.bundle.vcm_stmts,
        params=[
            ("Isolate*", "isolate", False),
            ("DirectHandle<Script>", "script", False),
        ],
        args=[isolate, script_handle],
        method_name="VerifyCodeMerge",
    )

  def run_begin_merge_in_background(
      self,
      task: BackgroundMergeTask,
      new_script_handle: HandleBox,
  ) -> None:
    """Executes real parsed ``BackgroundMergeTask::BeginMergeInBackground``."""
    mc = self.heap.mem_ctx
    mc.begin_background_handoff()
    prev_thread = mc.switch_thread(mc.bg_thread)
    try:
      self.interpreter.execute_statements(
          self.bundle.bg_stmts,
          params=[
              ("LocalIsolate*", "isolate", False),
              ("DirectHandle<Script>", "new_script", False),
          ],
          this_obj=task,
          args=[self.local_isolate, new_script_handle],
          method_name="BeginMergeInBackground",
      )
    finally:
      mc.switch_thread(prev_thread)

  def run_complete_merge_in_foreground(
      self,
      task: BackgroundMergeTask,
      new_script_handle: HandleBox,
  ) -> Any:
    """Executes real parsed ``BackgroundMergeTask::CompleteMergeInForeground``.

    Returns the EXACT ``Handle<SharedFunctionInfo>`` returned by the C++ code
    without overwriting ``task.state_`` or discarding the return handle.
    """
    mc = self.heap.mem_ctx
    prev_thread = mc.switch_thread(mc.main_thread)
    try:
      return self.interpreter.execute_statements(
          self.bundle.fg_stmts,
          params=[
              ("Isolate*", "isolate", False),
              ("DirectHandle<Script>", "new_script", False),
          ],
          this_obj=task,
          args=[self.isolate, new_script_handle],
          method_name="CompleteMergeInForeground",
      )
    finally:
      mc.switch_thread(prev_thread)
