# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""V8 heap, C++11 happens-before memory model, handle lifetimes, and moving GC.

Models the runtime primitives executed by the Clang AST interpreter for
``BackgroundMergeTask``, ``ConstantPoolPointerForwarder``, ``VerifyCodeMerge``,
and ``SharedFunctionInfo`` methods:

* **Pure C++11 happens-before memory model**: Every heap/SFI field is backed by
  a ``MemoryLocation`` with per-write timestamps, atomic vs plain tags, and
  optional release views (``rel_view``). Acquire loads are non-SC (they may
  return stale writes when no happens-before edge forces a newer write), and
  they absorb ``rel_view`` only from the actually chosen write. Newly allocated
  objects initialize their ``_map`` with ``UNINITIALIZED_MEMORY`` before the
  constructor write so unsafe publication without release/acquire fails with
  ``UnsafePublicationError`` regardless of field name. Non-atomic races fail
  with ``DataRaceError``.
* **Moving GC & real handle lifetimes**: ``V8Heap`` tracks ``gc_epoch`` and
  separate ``LocalHeap`` instances for the main thread and background thread.
  Raw ``Tagged<HeapObject>`` locals (``RawTaggedRef`` / ``MaybeObject``) record
  ``gc_epoch`` at load time and raise ``RawPointerAcrossSafepointError`` if
  dereferenced after a moving GC. ``HandleScope`` / ``LocalHandleScope`` push
  and pop ``HandleFrame`` objects on exit (raising ``DanglingHandleError`` on
  escaped local handles). ``DisallowGarbageCollection`` maintains per-thread
  ``no_gc_depth`` (raising ``GcInNoGcScopeError`` if a safepoint/GC runs inside
  a no-GC scope). GC reachability traces only live handles, live
  ``PersistentHandles``, and explicitly rooted JS closures.
* **C++ value containers**: ``CxxVector``, ``CxxUnorderedMap``, and
  ``CxxUnorderedSet`` implement deep value-copy (``__cxx_copy__``), proper
  forward/reverse iterators with postfix/prefix increment, and mutable entry
  references for range-for by reference.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, FrozenSet, Iterable, List, Optional, Set, Tuple

from errors import (
    CxxDcheckFailure,
    DanglingHandleError,
    DataRaceError,
    DuplicateScopeInfoError,
    GcInNoGcScopeError,
    RawPointerAcrossSafepointError,
    UnsafePublicationError,
)

# ---------------------------------------------------------------------------
# Sentinels, memory ordering tags, and V8 enums
# ---------------------------------------------------------------------------


class _Sentinel:
  __slots__ = ("name",)

  def __init__(self, name: str):
    self.name = name

  def __repr__(self) -> str:
    return self.name


UNINITIALIZED_MEMORY = _Sentinel("UNINITIALIZED_MEMORY")


class MemoryOrder:
  PLAIN = "plain"
  RELAXED = "relaxed"
  ACQUIRE = "acquire"
  RELEASE = "release"


class AcquireLoadTag:
  pass


class ReleaseStoreTag:
  pass


class RelaxedLoadTag:
  pass


class RelaxedStoreTag:
  pass


kAcquireLoad = AcquireLoadTag()
kReleaseStore = ReleaseStoreTag()
kRelaxedLoad = RelaxedLoadTag()
kRelaxedStore = RelaxedStoreTag()


class WriteBarrierMode:
  SKIP_WRITE_BARRIER = 0
  UPDATE_WRITE_BARRIER = 1


SKIP_WRITE_BARRIER = WriteBarrierMode.SKIP_WRITE_BARRIER
UPDATE_WRITE_BARRIER = WriteBarrierMode.UPDATE_WRITE_BARRIER


class ScopeType:
  CLASS_SCOPE = 0
  EVAL_SCOPE = 1
  FUNCTION_SCOPE = 2
  MODULE_SCOPE = 3
  SCRIPT_SCOPE = 4
  CATCH_SCOPE = 5
  BLOCK_SCOPE = 6
  WITH_SCOPE = 7
  SHADOW_REALM_SCOPE = 8


SCRIPT_SCOPE = ScopeType.SCRIPT_SCOPE
FUNCTION_SCOPE = ScopeType.FUNCTION_SCOPE
MODULE_SCOPE = ScopeType.MODULE_SCOPE
EVAL_SCOPE = ScopeType.EVAL_SCOPE
BLOCK_SCOPE = ScopeType.BLOCK_SCOPE
WITH_SCOPE = ScopeType.WITH_SCOPE


class Builtin:
  kIllegal = 0
  kCompileLazy = 1
  kEmptyFunction = 2


class BackgroundMergeState:
  kNotStarted = 0
  kPendingBackgroundWork = 1
  kPendingForegroundWork = 2
  kDone = 3


kFunctionLiteralIdTopLevel = 0
kMaxInt = 0x7FFFFFFF
kInvalidInfoId = -1


class StdMemoryOrder:
  memory_order_relaxed = MemoryOrder.RELAXED
  memory_order_acquire = MemoryOrder.ACQUIRE
  memory_order_release = MemoryOrder.RELEASE
  memory_order_seq_cst = MemoryOrder.ACQUIRE


# ---------------------------------------------------------------------------
# Synchronous ExecutionObserver (replaces generator-based top-level yields)
# ---------------------------------------------------------------------------


class ExecutionObserver:
  """Synchronous hook called from the interpreter and heap on every event.

  Subclasses in ``model_checker.py`` use these callbacks to inject concurrent
  main-thread mutations at ``MERGE_MODEL_STEP*``, ``SYNCHRONIZATION_POINT*``,
  ``LocalHeap::Safepoint()``, between consecutive shared-heap reads inside SFI
  helpers, and to run concurrent background reader verification after every
  store inside ``SharedFunctionInfo::CopyFrom`` and foreground release steps.
  """

  def on_merge_step(self, step_name: str, allow_safepoint: bool) -> None:
    pass

  def on_sync_point(self, name: str, allow_safepoint: bool) -> None:
    pass

  def on_safepoint(self, local_heap: "LocalHeap") -> None:
    pass

  def on_heap_read(self, owner: Any, field_name: str, order: str) -> None:
    pass

  def on_heap_write(self, owner: Any, field_name: str, value: Any,
                    order: str) -> None:
    pass

  def on_method_enter(self, class_name: str, method_name: str,
                      recv: Any) -> None:
    pass

  def on_method_return(self, class_name: str, method_name: str, recv: Any,
                       ret_val: Any) -> None:
    pass

  def on_branch(self, tag: str) -> None:
    pass


# ---------------------------------------------------------------------------
# Pure C++11 Happens-Before Memory Model
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class WriteEntry:
  ts: int
  value: Any
  rel_view: Optional[FrozenSet[int]]
  is_atomic: bool
  writer_tid: str
  loc_name: str


class ThreadMemoryView:
  """Per-thread happens-before timestamp set and per-location coherence floor."""

  def __init__(self, tid: str, mem_ctx: "MemoryContext"):
    self.tid = tid
    self.mem_ctx = mem_ctx
    self.view: Set[int] = {0}
    self.coherence: Dict[int, int] = {}
    # Used by pure field-agnostic "per_load_mp" stress mode: records whether
    # this thread has already observed an unsynchronized newer write via a
    # non-synchronizing read (plain/relaxed load, or acquire load of a
    # non-release write) since its last successful release-acquire sync.
    self.mp_saw_unsync_non_acquire: bool = False
    self.mp_unsync_owner_id: Optional[int] = None

  def sync_from(self, other: "ThreadMemoryView") -> None:
    """Thread spawn/join synchronization: ``other`` happens-before ``self``."""
    self.view.update(other.view)
    for loc_id, idx in other.coherence.items():
      if idx > self.coherence.get(loc_id, 0):
        self.coherence[loc_id] = idx
    self.mp_saw_unsync_non_acquire = False
    self.mp_unsync_owner_id = None

  def reset_mp_window(self) -> None:
    self.mp_saw_unsync_non_acquire = False
    self.mp_unsync_owner_id = None

  def snapshot_for_concurrent_reader(self, tid: str) -> "ThreadMemoryView":
    """Creates a fresh concurrent reader view synchronized only with pre-BG init."""
    tv = ThreadMemoryView(tid, self.mem_ctx)
    tv.view = set(self.view)
    tv.coherence = dict(self.coherence)
    return tv


class MemoryContext:
  """Global C++11 memory model state shared by all threads in one trace."""

  VALID_MODES = ("latest", "oldest", "intermediate", "per_load_mp")

  def __init__(self, mode: str = "latest"):
    if mode not in self.VALID_MODES:
      raise ValueError(f"Unsupported memory mode: {mode!r}")
    self.mode: str = mode
    self._next_ts: int = 1
    self.main_thread = ThreadMemoryView("main", self)
    self.bg_thread = ThreadMemoryView("bg", self)
    self.concurrent_readers: List[ThreadMemoryView] = []
    self.active_thread: ThreadMemoryView = self.main_thread
    self.observer: ExecutionObserver = ExecutionObserver()
    # Prevent re-entrant observer notifications when an observer itself runs a
    # concurrent reader probe or mutation.
    self.in_observer_callback: bool = False
    # Optional debug counter of total atomic/plain ops.
    self.read_count: int = 0
    self.write_count: int = 0

  def alloc_ts(self) -> int:
    ts = self._next_ts
    self._next_ts += 1
    return ts

  def switch_thread(self, thread: ThreadMemoryView) -> ThreadMemoryView:
    prev = self.active_thread
    self.active_thread = thread
    return prev

  def begin_background_handoff(self) -> None:
    """Models PostTask happens-before: main -> bg."""
    self.bg_thread.sync_from(self.main_thread)

  def end_background_handoff(self) -> None:
    """Models background completion happens-before: bg -> main."""
    self.main_thread.sync_from(self.bg_thread)


class MemoryLocation:
  """Single memory location (tagged slot or primitive field) with C++11 history."""

  __slots__ = (
      "name",
      "mem_ctx",
      "history",
      "_last_plain_readers",
      "_last_atomic_readers",
  )

  def __init__(
      self,
      mem_ctx: MemoryContext,
      initial_value: Any,
      name: str,
      *,
      uninitialized_before_init: bool = False,
      writer: Optional[ThreadMemoryView] = None,
      is_atomic_init: bool = False,
  ):
    self.name = name
    self.mem_ctx = mem_ctx
    self.history: List[WriteEntry] = []
    # Maps reader_tid -> latest read timestamp on that thread (split by plain
    # vs atomic) for exact C++11 vector-clock Write-After-Read data race checks.
    self._last_plain_readers: Dict[str, int] = {}
    self._last_atomic_readers: Dict[str, int] = {}

    if uninitialized_before_init:
      w = writer or mem_ctx.active_thread
      # Entry 0 is uninitialized raw heap memory (before object constructor).
      self.history.append(
          WriteEntry(
              ts=0,
              value=UNINITIALIZED_MEMORY,
              rel_view=None,
              is_atomic=False,
              writer_tid="alloc",
              loc_name=name,
          ))
      ts = mem_ctx.alloc_ts()
      self.history.append(
          WriteEntry(
              ts=ts,
              value=initial_value,
              rel_view=None,
              is_atomic=is_atomic_init,
              writer_tid=w.tid,
              loc_name=name,
          ))
      w.view.add(ts)
      w.coherence[id(self)] = 1
    else:
      self.history.append(
          WriteEntry(
              ts=0,
              value=initial_value,
              rel_view=frozenset({0}),
              is_atomic=True,
              writer_tid="init",
              loc_name=name,
          ))

  def write(
      self,
      value: Any,
      order: str = MemoryOrder.PLAIN,
      *,
      thread: Optional[ThreadMemoryView] = None,
      owner: Any = None,
      field_name: str = "",
      notify: bool = True,
  ) -> None:
    t = thread or self.mem_ctx.active_thread
    is_atomic = order != MemoryOrder.PLAIN

    # Check Write-Write data race with any unsynchronized write from another
    # thread where at least one write is non-atomic.
    for w in self.history:
      if (w.writer_tid not in (t.tid, "init", "alloc") and
          w.ts not in t.view and (not is_atomic or not w.is_atomic)):
        raise DataRaceError(
            f"C++11 write-write data race on {self.name}: thread {t.tid!r} "
            f"({order}) races with thread {w.writer_tid!r} write ts={w.ts}")

    # Check Write-After-Read (WAR) data race with any prior read on another
    # thread that does not happen-before this write when either op is non-atomic.
    for rtid, r_ts in self._last_plain_readers.items():
      if rtid != t.tid and r_ts not in t.view:
        raise DataRaceError(
            f"C++11 write-after-read data race on {self.name}: thread {t.tid!r} "
            f"({order}) races with prior plain read on thread {rtid!r} (ts={r_ts})"
        )
    if not is_atomic:
      for rtid, r_ts in self._last_atomic_readers.items():
        if rtid != t.tid and r_ts not in t.view:
          raise DataRaceError(
              f"C++11 write-after-read data race on {self.name}: thread {t.tid!r} "
              f"(plain) races with prior atomic read on thread {rtid!r} (ts={r_ts})"
          )

    ts = self.mem_ctx.alloc_ts()
    # Release store captures the writer's current happens-before view INCLUDING
    # this store itself so acquiring this store makes both it and all prior
    # writes visible.
    t.view.add(ts)
    rel_view = frozenset(t.view) if order == MemoryOrder.RELEASE else None
    entry = WriteEntry(
        ts=ts,
        value=value,
        rel_view=rel_view,
        is_atomic=is_atomic,
        writer_tid=t.tid,
        loc_name=self.name,
    )
    self.history.append(entry)
    t.coherence[id(self)] = len(self.history) - 1
    self.mem_ctx.write_count += 1

    if notify and not self.mem_ctx.in_observer_callback:
      self.mem_ctx.in_observer_callback = True
      try:
        self.mem_ctx.observer.on_heap_write(owner, field_name or self.name,
                                            value, order)
      finally:
        self.mem_ctx.in_observer_callback = False

  def read(
      self,
      order: str = MemoryOrder.PLAIN,
      *,
      thread: Optional[ThreadMemoryView] = None,
      owner: Any = None,
      field_name: str = "",
      is_map_check: bool = False,
      notify: bool = True,
  ) -> Any:
    t = thread or self.mem_ctx.active_thread
    if notify and not is_map_check and not self.mem_ctx.in_observer_callback:
      self.mem_ctx.in_observer_callback = True
      try:
        self.mem_ctx.observer.on_heap_read(owner, field_name or self.name,
                                           order)
      finally:
        self.mem_ctx.in_observer_callback = False

    self.mem_ctx.read_count += 1
    loc_id = id(self)
    min_idx = t.coherence.get(loc_id, 0)
    last_idx = len(self.history) - 1

    # Advance min_idx to the latest write that happens-before thread `t`.
    for idx in range(min_idx, last_idx + 1):
      if self.history[idx].ts in t.view:
        min_idx = idx

    # Check Read-Write data race and unsafe publication:
    # Any write in history[min_idx:] from another thread that does NOT
    # happen-before `t` is concurrent with this read.
    for idx in range(min_idx, last_idx + 1):
      w = self.history[idx]
      if w.writer_tid in (t.tid, "init", "alloc"):
        continue
      if w.ts in t.view:
        continue
      if is_map_check and self.history[
          0].value is UNINITIALIZED_MEMORY and min_idx == 0:
        raise UnsafePublicationError(
            f"Unsafe publication: thread {t.tid!r} inspected {self.name} "
            f"without happens-before from publisher thread {w.writer_tid!r} "
            f"(write ts={w.ts} not in reader view)")
      if not w.is_atomic or order == MemoryOrder.PLAIN:
        raise DataRaceError(
            f"C++11 read-write data race on {self.name}: thread {t.tid!r} "
            f"({order}) races with thread {w.writer_tid!r} "
            f"({'atomic' if w.is_atomic else 'plain'} write ts={w.ts})")

    # Select which write index in [min_idx, last_idx] is observed.
    if min_idx == last_idx:
      chosen_idx = min_idx
    else:
      mode = self.mem_ctx.mode
      if mode == "latest":
        chosen_idx = last_idx
      elif mode == "oldest":
        # Non-SC: even an ACQUIRE load can legally observe min_idx when no
        # happens-before edge forces a newer write!
        chosen_idx = min_idx
      elif mode == "intermediate":
        chosen_idx = min_idx + 1 if (min_idx + 1) < last_idx else min_idx
      elif mode == "per_load_mp":
        if is_map_check:
          chosen_idx = min_idx
        elif order == MemoryOrder.ACQUIRE and self.history[
            last_idx].rel_view is not None:
          # Preserve same-object (or standalone owner=None) MP anomaly when a
          # prior unsynced non-acquire / non-release load flipped the MP flag,
          # while allowing genuine acquiring loads of release writes on other
          # objects to synchronize and unlatch the flag.
          cur_owner_id = id(owner) if owner is not None else None
          if (t.mp_saw_unsync_non_acquire and
              cur_owner_id == t.mp_unsync_owner_id):
            chosen_idx = min_idx
          else:
            chosen_idx = last_idx
        else:
          # Either a relaxed/plain load, or an acquire load reading a
          # non-release write (missing kReleaseStore on the writer).
          if not t.mp_saw_unsync_non_acquire:
            chosen_idx = last_idx
            t.mp_saw_unsync_non_acquire = True
            t.mp_unsync_owner_id = id(owner) if owner is not None else None
          else:
            chosen_idx = min_idx
      else:
        chosen_idx = last_idx

    chosen = self.history[chosen_idx]
    # Enforce per-location coherence (monotonic modification-order observation).
    if chosen_idx > t.coherence.get(loc_id, 0):
      t.coherence[loc_id] = chosen_idx

    # Acquire-Release synchronization happens ONLY if BOTH:
    # 1. This read uses ACQUIRE ordering, AND
    # 2. The ACTUALLY CHOSEN write used RELEASE ordering (has rel_view).
    if order == MemoryOrder.ACQUIRE and chosen.rel_view is not None:
      t.view.update(chosen.rel_view)
      # Clear the MP latch once an acquire load genuinely synchronizes a newer
      # release store.
      if chosen_idx > min_idx:
        t.mp_saw_unsync_non_acquire = False
        t.mp_unsync_owner_id = None

    if not is_map_check:
      read_ts = self.mem_ctx.alloc_ts()
      t.view.add(read_ts)
      if order == MemoryOrder.PLAIN:
        self._last_plain_readers[t.tid] = read_ts
      else:
        self._last_atomic_readers[t.tid] = read_ts

    if chosen.value is UNINITIALIZED_MEMORY:
      raise UnsafePublicationError(
          f"Read UNINITIALIZED_MEMORY from {self.name} on thread {t.tid!r} "
          f"(mode={self.mem_ctx.mode!r}, order={order!r})")
    return chosen.value

  def latest_raw(self) -> Any:
    """Returns latest stored value directly (for GC tracing / diagnostics only)."""
    return self.history[-1].value


# ---------------------------------------------------------------------------
# TaggedMember / TrustedPointerMember / AtomicPrimitiveMember wrappers
# ---------------------------------------------------------------------------


class TaggedMember:
  """Models V8 ``TaggedMember<T>`` (atomic relaxed load/store + Acquire/Release)."""

  __slots__ = ("_loc", "_owner", "_field_name")

  def __init__(self, loc: MemoryLocation, owner: Any, field_name: str):
    self._loc = loc
    self._owner = owner
    self._field_name = field_name

  def load(self) -> Any:
    # V8 TaggedMember::load() is relaxed atomic load (AsAtomicTagged::Relaxed_Load).
    val = self._loc.read(
        MemoryOrder.RELAXED, owner=self._owner, field_name=self._field_name)
    return wrap_raw_if_movable(val)

  def Acquire_Load(self) -> Any:
    val = self._loc.read(
        MemoryOrder.ACQUIRE, owner=self._owner, field_name=self._field_name)
    return wrap_raw_if_movable(val)

  def store(self,
            owner_or_val: Any,
            val: Any = _Sentinel("MISSING"),
            mode: Any = UPDATE_WRITE_BARRIER) -> None:
    # Supports both TaggedMember::store(this, value, mode) and store(value).
    actual_val = owner_or_val if isinstance(val, _Sentinel) else val
    raw = unwrap_raw(actual_val)
    self._loc.write(
        raw,
        MemoryOrder.RELAXED,
        owner=self._owner,
        field_name=self._field_name)

  def Release_Store(self,
                    owner_or_val: Any,
                    val: Any = _Sentinel("MISSING"),
                    mode: Any = UPDATE_WRITE_BARRIER) -> None:
    actual_val = owner_or_val if isinstance(val, _Sentinel) else val
    raw = unwrap_raw(actual_val)
    self._loc.write(
        raw,
        MemoryOrder.RELEASE,
        owner=self._owner,
        field_name=self._field_name)

  def latest_raw(self) -> Any:
    return self._loc.latest_raw()


class TrustedPointerMember:
  """Models V8 ``TrustedPointerMember<T, tag>`` on SharedFunctionInfo."""

  __slots__ = ("_loc", "_owner", "_field_name")

  def __init__(self, loc: MemoryLocation, owner: Any, field_name: str):
    self._loc = loc
    self._owner = owner
    self._field_name = field_name

  def Acquire_Load(self) -> Any:
    val = self._loc.read(
        MemoryOrder.ACQUIRE, owner=self._owner, field_name=self._field_name)
    return wrap_raw_if_movable(val)

  def Release_Store(self,
                    owner_or_val: Any,
                    val: Any = _Sentinel("MISSING"),
                    mode: Any = UPDATE_WRITE_BARRIER) -> None:
    actual_val = owner_or_val if isinstance(val, _Sentinel) else val
    raw = unwrap_raw(actual_val)
    self._loc.write(
        raw,
        MemoryOrder.RELEASE,
        owner=self._owner,
        field_name=self._field_name)

  def clear(self, _owner: Any = None) -> None:
    self._loc.write(
        None,
        MemoryOrder.RELAXED,
        owner=self._owner,
        field_name=self._field_name)

  def is_empty(self) -> bool:
    val = self._loc.read(
        MemoryOrder.RELAXED, owner=self._owner, field_name=self._field_name)
    return val is None

  def latest_raw(self) -> Any:
    return self._loc.latest_raw()


class AtomicPrimitiveMember:
  """Models ``std::atomic<T>`` fields on SharedFunctionInfo (age_, flags_, etc.)."""

  __slots__ = ("_loc", "_owner", "_field_name")

  def __init__(self, loc: MemoryLocation, owner: Any, field_name: str):
    self._loc = loc
    self._owner = owner
    self._field_name = field_name

  def load(self, order: str = MemoryOrder.RELAXED) -> Any:
    return self._loc.read(order, owner=self._owner, field_name=self._field_name)

  def store(self, value: Any, order: str = MemoryOrder.RELAXED) -> None:
    self._loc.write(
        value, order, owner=self._owner, field_name=self._field_name)

  def latest_raw(self) -> Any:
    return self._loc.latest_raw()


# ---------------------------------------------------------------------------
# RawTaggedRef (moving-GC staleness check) & HeapObject base
# ---------------------------------------------------------------------------


class Smi:
  """Models V8 ``Tagged<Smi>`` (unboxed integer, never moves during GC)."""

  __slots__ = ("_value",)

  def __init__(self, value: int):
    self._value = int(value)

  @staticmethod
  def FromInt(v: int) -> "Smi":
    return Smi(int(v))

  @staticmethod
  def FromEnum(v: int) -> "Smi":
    return Smi(int(v))

  @staticmethod
  def zero() -> "Smi":
    return Smi(0)

  def value(self) -> int:
    return self._value

  def IsSmi(self) -> bool:
    return True

  def IsWeak(self) -> bool:
    return False

  def is_null(self) -> bool:
    return False

  def ptr(self) -> int:
    return (self._value << 1)

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, Smi):
      return self._value == other._value
    if isinstance(other, int) and not isinstance(other, bool):
      return self._value == other
    return False

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)

  def __hash__(self) -> int:
    return hash(("Smi", self._value))

  def __repr__(self) -> str:
    return f"Smi({self._value})"


kNoSharedNameSentinel = Smi.zero()


class RawTaggedRef:
  """A raw C++ ``Tagged<T>`` local pointer tied to the ``gc_epoch`` at load time.

  If a moving GC executes while a raw ``Tagged<T>`` sits on the C++ stack
  (unrooted in a ``Handle`` / ``DirectHandle``), ``heap.gc_epoch`` advances and
  any subsequent dereference, cast, type check, or conversion to a handle raises
  ``RawPointerAcrossSafepointError``.
  """

  __slots__ = ("_obj", "_gc_epoch")

  def __init__(self, obj: "HeapObject", gc_epoch: int):
    self._obj = obj
    self._gc_epoch = gc_epoch

  def deref(self, *, check_published: bool = True) -> "HeapObject":
    obj = self._obj
    if obj is None:
      raise DanglingHandleError("Dereferenced null Tagged<T>")
    if not obj.is_read_only and obj.heap is not None:
      if self._gc_epoch != obj.heap.gc_epoch:
        raise RawPointerAcrossSafepointError(
            f"Raw Tagged<{type(obj).__name__}> ({obj}) loaded at gc_epoch="
            f"{self._gc_epoch} was used after moving GC advanced gc_epoch to "
            f"{obj.heap.gc_epoch}")
    if check_published:
      obj._check_published()
    return obj

  def is_null(self) -> bool:
    return self._obj is None

  def ptr(self) -> int:
    return id(self.deref())

  def address(self) -> Any:
    return self.deref().address()

  def IsSmi(self) -> bool:
    return False

  def IsWeak(self) -> bool:
    return False

  def __getattr__(self, name: str) -> Any:
    obj = self.deref()
    return getattr(obj, name)

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, RawTaggedRef):
      if self._obj is None or other._obj is None:
        return self._obj is other._obj
      # Comparing raw pointers after moving GC is also invalid if movable!
      return self.deref(check_published=False) is other.deref(
          check_published=False)
    if isinstance(other, HeapObject):
      if self._obj is None:
        return False
      return self.deref(check_published=False) is other
    if other is None:
      return self._obj is None
    return False

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)

  def __hash__(self) -> int:
    return id(self._obj)

  def __bool__(self) -> bool:
    return self._obj is not None

  def __repr__(self) -> str:
    return f"RawTagged({self._obj!r}@e{self._gc_epoch})"


class NullTagged:
  """Represents default-constructed null ``Tagged<T>()``."""

  __slots__ = ("type_name",)

  def __init__(self, type_name: str = "Object"):
    self.type_name = type_name

  def is_null(self) -> bool:
    return True

  def IsWeak(self) -> bool:
    return False

  def IsSmi(self) -> bool:
    return False

  def ptr(self) -> int:
    return 0

  def __bool__(self) -> bool:
    return False

  def __eq__(self, other: Any) -> bool:
    return isinstance(other, NullTagged) or other is None

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)

  def __repr__(self) -> str:
    return f"Tagged<{self.type_name}>(null)"


def wrap_raw_if_movable(val: Any) -> Any:
  """Wraps a movable HeapObject in a RawTaggedRef at the current gc_epoch."""
  if isinstance(val, RawTaggedRef):
    return val
  if isinstance(val, HeapObject):
    if val.is_read_only or val.heap is None:
      return val
    return RawTaggedRef(val, val.heap.gc_epoch)
  if isinstance(val, MaybeObject):
    # Refresh gc_epoch for a freshly loaded MaybeObject slot value.
    if val.is_weak and val.obj is not None and not val.obj.is_read_only and val.obj.heap is not None:
      return MaybeObject(val.obj, is_weak=True, gc_epoch=val.obj.heap.gc_epoch)
    if not val.is_weak and isinstance(
        val.obj,
        HeapObject) and not val.obj.is_read_only and val.obj.heap is not None:
      return MaybeObject(val.obj, is_weak=False, gc_epoch=val.obj.heap.gc_epoch)
  return val


def unwrap_raw(val: Any,
               *,
               check_epoch: bool = True,
               check_published: bool = True) -> Any:
  """Extracts the underlying HeapObject / Smi / primitive from RawTaggedRef/HandleBox."""
  if isinstance(val, RawTaggedRef):
    if check_epoch:
      return val.deref(check_published=check_published)
    return val._obj
  if isinstance(val, NullTagged):
    return None
  if isinstance(val, HeapObject) and check_published:
    val._check_published()
  return val


def unwrap_for_gc(val: Any) -> Any:
  """Non-checking unwrap used strictly by the GC tracer and diagnostics."""
  if isinstance(val, RawTaggedRef):
    return val._obj
  if isinstance(val, HandleBox):
    return val._obj
  if isinstance(val, NullTagged):
    return None
  return val


class HeapObject:
  """Base class for all V8 heap objects with ``_map`` publication tracking."""

  def __init__(
      self,
      heap: Optional["V8Heap"],
      map_name: str,
      *,
      is_read_only: bool = False,
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    self.heap = heap
    self.is_read_only = is_read_only
    self.freed = False
    self._map_name = map_name
    if heap is not None and not is_read_only:
      self._map_loc: Optional[MemoryLocation] = MemoryLocation(
          heap.mem_ctx,
          map_name,
          f"{map_name}#{id(self) & 0xffff:x}._map",
          uninitialized_before_init=newly_allocated,
          writer=writer,
          is_atomic_init=False,
      )
      heap.register_object(self)
    else:
      self._map_loc = None

  def _check_published(self) -> None:
    if self.freed:
      raise DanglingHandleError(
          f"Use-after-free: accessed garbage-collected {type(self).__name__} ({self})"
      )
    if self._map_loc is not None:
      self._map_loc.read(
          MemoryOrder.RELAXED,
          owner=self,
          field_name="_map",
          is_map_check=True,
          notify=False,
      )

  def is_null(self) -> bool:
    return False

  def IsSmi(self) -> bool:
    return False

  def IsWeak(self) -> bool:
    return False

  def ptr(self) -> int:
    return id(self)

  def _gc_children(self) -> Iterable[Any]:
    return ()


class MaybeObject:
  """Models V8 ``Tagged<MaybeObject>`` (strong, weak, or cleared weak ref)."""

  __slots__ = ("obj", "is_weak", "is_cleared", "gc_epoch")

  def __init__(
      self,
      obj: Any,
      *,
      is_weak: bool = False,
      is_cleared: bool = False,
      gc_epoch: Optional[int] = None,
  ):
    self.obj = unwrap_for_gc(obj) if obj is not None else None
    self.is_weak = bool(is_weak and not is_cleared and self.obj is not None)
    self.is_cleared = bool(is_cleared)
    if gc_epoch is not None:
      self.gc_epoch = gc_epoch
    elif isinstance(
        self.obj,
        HeapObject) and not self.obj.is_read_only and self.obj.heap is not None:
      self.gc_epoch = self.obj.heap.gc_epoch
    else:
      self.gc_epoch = -1

  @staticmethod
  def Weak(obj: Any) -> "MaybeObject":
    raw = unwrap_raw(obj)
    if raw is None:
      return MaybeObject.Cleared()
    if isinstance(raw, HeapObject) and raw.freed:
      raise DanglingHandleError(
          f"Use-after-free: MakeWeak() called on collected {type(raw).__name__} ({raw})"
      )
    ep = (
        raw.heap.gc_epoch if isinstance(raw, HeapObject) and
        not raw.is_read_only and raw.heap is not None else -1)
    return MaybeObject(raw, is_weak=True, gc_epoch=ep)

  @staticmethod
  def Strong(obj: Any) -> "MaybeObject":
    raw = unwrap_raw(obj)
    if isinstance(raw, HeapObject) and raw.freed:
      raise DanglingHandleError(
          f"Use-after-free: MaybeObject.Strong() called on collected {type(raw).__name__} ({raw})"
      )
    ep = (
        raw.heap.gc_epoch if isinstance(raw, HeapObject) and
        not raw.is_read_only and raw.heap is not None else -1)
    return MaybeObject(raw, is_weak=False, gc_epoch=ep)

  @staticmethod
  def Cleared() -> "MaybeObject":
    return MaybeObject(None, is_weak=False, is_cleared=True, gc_epoch=-1)

  def _check_epoch(self) -> None:
    if (isinstance(self.obj, HeapObject) and not self.obj.is_read_only and
        self.obj.heap is not None and self.gc_epoch >= 0 and
        self.gc_epoch != self.obj.heap.gc_epoch):
      raise RawPointerAcrossSafepointError(
          f"Raw Tagged<MaybeObject>({self.obj}) loaded at gc_epoch="
          f"{self.gc_epoch} used after moving GC advanced gc_epoch to "
          f"{self.obj.heap.gc_epoch}")

  def IsWeak(self) -> bool:
    self._check_epoch()
    return self.is_weak and not self.is_cleared and self.obj is not None

  def IsCleared(self) -> bool:
    return self.is_cleared

  def IsSmi(self) -> bool:
    return isinstance(self.obj, Smi)

  def GetHeapObjectAssumeWeak(self) -> Any:
    self._check_epoch()
    if not self.IsWeak():
      raise CxxDcheckFailure(
          "GetHeapObjectAssumeWeak() called on non-weak MaybeObject")
    self.obj._check_published()
    return wrap_raw_if_movable(self.obj)

  def GetHeapObjectIfWeak(self, out_ref: Any) -> bool:
    self._check_epoch()
    if self.IsWeak():
      self.obj._check_published()
      out_ref.set(wrap_raw_if_movable(self.obj))
      return True
    return False

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, MaybeObject):
      if self.is_cleared and other.is_cleared:
        return True
      if self.is_weak != other.is_weak or self.is_cleared != other.is_cleared:
        return False
      return self.obj is other.obj
    if not self.is_weak and not self.is_cleared:
      return self.obj == unwrap_for_gc(other)
    return False

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)

  def __repr__(self) -> str:
    if self.is_cleared:
      return "MaybeObject(CLEARED)"
    kind = "Weak" if self.is_weak else "Strong"
    return f"MaybeObject({kind}, {self.obj!r})"


def MakeWeak(obj: Any) -> MaybeObject:
  return MaybeObject.Weak(obj)


# ---------------------------------------------------------------------------
# ReadOnlyRoots Singletons & Core V8 Heap Objects
# ---------------------------------------------------------------------------


class ReadOnlySingleton(HeapObject):

  def __init__(self, name: str):
    super().__init__(None, name, is_read_only=True)
    self.name = name

  def __repr__(self) -> str:
    return self.name


class String(HeapObject):

  def __init__(
      self,
      heap: Optional["V8Heap"],
      text: str,
      *,
      is_read_only: bool = False,
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap,
        "String",
        is_read_only=is_read_only,
        newly_allocated=newly_allocated,
        writer=writer,
    )
    self.text = text

  def length(self) -> int:
    return len(self.text)

  def __eq__(self, other: Any) -> bool:
    raw = unwrap_for_gc(other)
    if isinstance(raw, String):
      return self.text == raw.text
    return False

  def __hash__(self) -> int:
    return hash(("String", self.text))

  def __repr__(self) -> str:
    return f"String({self.text!r})"


class FeedbackMetadata(HeapObject):

  def __init__(
      self,
      heap: "V8Heap",
      tag: str = "fb",
      *,
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap,
        "FeedbackMetadata",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    self.tag = tag

  def __repr__(self) -> str:
    return f"FeedbackMetadata({self.tag!r})"


class ScopeInfo(HeapObject):
  """Models V8 ``ScopeInfo`` (including singleton ``empty_scope_info``)."""

  def __init__(
      self,
      heap: Optional["V8Heap"],
      unique_id: int,
      *,
      scope_type: int = ScopeType.FUNCTION_SCOPE,
      context_length: int = 0,
      end_position: Optional[int] = None,
      outer_scope: Optional["ScopeInfo"] = None,
      function_name: Any = None,
      is_empty: bool = False,
      is_read_only: bool = False,
      label: str = "",
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap,
        "ScopeInfo",
        is_read_only=is_read_only,
        newly_allocated=newly_allocated,
        writer=writer,
    )
    self._unique_id = int(unique_id)
    self._scope_type = int(scope_type)
    self._context_length = int(context_length)
    self._start_position = int(unique_id)
    self._end_position = (
        int(end_position) if end_position is not None else int(unique_id) + 10)
    self._is_empty = bool(is_empty)
    self.label = label or f"si_{unique_id}"
    self._function_name = (
        function_name if function_name is not None else kNoSharedNameSentinel)
    if heap is not None and not is_read_only:
      self._outer_scope_loc: Optional[MemoryLocation] = MemoryLocation(
          heap.mem_ctx,
          outer_scope,
          f"ScopeInfo({self.label}).outer_scope_info",
          uninitialized_before_init=newly_allocated,
          writer=writer,
      )
    else:
      self._outer_scope_loc = None
      self._static_outer_scope = outer_scope

  def IsEmpty(self) -> bool:
    return self._is_empty

  def UniqueIdInScript(self) -> int:
    if self._is_empty:
      raise CxxDcheckFailure("UniqueIdInScript() called on empty ScopeInfo")
    return self._unique_id

  def scope_type(self) -> int:
    return self._scope_type

  def ContextLength(self) -> int:
    return self._context_length

  def StartPosition(self) -> int:
    return self._start_position

  def EndPosition(self) -> int:
    return self._end_position

  def HasOuterScopeInfo(self) -> bool:
    if self._is_empty:
      return False
    val = self._read_outer_raw()
    return val is not None and not (isinstance(val, ScopeInfo) and
                                    val.IsEmpty())

  def _read_outer_raw(self) -> Optional["ScopeInfo"]:
    if self._outer_scope_loc is not None:
      return self._outer_scope_loc.read(
          MemoryOrder.RELAXED, owner=self, field_name="outer_scope_info")
    return self._static_outer_scope

  def OuterScopeInfo(self) -> Any:
    val = self._read_outer_raw()
    if val is None:
      raise CxxDcheckFailure(
          f"OuterScopeInfo() called on {self} with no outer scope")
    return wrap_raw_if_movable(val)

  def set_outer_scope_info(
      self,
      value: Any,
      mode: Any = UPDATE_WRITE_BARRIER,
  ) -> None:
    raw = unwrap_raw(value)
    if raw is not None and not isinstance(raw, ScopeInfo):
      raise CxxDcheckFailure(
          f"set_outer_scope_info expected ScopeInfo, got {raw!r}")
    if self._outer_scope_loc is not None:
      self._outer_scope_loc.write(
          raw, MemoryOrder.RELAXED, owner=self, field_name="outer_scope_info")
    else:
      self._static_outer_scope = raw

  def FunctionName(self) -> Any:
    return self._function_name

  def SetFunctionName(self, name: Any) -> None:
    self._function_name = unwrap_raw(name)

  def _gc_children(self) -> Iterable[Any]:
    if self._outer_scope_loc is not None:
      outer = self._outer_scope_loc.latest_raw()
      if outer is not None:
        yield outer

  def __repr__(self) -> str:
    if self._is_empty:
      return "ScopeInfo(EMPTY)"
    return f"ScopeInfo({self.label}, uid={self._unique_id}, id={id(self) & 0xfff:x})"


class ReadOnlyRoots:
  """Singleton ReadOnlyRoots shared across the isolate (identity-preserving!)."""

  def __init__(self):
    self._empty_scope_info = ScopeInfo(
        None, -999, is_empty=True, is_read_only=True, label="EMPTY_SCOPE_INFO")
    self._the_hole = ReadOnlySingleton("THE_HOLE")
    self._undefined = ReadOnlySingleton("UNDEFINED")
    self._uninitialized_hole = ReadOnlySingleton("UNINITIALIZED_HOLE")
    self._empty_string = String(None, "", is_read_only=True)

  def empty_scope_info(self) -> ScopeInfo:
    return self._empty_scope_info

  def the_hole_value(self) -> ReadOnlySingleton:
    return self._the_hole

  def undefined_value(self) -> ReadOnlySingleton:
    return self._undefined

  def uninitialized_value(self) -> ReadOnlySingleton:
    return self._uninitialized_hole

  def empty_string(self) -> String:
    return self._empty_string


READ_ONLY_ROOTS = ReadOnlyRoots()
EMPTY_SCOPE_INFO = READ_ONLY_ROOTS.empty_scope_info()
THE_HOLE = READ_ONLY_ROOTS.the_hole_value()
UNDEFINED = READ_ONLY_ROOTS.undefined_value()
UNINITIALIZED_HOLE = READ_ONLY_ROOTS.uninitialized_value()


class SafeUnsignedInt:
  """Models V8 ``SafeHeapObjectSize`` returned by ``ulength()``."""

  __slots__ = ("_val",)

  def __init__(self, val: int):
    self._val = int(val)

  def value(self) -> int:
    return self._val

  def __int__(self) -> int:
    return self._val


class FixedArray(HeapObject):
  """Models V8 ``FixedArray`` / ``WeakFixedArray`` / ``TrustedFixedArray``."""

  def __init__(
      self,
      heap: "V8Heap",
      elements: List[Any],
      *,
      label: str = "FixedArray",
      is_weak_array: bool = False,
      is_trusted: bool = False,
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    map_name = ("TrustedFixedArray" if is_trusted else
                ("WeakFixedArray" if is_weak_array else "FixedArray"))
    super().__init__(
        heap, map_name, newly_allocated=newly_allocated, writer=writer)
    self.label = label
    self.is_weak_array = is_weak_array
    self.is_trusted = is_trusted
    if is_weak_array and heap is not None:
      heap.register_weak_array(self)
    self._slots: List[MemoryLocation] = [
        MemoryLocation(
            heap.mem_ctx,
            el,
            f"{label}[{i}]",
            uninitialized_before_init=newly_allocated,
            writer=writer,
        ) for i, el in enumerate(elements)
    ]

  def length(self) -> int:
    return len(self._slots)

  def ulength(self) -> SafeUnsignedInt:
    return SafeUnsignedInt(len(self._slots))

  def get(self, index: int, tag: Any = None) -> Any:
    idx = int(index)
    if idx < 0 or idx >= len(self._slots):
      raise CxxDcheckFailure(
          f"{self.label}.get({idx}) out of bounds (len={len(self._slots)})")
    if isinstance(tag, AcquireLoadTag):
      order = MemoryOrder.ACQUIRE
    else:
      # V8 FixedArray::get(i) / WeakFixedArray::get(i) without tag is relaxed atomic.
      order = MemoryOrder.RELAXED
    val = self._slots[idx].read(
        order, owner=self, field_name=f"{self.label}[{idx}]")
    return wrap_raw_if_movable(val)

  def set(self, index: int, value: Any, *rest: Any) -> None:
    idx = int(index)
    if idx < 0 or idx >= len(self._slots):
      raise CxxDcheckFailure(
          f"{self.label}.set({idx}) out of bounds (len={len(self._slots)})")
    order = MemoryOrder.RELAXED
    for arg in rest:
      if isinstance(arg, ReleaseStoreTag):
        order = MemoryOrder.RELEASE
      elif isinstance(arg, RelaxedStoreTag):
        order = MemoryOrder.RELAXED
    raw = unwrap_raw(value)
    if isinstance(raw, HeapObject) and raw.freed:
      raise DanglingHandleError(
          f"Use-after-free: storing collected {type(raw).__name__} ({raw}) into {self.label}[{idx}]"
      )
    if isinstance(raw, MaybeObject) and isinstance(
        raw.obj, HeapObject) and raw.obj.freed:
      raise DanglingHandleError(
          f"Use-after-free: storing MaybeObject referencing collected "
          f"{type(raw.obj).__name__} ({raw.obj}) into {self.label}[{idx}]")
    self._slots[idx].write(
        raw, order, owner=self, field_name=f"{self.label}[{idx}]")

  def get_latest_raw(self, index: int) -> Any:
    return self._slots[int(index)].latest_raw()

  def _gc_children(self) -> Iterable[Any]:
    # WeakFixedArray slots (script->infos()) are WEAK and do NOT strongly root!
    if self.is_weak_array:
      return
    for slot in self._slots:
      val = slot.latest_raw()
      if isinstance(val, MaybeObject):
        if not val.is_weak and not val.is_cleared and val.obj is not None:
          yield val.obj
      elif isinstance(val, HeapObject):
        yield val

  def __repr__(self) -> str:
    return f"{self._map_name}({self.label}, len={len(self._slots)})"


class ObjectBoilerplateDescription(HeapObject):
  """Models V8 ``ObjectBoilerplateDescription`` inside constant pools."""

  def __init__(
      self,
      heap: "V8Heap",
      values: List[Any],
      *,
      label: str = "Boilerplate",
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap,
        "ObjectBoilerplateDescription",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    self.label = label
    self._values: List[MemoryLocation] = [
        MemoryLocation(
            heap.mem_ctx,
            v,
            f"{label}.value[{i}]",
            uninitialized_before_init=newly_allocated,
            writer=writer,
        ) for i, v in enumerate(values)
    ]

  def boilerplate_properties_count(self) -> int:
    return len(self._values)

  def value(self, idx: int) -> Any:
    val = self._values[int(idx)].read(
        MemoryOrder.RELAXED, owner=self, field_name=f"value[{idx}]")
    return wrap_raw_if_movable(val)

  def set_value(self, idx: int, val: Any) -> None:
    raw = unwrap_raw(val)
    self._values[int(idx)].write(
        raw, MemoryOrder.RELAXED, owner=self, field_name=f"value[{idx}]")

  def _gc_children(self) -> Iterable[Any]:
    for loc in self._values:
      val = loc.latest_raw()
      if isinstance(val, HeapObject):
        yield val

  def __repr__(self) -> str:
    return f"ObjectBoilerplateDescription({self.label}, count={len(self._values)})"


class BytecodeArray(HeapObject):
  """Models V8 ``BytecodeArray`` with a ``TrustedFixedArray`` constant pool."""

  def __init__(
      self,
      heap: "V8Heap",
      constant_pool: FixedArray,
      *,
      label: str = "BC",
      has_source_position_table: bool = True,
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap, "BytecodeArray", newly_allocated=newly_allocated, writer=writer)
    self.label = label
    self.has_source_position_table = has_source_position_table
    self._cp_loc = MemoryLocation(
        heap.mem_ctx,
        constant_pool,
        f"{label}.constant_pool",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )

  def constant_pool(self) -> Any:
    val = self._cp_loc.read(
        MemoryOrder.RELAXED, owner=self, field_name="constant_pool")
    return wrap_raw_if_movable(val)

  def HasSourcePositionTable(self) -> bool:
    return self.has_source_position_table

  def _gc_children(self) -> Iterable[Any]:
    cp = self._cp_loc.latest_raw()
    if isinstance(cp, HeapObject):
      yield cp

  def __repr__(self) -> str:
    return f"BytecodeArray({self.label})"


class Script(HeapObject):
  """Models V8 ``Script`` with ``WeakFixedArray`` ``infos()``."""

  def __init__(
      self,
      heap: "V8Heap",
      infos: FixedArray,
      *,
      label: str = "Script",
  ):
    super().__init__(heap, "Script")
    self.label = label
    self._infos = infos
    self.line_ends_initialized = False

  def infos(self) -> Any:
    return wrap_raw_if_movable(self._infos)

  def infos_raw(self) -> FixedArray:
    return self._infos

  @staticmethod
  def InitLineEnds(isolate: "Isolate", script_handle: Any) -> None:
    # Real V8 Script::InitLineEnds allocates on the main-thread heap (safepoint
    # + potential moving GC). Any raw Tagged<> held across this call MUST fail!
    isolate.v8_heap.run_gc(check_thread="main", advance_epoch=True)
    s = unwrap_raw(script_handle.deref() if isinstance(script_handle, HandleBox
                                                      ) else script_handle)
    if isinstance(s, Script):
      s.line_ends_initialized = True

  def _gc_children(self) -> Iterable[Any]:
    yield self._infos

  def __repr__(self) -> str:
    return f"Script({self.label})"


# ---------------------------------------------------------------------------
# SharedFunctionInfo (matching real V8 field layout + memcmp for CopyFrom)
# ---------------------------------------------------------------------------


class SfiAddress:
  """Models ``SharedFunctionInfo::address() + offset`` for ``#if DEBUG`` ``memcmp``."""

  __slots__ = ("sfi", "offset")

  def __init__(self, sfi: "SharedFunctionInfo", offset: int = 0):
    self.sfi = sfi
    self.offset = int(offset)

  def __add__(self, delta: int) -> "SfiAddress":
    return SfiAddress(self.sfi, self.offset + int(delta))

  def __radd__(self, delta: int) -> "SfiAddress":
    return SfiAddress(self.sfi, self.offset + int(delta))


# Real V8 field byte layout from src/objects/shared-function-info.h:1026-1050.
SFI_FIELD_LAYOUT: Tuple[Tuple[str, int, int], ...] = (
    ("trusted_function_data_", 0, 4),
    ("untrusted_function_data_", 4, 4),
    ("name_or_scope_info_", 8, 4),
    ("outer_scope_info_or_feedback_metadata_", 12, 4),
    ("script_", 16, 4),
    ("length_", 20, 2),
    ("formal_parameter_count_", 22, 2),
    ("function_token_offset_", 24, 2),
    ("expected_nof_properties_", 26, 1),
    ("flags2_", 27, 1),
    ("flags_", 28, 4),
    ("function_literal_id_", 32, 4),
    ("unique_id_", 36, 4),
    ("age_", 40, 2),
    ("feedback_slot_", 42, 2),
)
SFI_OFFSETS: Dict[str, int] = {name: off for name, off, _ in SFI_FIELD_LAYOUT}
SFI_AGE_SIZE = 2
SFI_TOTAL_SIZE = 44


class SharedFunctionInfo(HeapObject):
  """Models V8 ``SharedFunctionInfo`` with exact field atomicity & layout.

  Parsed C++ methods from ``shared-function-info-inl.h`` and
  ``shared-function-info.cc`` (including ``CopyFrom``) execute directly against
  these members via ``BoundMergeRuntime``.
  """

  kAgeSize = SFI_AGE_SIZE
  kSize = SFI_TOTAL_SIZE

  def __init__(
      self,
      heap: "V8Heap",
      fn_id: int,
      script: Optional[Script],
      *,
      name_or_scope_info: Any = None,
      outer_scope_or_fb: Any = None,
      bytecode: Optional[BytecodeArray] = None,
      unique_id_in_script: Optional[int] = None,
      start_position: Optional[int] = None,
      end_position: int = 100,
      length: int = 10,
      formal_parameter_count: int = 1,
      function_token_offset: int = 0,
      expected_nof_properties: int = 0,
      flags: int = 1,
      flags2: int = 0,
      unique_id: Optional[int] = None,
      age: int = 0,
      feedback_slot: int = 0,
      has_debug_info: bool = False,
      needs_source_positions: bool = False,
      label: str = "",
      newly_allocated: bool = False,
      writer: Optional[ThreadMemoryView] = None,
  ):
    super().__init__(
        heap,
        "SharedFunctionInfo",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    self.label = label or f"SFI(fn={fn_id})"
    self._has_debug_info = has_debug_info
    self._needs_source_positions = needs_source_positions
    self._start_position = (
        start_position if start_position is not None else (fn_id * 100))
    self._end_position = end_position
    self._explicit_uid_in_script = unique_id_in_script
    self.source_positions_ensured = False

    mc = heap.mem_ctx
    init_name = (
        name_or_scope_info
        if name_or_scope_info is not None else kNoSharedNameSentinel)
    init_outer = outer_scope_or_fb if outer_scope_or_fb is not None else THE_HOLE
    init_trusted = bytecode
    init_untrusted = (
        Smi.FromInt(-1)
        if bytecode is not None else Smi.FromEnum(Builtin.kCompileLazy))

    # Tagged / Trusted members
    self._trusted_loc = MemoryLocation(
        mc,
        init_trusted,
        f"{self.label}.trusted_function_data_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._untrusted_loc = MemoryLocation(
        mc,
        init_untrusted,
        f"{self.label}.untrusted_function_data_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._name_or_scope_loc = MemoryLocation(
        mc,
        unwrap_for_gc(init_name),
        f"{self.label}.name_or_scope_info_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._outer_or_fb_loc = MemoryLocation(
        mc,
        unwrap_for_gc(init_outer),
        f"{self.label}.outer_scope_info_or_feedback_metadata_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._script_loc = MemoryLocation(
        mc,
        script if script is not None else UNDEFINED,
        f"{self.label}.script_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )

    self.trusted_function_data_ = TrustedPointerMember(
        self._trusted_loc, self, "trusted_function_data_")
    self.untrusted_function_data_ = TaggedMember(self._untrusted_loc, self,
                                                 "untrusted_function_data_")
    self.name_or_scope_info_ = TaggedMember(self._name_or_scope_loc, self,
                                            "name_or_scope_info_")
    self.outer_scope_info_or_feedback_metadata_ = TaggedMember(
        self._outer_or_fb_loc, self, "outer_scope_info_or_feedback_metadata_")
    self.script_ = TaggedMember(self._script_loc, self, "script_")

    # Plain non-atomic primitive fields (real V8 layout!)
    self._length_loc = MemoryLocation(
        mc,
        int(length) & 0xFFFF,
        f"{self.label}.length_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._formal_param_loc = MemoryLocation(
        mc,
        int(formal_parameter_count) & 0xFFFF,
        f"{self.label}.formal_parameter_count_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._token_offset_loc = MemoryLocation(
        mc,
        int(function_token_offset) & 0xFFFF,
        f"{self.label}.function_token_offset_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._expected_props_loc = MemoryLocation(
        mc,
        int(expected_nof_properties) & 0xFF,
        f"{self.label}.expected_nof_properties_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._flags2_loc = MemoryLocation(
        mc,
        int(flags2) & 0xFF,
        f"{self.label}.flags2_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._unique_id_loc = MemoryLocation(
        mc,
        int(unique_id if unique_id is not None else fn_id),
        f"{self.label}.unique_id_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )

    # Atomic relaxed primitive fields (real V8 layout!)
    self._flags_loc = MemoryLocation(
        mc,
        int(flags),
        f"{self.label}.flags_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._fn_id_loc = MemoryLocation(
        mc,
        int(fn_id),
        f"{self.label}.function_literal_id_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._age_loc = MemoryLocation(
        mc,
        int(age) & 0xFFFF,
        f"{self.label}.age_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self._feedback_slot_loc = MemoryLocation(
        mc,
        int(feedback_slot) & 0xFFFF,
        f"{self.label}.feedback_slot_",
        uninitialized_before_init=newly_allocated,
        writer=writer,
    )
    self.flags_ = AtomicPrimitiveMember(self._flags_loc, self, "flags_")
    self.function_literal_id_ = AtomicPrimitiveMember(self._fn_id_loc, self,
                                                      "function_literal_id_")
    self.age_ = AtomicPrimitiveMember(self._age_loc, self, "age_")
    self.feedback_slot_ = AtomicPrimitiveMember(self._feedback_slot_loc, self,
                                                "feedback_slot_")

  # --- Low-level C++ accessors (exact V8 memory ordering!) ---

  def name_or_scope_info(self, tag: Any = None) -> Any:
    if isinstance(tag, AcquireLoadTag):
      return self.name_or_scope_info_.Acquire_Load()
    return self.name_or_scope_info_.load()

  def set_name_or_scope_info(self, value: Any, *rest: Any) -> None:
    is_release = any(isinstance(a, ReleaseStoreTag) for a in rest)
    if is_release:
      self.name_or_scope_info_.Release_Store(self, value)
    else:
      self.name_or_scope_info_.store(self, value)

  def raw_outer_scope_info_or_feedback_metadata(self, tag: Any = None) -> Any:
    if isinstance(tag, AcquireLoadTag):
      return self.outer_scope_info_or_feedback_metadata_.Acquire_Load()
    # Real V8 shared-function-info-inl.h:243-251: 0-arg is relaxed/plain load!
    return self.outer_scope_info_or_feedback_metadata_.load()

  def outer_scope_info_or_feedback_metadata(self) -> Any:
    return self.outer_scope_info_or_feedback_metadata_.load()

  def set_raw_outer_scope_info_or_feedback_metadata(
      self, value: Any, mode: Any = UPDATE_WRITE_BARRIER) -> None:
    if isinstance(mode, ReleaseStoreTag):
      raise CxxDcheckFailure(
          "set_raw_outer_scope_info_or_feedback_metadata() is relaxed-only "
          "(use set_feedback_metadata(..., kReleaseStore) for release stores)")
    self.outer_scope_info_or_feedback_metadata_.store(self, value, mode)

  def set_outer_scope_info_or_feedback_metadata(self,
                                                value: Any,
                                                mode: Any = UPDATE_WRITE_BARRIER
                                               ) -> None:
    if isinstance(mode, ReleaseStoreTag):
      raise CxxDcheckFailure(
          "set_outer_scope_info_or_feedback_metadata() is relaxed-only "
          "(use set_feedback_metadata(..., kReleaseStore) for release stores)")
    self.outer_scope_info_or_feedback_metadata_.store(self, value, mode)

  def script(self, tag: Any = None) -> Any:
    if isinstance(tag, RelaxedLoadTag):
      return self.script_.load()
    # Real V8 DEF_GETTER(SharedFunctionInfo, script) delegates to script(kAcquireLoad).
    return self.script_.Acquire_Load()

  def set_script(self, value: Any, *rest: Any) -> None:
    is_release = any(isinstance(a, ReleaseStoreTag) for a in rest)
    if is_release:
      self.script_.Release_Store(self, value)
    else:
      self.script_.store(self, value)

  def GetTrustedData(self, _isolate: Any = None) -> Any:
    return self.trusted_function_data_.Acquire_Load()

  def GetUntrustedData(self) -> Any:
    return self.untrusted_function_data_.Acquire_Load()

  def HasTrustedData(self) -> bool:
    return not self.trusted_function_data_.is_empty()

  def HasUntrustedData(self) -> bool:
    return not self.HasTrustedData()

  def HasUncompiledData(self, _isolate: Any = None) -> bool:
    return False

  def HasBytecodeArray(self) -> bool:
    val = self.trusted_function_data_.Acquire_Load()
    return val is not None and isinstance(unwrap_for_gc(val), BytecodeArray)

  def GetBytecodeArray(self, _isolate: Any = None) -> Any:
    val = self.trusted_function_data_.Acquire_Load()
    raw = unwrap_raw(val)
    if not isinstance(raw, BytecodeArray):
      raise CxxDcheckFailure(
          f"GetBytecodeArray() on {self} without BytecodeArray (got {raw!r})")
    return wrap_raw_if_movable(raw)

  def SetScopeInfo(self,
                   scope_info: Any,
                   mode: Any = UPDATE_WRITE_BARRIER) -> None:
    raw_si = unwrap_raw(scope_info)
    if not isinstance(raw_si, ScopeInfo):
      raise CxxDcheckFailure(f"SetScopeInfo expected ScopeInfo, got {raw_si!r}")
    self.name_or_scope_info_.Release_Store(self, raw_si, mode)

  # Plain primitive accessors
  def length(self) -> int:
    return self._length_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="length_")

  def set_length(self, v: int) -> None:
    self._length_loc.write(
        int(v) & 0xFFFF, MemoryOrder.PLAIN, owner=self, field_name="length_")

  def formal_parameter_count(self) -> int:
    return self._formal_param_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="formal_parameter_count_")

  def set_formal_parameter_count(self, v: int) -> None:
    self._formal_param_loc.write(
        int(v) & 0xFFFF,
        MemoryOrder.PLAIN,
        owner=self,
        field_name="formal_parameter_count_")

  def function_token_offset(self) -> int:
    return self._token_offset_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="function_token_offset_")

  def set_function_token_offset(self, v: int) -> None:
    self._token_offset_loc.write(
        int(v) & 0xFFFF,
        MemoryOrder.PLAIN,
        owner=self,
        field_name="function_token_offset_")

  def expected_nof_properties(self) -> int:
    return self._expected_props_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="expected_nof_properties_")

  def set_expected_nof_properties(self, v: int) -> None:
    self._expected_props_loc.write(
        int(v) & 0xFF,
        MemoryOrder.PLAIN,
        owner=self,
        field_name="expected_nof_properties_")

  def flags2(self) -> int:
    return self._flags2_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="flags2_")

  def set_flags2(self, v: int) -> None:
    self._flags2_loc.write(
        int(v) & 0xFF, MemoryOrder.PLAIN, owner=self, field_name="flags2_")

  def unique_id(self) -> int:
    return self._unique_id_loc.read(
        MemoryOrder.PLAIN, owner=self, field_name="unique_id_")

  def set_unique_id(self, v: int) -> None:
    self._unique_id_loc.write(
        int(v), MemoryOrder.PLAIN, owner=self, field_name="unique_id_")

  # Atomic relaxed primitive accessors
  def flags(self, _tag: Any = None) -> int:
    return self.flags_.load(MemoryOrder.RELAXED)

  def set_flags(self, v: int, _tag: Any = None) -> None:
    self.flags_.store(int(v), MemoryOrder.RELAXED)

  def function_literal_id(self, _tag: Any = None) -> int:
    return self.function_literal_id_.load(MemoryOrder.RELAXED)

  def set_function_literal_id(self, v: int, _tag: Any = None) -> None:
    self.function_literal_id_.store(int(v), MemoryOrder.RELAXED)

  def age(self) -> int:
    return self.age_.load(MemoryOrder.RELAXED)

  def set_age(self, v: int) -> None:
    self.age_.store(int(v) & 0xFFFF, MemoryOrder.RELAXED)

  def feedback_slot(self) -> int:
    return self.feedback_slot_.load(MemoryOrder.RELAXED)

  def set_feedback_slot(self, v: int) -> None:
    self.feedback_slot_.store(int(v) & 0xFFFF, MemoryOrder.RELAXED)

  def HasDebugInfo(self, _isolate: Any = None) -> bool:
    return self._has_debug_info

  def EndPosition(self) -> int:
    return self._end_position

  def UniqueIdInScript(self) -> int:
    if self._explicit_uid_in_script is not None:
      return self._explicit_uid_in_script
    fn_id = self.function_literal_id(kRelaxedLoad)
    if fn_id == kFunctionLiteralIdTopLevel:
      return -2
    return self._start_position

  def address(self) -> SfiAddress:
    return SfiAddress(self, 0)

  def _field_value_for_memcmp(self, field_name: str) -> Any:
    mapping = {
        "trusted_function_data_": self._trusted_loc,
        "untrusted_function_data_": self._untrusted_loc,
        "name_or_scope_info_": self._name_or_scope_loc,
        "outer_scope_info_or_feedback_metadata_": self._outer_or_fb_loc,
        "script_": self._script_loc,
        "length_": self._length_loc,
        "formal_parameter_count_": self._formal_param_loc,
        "function_token_offset_": self._token_offset_loc,
        "expected_nof_properties_": self._expected_props_loc,
        "flags2_": self._flags2_loc,
        "flags_": self._flags_loc,
        "function_literal_id_": self._fn_id_loc,
        "unique_id_": self._unique_id_loc,
        "age_": self._age_loc,
        "feedback_slot_": self._feedback_slot_loc,
    }
    return mapping[field_name].latest_raw()

  @staticmethod
  def EnsureSourcePositionsAvailable(isolate: "Isolate",
                                     sfi_handle: Any) -> None:
    # Real V8 EnsureSourcePositionsAvailable allocates source position tables
    # on the main-thread heap (safepoint + potential moving GC).
    isolate.v8_heap.run_gc(check_thread="main", advance_epoch=True)
    raw = unwrap_raw(
        sfi_handle.deref() if isinstance(sfi_handle, HandleBox) else sfi_handle)
    if isinstance(raw, SharedFunctionInfo):
      raw.source_positions_ensured = True

  def _gc_children(self) -> Iterable[Any]:
    for loc in (
        self._trusted_loc,
        self._untrusted_loc,
        self._name_or_scope_loc,
        self._outer_or_fb_loc,
        self._script_loc,
    ):
      val = loc.latest_raw()
      if isinstance(val, HeapObject):
        yield val

  def __repr__(self) -> str:
    return f"{self.label}#{id(self) & 0xfff:x}"


def cxx_memcmp(addr_a: Any, addr_b: Any, size: int) -> int:
  """Faithful ``memcmp`` for ``SharedFunctionInfo::CopyFrom`` ``#if DEBUG`` check."""
  if not isinstance(addr_a, SfiAddress) or not isinstance(addr_b, SfiAddress):
    raise CxxDcheckFailure(
        f"memcmp expected SfiAddress, got {addr_a!r}, {addr_b!r}")
  start_a = addr_a.offset
  end_a = start_a + int(size)
  start_b = addr_b.offset
  if start_a != start_b:
    return 1
  for fname, foff, fsz in SFI_FIELD_LAYOUT:
    fend = foff + fsz
    if fend <= start_a or foff >= end_a:
      continue
    va = addr_a.sfi._field_value_for_memcmp(fname)
    vb = addr_b.sfi._field_value_for_memcmp(fname)
    if isinstance(va, HeapObject) or isinstance(vb, HeapObject):
      if va is not vb:
        return 1
    elif va != vb:
      return 1
  return 0


# ---------------------------------------------------------------------------
# Handles, HandleScopes, PersistentHandles, LocalHeap, Isolate, and Moving GC
# ---------------------------------------------------------------------------


class HandleFrame:
  """One active ``HandleScope`` or ``LocalHandleScope`` stack frame."""

  def __init__(self, local_heap: "LocalHeap"):
    self.local_heap = local_heap
    self.closed = False
    self.handles: List["HandleBox"] = []

  def close(self) -> None:
    self.closed = True


class PersistentHandles:
  """Models V8 ``PersistentHandles`` container rooted across BG->FG handoff."""

  def __init__(self, isolate: Any = None):
    self.isolate = isolate
    self.handles: List["HandleBox"] = []
    self.destroyed = False
    self.attached_to: Optional["LocalHeap"] = None

  def destroy(self) -> None:
    """Destroys this PersistentHandles block (models ``~PersistentHandles()``)."""
    self.destroyed = True
    self.attached_to = None

  def NewHandle(self, obj: Any) -> "HandleBox":
    if self.destroyed:
      raise DanglingHandleError(
          "NewHandle called on destroyed PersistentHandles")
    raw = unwrap_raw(obj)
    box = HandleBox(raw, persistent=self)
    self.handles.append(box)
    return box

  def Contains(self, box: "HandleBox") -> bool:
    return box in self.handles


class HandleBox:
  """Models V8 ``Handle<T>`` / ``DirectHandle<T>`` / ``IndirectHandle<T>`` / ``MaybeHandle<T>``."""

  __slots__ = ("_obj", "_frame", "_persistent", "is_maybe")

  def __init__(
      self,
      obj: Any,
      *,
      frame: Optional[HandleFrame] = None,
      persistent: Optional[PersistentHandles] = None,
      is_maybe: bool = False,
  ):
    self._obj = unwrap_for_gc(obj) if obj is not None else None
    self._frame = frame
    self._persistent = persistent
    self.is_maybe = is_maybe

  def _validate(self) -> None:
    if self._frame is not None and self._frame.closed:
      raise DanglingHandleError(
          f"Dangling Handle<{type(self._obj).__name__ if self._obj else 'null'}>: "
          f"owning HandleScope/LocalHandleScope already exited!")
    if self._persistent is not None and self._persistent.destroyed:
      raise DanglingHandleError(
          f"Dangling PersistentHandle<{type(self._obj).__name__ if self._obj else 'null'}>: "
          f"owning PersistentHandles was destroyed!")

  def is_null(self) -> bool:
    self._validate()
    return self._obj is None

  def deref(self) -> Any:
    """Models ``*handle`` -> returns a fresh ``RawTaggedRef`` at current ``gc_epoch``."""
    self._validate()
    if self._obj is None:
      raise DanglingHandleError("Dereferenced null Handle<T>!")
    if isinstance(self._obj, HeapObject):
      self._obj._check_published()
    return wrap_raw_if_movable(self._obj)

  def ToHandleChecked(self) -> "HandleBox":
    self._validate()
    if self._obj is None:
      raise DanglingHandleError(
          "ToHandleChecked() called on empty MaybeHandle!")
    return HandleBox(
        self._obj,
        frame=self._frame,
        persistent=self._persistent,
        is_maybe=False)

  def ToHandle(self, out_ref: Any) -> bool:
    self._validate()
    if self._obj is None:
      return False
    out_ref.set(
        HandleBox(
            self._obj,
            frame=self._frame,
            persistent=self._persistent,
            is_maybe=False))
    return True

  def __getattr__(self, name: str) -> Any:
    # Handle->method() validates the handle and forwards to the live target!
    target = self.deref()
    return getattr(target, name)

  def __bool__(self) -> bool:
    return not self.is_null()

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, HandleBox):
      self._validate()
      other._validate()
      return self._obj is other._obj
    return False

  def __repr__(self) -> str:
    kind = "PHandle" if self._persistent is not None else "Handle"
    return f"{kind}({self._obj!r})"


class HandleScope:
  """RAII ``HandleScope`` on main thread ``Isolate``."""

  def __init__(self, isolate: "Isolate"):
    self.local_heap = isolate.main_thread_local_heap()
    self.frame = HandleFrame(self.local_heap)
    self.local_heap._handle_frames.append(self.frame)

  def CloseAndEscape(self, handle_box: HandleBox) -> HandleBox:
    handle_box._validate()
    obj = handle_box._obj
    frames = self.local_heap._handle_frames
    if len(frames) < 2:
      # Escape into an implicit outer caller frame if none pushed explicitly.
      return HandleBox(obj, frame=None)
    parent_frame = frames[-2]
    escaped = HandleBox(obj, frame=parent_frame)
    parent_frame.handles.append(escaped)
    return escaped

  def __cxx_exit__(self) -> None:
    self.frame.close()
    frames = self.local_heap._handle_frames
    if frames and frames[-1] is self.frame:
      frames.pop()
    elif self.frame in frames:
      frames.remove(self.frame)


class LocalHandleScope:
  """RAII ``LocalHandleScope`` on ``LocalHeap`` / ``LocalIsolate``."""

  def __init__(self, local_heap_or_isolate: Any):
    if isinstance(local_heap_or_isolate, LocalIsolate):
      self.local_heap = local_heap_or_isolate.heap()
    else:
      self.local_heap = local_heap_or_isolate
    self.frame = HandleFrame(self.local_heap)
    self.local_heap._handle_frames.append(self.frame)

  def __cxx_exit__(self) -> None:
    self.frame.close()
    frames = self.local_heap._handle_frames
    if frames and frames[-1] is self.frame:
      frames.pop()
    elif self.frame in frames:
      frames.remove(self.frame)


class DisallowGarbageCollection:
  """RAII ``DisallowGarbageCollection`` scope checked at every safepoint/GC."""

  def __init__(self, heap: "V8Heap"):
    self._heap = heap
    self._tid = heap.mem_ctx.active_thread.tid
    heap.no_gc_depth[self._tid] = heap.no_gc_depth.get(self._tid, 0) + 1

  def __cxx_exit__(self) -> None:
    cur = self._heap.no_gc_depth.get(self._tid, 0)
    self._heap.no_gc_depth[self._tid] = max(0, cur - 1)


class DisallowHeapAllocation(DisallowGarbageCollection):
  pass


class DisallowSafepoints(DisallowGarbageCollection):
  pass


class LocalHeap:
  """Separate ``LocalHeap`` for main thread vs background thread."""

  def __init__(self, heap: "V8Heap", *, is_main_thread: bool):
    self.heap = heap
    self.is_main_thread = is_main_thread
    self._handle_frames: List[HandleFrame] = []
    self._attached_persistent: Optional[PersistentHandles] = None
    self.safepoint_count: int = 0

  def AttachPersistentHandles(self, ph: Optional[PersistentHandles]) -> None:
    if self._attached_persistent is not None:
      raise CxxDcheckFailure(
          "AttachPersistentHandles called when PersistentHandles already attached"
      )
    if ph is None:
      raise DanglingHandleError(
          "AttachPersistentHandles called with null PersistentHandles")
    if ph.destroyed:
      raise DanglingHandleError(
          "AttachPersistentHandles called with destroyed PersistentHandles")
    self._attached_persistent = ph
    ph.attached_to = self

  def DetachPersistentHandles(self) -> PersistentHandles:
    if self._attached_persistent is None:
      raise DanglingHandleError(
          "DetachPersistentHandles called with none attached")
    ph = self._attached_persistent
    self._attached_persistent = None
    ph.attached_to = None
    return ph

  def NewPersistentHandle(self, obj: Any) -> HandleBox:
    # Matches real V8 LocalHeap::NewPersistentHandle -> EnsurePersistentHandles()
    # (src/heap/local-heap.h:89-92): lazily allocates a fresh PersistentHandles
    # block if none was attached yet.
    if self._attached_persistent is None:
      ph = PersistentHandles(self.heap)
      self._attached_persistent = ph
      ph.attached_to = self
    return self._attached_persistent.NewHandle(obj)

  def destroy(self) -> None:
    """Models ``~LocalHeap()`` when the background thread/LocalIsolate exits.

    Closes any remaining local handle frames and destroys any PersistentHandles
    still owned by this LocalHeap (e.g. if DetachPersistentHandles was omitted).
    """
    for frame in list(self._handle_frames):
      frame.close()
    self._handle_frames.clear()
    if self._attached_persistent is not None:
      ph = self._attached_persistent
      self._attached_persistent = None
      ph.destroy()

  def NewLocalHandle(self, obj: Any) -> HandleBox:
    raw = unwrap_raw(obj)
    if not self._handle_frames:
      raise DanglingHandleError(
          "Local handle created without an active HandleScope / LocalHandleScope"
      )
    frame = self._handle_frames[-1]
    box = HandleBox(raw, frame=frame)
    frame.handles.append(box)
    return box

  def Safepoint(self) -> None:
    tid = self.heap.mem_ctx.active_thread.tid
    if self.heap.no_gc_depth.get(tid, 0) > 0:
      raise GcInNoGcScopeError(
          f"LocalHeap::Safepoint() called on thread {tid!r} inside "
          f"DisallowGarbageCollection (depth={self.heap.no_gc_depth[tid]})")
    self.safepoint_count += 1
    mc = self.heap.mem_ctx
    if not mc.in_observer_callback:
      mc.in_observer_callback = True
      try:
        mc.observer.on_safepoint(self)
      finally:
        mc.in_observer_callback = False

  def active_handle_boxes(self) -> List[HandleBox]:
    boxes: List[HandleBox] = []
    for f in self._handle_frames:
      if not f.closed:
        boxes.extend(f.handles)
    if self._attached_persistent is not None and not self._attached_persistent.destroyed:
      boxes.extend(self._attached_persistent.handles)
    return boxes


class IsolateForSandbox:

  def __init__(self, isolate: Any = None):
    self.isolate = isolate


class V8Flags:

  def __init__(self, *, verify_code_merge: bool = False):
    self.verify_code_merge = verify_code_merge


class Isolate:
  """Models main-thread ``v8::internal::Isolate``."""

  def __init__(self, heap: "V8Heap", *, needs_source_positions: bool = False):
    self.v8_heap = heap
    self._main_local_heap = heap.main_local_heap
    self._needs_source_positions = needs_source_positions

  def main_thread_local_heap(self) -> LocalHeap:
    return self._main_local_heap

  def heap(self) -> LocalHeap:
    return self._main_local_heap

  def NeedsSourcePositions(self) -> bool:
    return self._needs_source_positions

  def PushParamsAndDie(self, msg: str, *args: Any) -> None:
    if "duplicate scope info" in str(msg).lower():
      raise DuplicateScopeInfoError(f"PushParamsAndDie({msg!r}, {args!r})")
    raise CxxDcheckFailure(f"PushParamsAndDie({msg!r}, {args!r})")


class LocalIsolate:
  """Models background-thread ``v8::internal::LocalIsolate``."""

  def __init__(self, heap: "V8Heap"):
    self.v8_heap = heap
    self._bg_local_heap = heap.bg_local_heap

  def heap(self) -> LocalHeap:
    return self._bg_local_heap


class V8Heap:
  """Managed V8 heap with moving GC epochs and real handle-based reachability."""

  def __init__(self, mem_ctx: MemoryContext):
    self.mem_ctx = mem_ctx
    self.gc_epoch: int = 0
    self.no_gc_depth: Dict[str, int] = {"main": 0, "bg": 0}
    self.objects: List[HeapObject] = []
    self.weak_arrays: List[FixedArray] = []
    self.main_local_heap = LocalHeap(self, is_main_thread=True)
    self.bg_local_heap = LocalHeap(self, is_main_thread=False)
    self.external_roots: List[HeapObject] = []
    # Optional caller root for BackgroundMergeTask's detached persistent_handles_
    self.active_tasks: List["BackgroundMergeTask"] = []

  def register_object(self, obj: HeapObject) -> None:
    self.objects.append(obj)

  def register_weak_array(self, arr: FixedArray) -> None:
    self.weak_arrays.append(arr)

  def add_root(self, obj: Any) -> None:
    raw = unwrap_for_gc(obj)
    if isinstance(raw, HeapObject) and raw not in self.external_roots:
      self.external_roots.append(raw)

  def remove_root(self, obj: Any) -> None:
    raw = unwrap_for_gc(obj)
    if raw in self.external_roots:
      self.external_roots.remove(raw)

  def run_gc(
      self,
      *,
      check_thread: Optional[str] = None,
      advance_epoch: bool = True,
  ) -> List[Tuple[FixedArray, int, Any]]:
    """Executes a real moving + weak-clearing + poisoning GC traced ONLY from real roots.

    Matches real V8 mark-compact weak clearing:
      1. Traces strong reachability strictly from ``self.external_roots``, open
         ``HandleFrame`` boxes on ``main_local_heap`` and ``bg_local_heap``,
         and live ``PersistentHandles`` attached to a ``LocalHeap`` or owned by
         ``task.persistent_handles_``.
      2. Clears EVERY unreachable weak slot across EVERY registered
         ``WeakFixedArray`` (including both ``old_script->infos()`` and
         ``new_script->infos()``).
      3. Poisons (``obj.freed = True``) EVERY unreachable non-read-only
         ``HeapObject`` so any subsequent dereference or store raises
         ``DanglingHandleError``.
    """
    tid = check_thread or self.mem_ctx.active_thread.tid
    if self.no_gc_depth.get(tid, 0) > 0:
      raise GcInNoGcScopeError(
          f"GC triggered while thread {tid!r} is inside "
          f"DisallowGarbageCollection (depth={self.no_gc_depth[tid]})")

    # Stop-the-world GC global safepoint park barrier: background thread and all
    # active concurrent background readers park (release) and main thread enters
    # global safepoint (acquire).
    self.mem_ctx.main_thread.sync_from(self.mem_ctx.bg_thread)
    for reader in self.mem_ctx.concurrent_readers:
      self.mem_ctx.main_thread.sync_from(reader)

    if advance_epoch:
      self.gc_epoch += 1

    # 1. Gather true strong roots.
    worklist: List[HeapObject] = []
    seen: Set[int] = set()

    def _mark(obj: Any) -> None:
      raw = unwrap_for_gc(obj)
      if isinstance(raw, HeapObject) and not raw.is_read_only:
        oid = id(raw)
        if oid not in seen:
          seen.add(oid)
          worklist.append(raw)

    for r in self.external_roots:
      _mark(r)
    for box in self.main_local_heap.active_handle_boxes():
      _mark(box._obj)
    for box in self.bg_local_heap.active_handle_boxes():
      _mark(box._obj)
    for task in self.active_tasks:
      ph = task.persistent_handles_
      if ph is not None and not ph.destroyed:
        for box in ph.handles:
          _mark(box._obj)

    while worklist:
      cur = worklist.pop()
      for child in cur._gc_children():
        _mark(child)

    # 2. Clear EVERY unreachable weak slot across EVERY registered WeakFixedArray!
    cleared: List[Tuple[FixedArray, int, Any]] = []
    for arr in self.weak_arrays:
      if id(arr) not in seen:
        continue
      for idx in range(arr.length()):
        val = arr.get_latest_raw(idx)
        if isinstance(val, MaybeObject) and val.is_weak and val.obj is not None:
          if not val.obj.is_read_only and id(val.obj) not in seen:
            arr._slots[idx].write(
                MaybeObject.Cleared(),
                MemoryOrder.RELAXED,
                thread=self.mem_ctx.main_thread,
                owner=arr,
                field_name=f"{arr.label}[{idx}]",
                notify=False,
            )
            cleared.append((arr, idx, val.obj))

    # 3. Poison every unreachable non-read-only HeapObject so use-after-free or
    #    resurrection is immediately caught.
    for obj in self.objects:
      if not obj.is_read_only and id(obj) not in seen:
        obj.freed = True

    # Stop-the-world GC global safepoint unpark barrier: main thread leaves
    # global safepoint (release) and all background threads unpark (acquire), so
    # post-GC reads cannot observe pre-GC overwritten/collected pointers.
    self.mem_ctx.bg_thread.sync_from(self.mem_ctx.main_thread)
    for reader in self.mem_ctx.concurrent_readers:
      reader.sync_from(self.mem_ctx.main_thread)

    return cleared


def cxx_handle(obj: Any, isolate_or_heap: Any) -> HandleBox:
  """Models V8 free function ``handle(Tagged<T>, Isolate* / LocalHeap* / LocalIsolate*)``."""
  if isinstance(isolate_or_heap, Isolate):
    return isolate_or_heap.main_thread_local_heap().NewLocalHandle(obj)
  if isinstance(isolate_or_heap, LocalIsolate):
    return isolate_or_heap.heap().NewLocalHandle(obj)
  if isinstance(isolate_or_heap, LocalHeap):
    return isolate_or_heap.NewLocalHandle(obj)
  raise CxxDcheckFailure(
      f"handle() got unexpected isolate/heap: {isolate_or_heap!r}")


# ---------------------------------------------------------------------------
# C++ STL Containers & Iterators with True Value-Copy & Reference Semantics
# ---------------------------------------------------------------------------


def cxx_copy_value(val: Any) -> Any:
  """Performs C++ value-copy on containers / structs, preserving pointer/handle refs."""
  if hasattr(val, "__cxx_copy__"):
    return val.__cxx_copy__()
  return val


class CxxVectorIterator:
  __slots__ = ("_vec", "_idx", "_reverse")

  def __init__(self, vec: "CxxVector", idx: int, reverse: bool = False):
    self._vec = vec
    self._idx = idx
    self._reverse = reverse

  def __cxx_copy__(self) -> "CxxVectorIterator":
    return CxxVectorIterator(self._vec, self._idx, self._reverse)

  def advance(self, delta: int = 1) -> "CxxVectorIterator":
    self._idx += delta
    return self

  def deref(self) -> Any:
    if self._reverse:
      real_idx = len(self._vec._data) - 1 - self._idx
    else:
      real_idx = self._idx
    if real_idx < 0 or real_idx >= len(self._vec._data):
      raise CxxDcheckFailure(
          f"Out-of-bounds iterator dereference (idx={real_idx}, len={len(self._vec._data)})"
      )
    return self._vec._data[real_idx]

  def __getattr__(self, name: str) -> Any:
    if name.startswith("__") and name.endswith("__"):
      raise AttributeError(name)
    return getattr(self.deref(), name)

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, CxxVectorIterator):
      return (self._vec is other._vec and self._reverse == other._reverse and
              self._idx == other._idx)
    return False

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)


class CxxVector:
  """Models ``std::vector<T>`` with true value copy and mutable subscript/iterators."""

  def __init__(
      self,
      items: Iterable[Any] = (),
      *,
      elem_ctor: Optional[Callable[..., Any]] = None,
  ):
    self._data: List[Any] = [cxx_copy_value(x) for x in items]
    self._elem_ctor = elem_ctor

  def __cxx_copy__(self) -> "CxxVector":
    return CxxVector(self._data, elem_ctor=self._elem_ctor)

  def push_back(self, val: Any) -> None:
    if isinstance(val, list) and self._elem_ctor is not None:
      self._data.append(self._elem_ctor(*val))
    else:
      self._data.append(cxx_copy_value(val))

  def emplace_back(self, *args: Any) -> Any:
    if self._elem_ctor is not None:
      item = self._elem_ctor(*args)
    elif len(args) == 2 and isinstance(args[1],
                                       (LocalHeap, Isolate, LocalIsolate)):
      # ConstantPoolPointerForwarder::AddBytecodeArray:
      # bytecode_arrays_to_update_.emplace_back(bytecode_array, local_heap_);
      item = cxx_handle(args[0], args[1])
    elif len(args) == 1:
      item = cxx_copy_value(args[0])
    else:
      item = tuple(args)
    self._data.append(item)
    return item

  def empty(self) -> bool:
    return len(self._data) == 0

  def size(self) -> int:
    return len(self._data)

  def clear(self) -> None:
    self._data.clear()

  def begin(self) -> CxxVectorIterator:
    return CxxVectorIterator(self, 0, reverse=False)

  def end(self) -> CxxVectorIterator:
    return CxxVectorIterator(self, len(self._data), reverse=False)

  def rbegin(self) -> CxxVectorIterator:
    return CxxVectorIterator(self, 0, reverse=True)

  def rend(self) -> CxxVectorIterator:
    return CxxVectorIterator(self, len(self._data), reverse=True)

  def __len__(self) -> int:
    return len(self._data)

  def __getitem__(self, idx: int) -> Any:
    return self._data[int(idx)]

  def __setitem__(self, idx: int, val: Any) -> None:
    self._data[int(idx)] = cxx_copy_value(val)

  def __iter__(self):
    return iter(self._data)

  def __repr__(self) -> str:
    return f"CxxVector({self._data!r})"


class CxxPair:
  """Models ``std::pair<T1, T2>`` with C++ value-copy semantics."""

  __slots__ = ("first", "second")

  def __init__(self, first: Any, second: Any):
    self.first = first
    self.second = second

  def __cxx_copy__(self) -> "CxxPair":
    return CxxPair(cxx_copy_value(self.first), cxx_copy_value(self.second))

  def __repr__(self) -> str:
    return f"CxxPair({self.first!r}, {self.second!r})"


CxxMapEntry = CxxPair


class _CxxMapEntryRef:
  """Live element reference yielded by ``CxxUnorderedMap.__iter__``.

  By-reference iteration (``for (auto& kv : m)``) binds directly so assigning
  ``kv.second = ...`` mutates the map entry in place; by-value iteration
  (``for (auto kv : m)``) invokes ``__cxx_copy__`` to produce an independent
  ``CxxPair`` copy.
  """

  __slots__ = ("_map", "_key")

  def __init__(self, m: "CxxUnorderedMap", key: Any):
    self._map = m
    self._key = key

  @property
  def first(self) -> Any:
    return self._key

  @property
  def second(self) -> Any:
    return self._map._data[self._key]

  @second.setter
  def second(self, val: Any) -> None:
    self._map._data[self._key] = cxx_copy_value(val)

  def __cxx_copy__(self) -> CxxPair:
    return CxxPair(
        cxx_copy_value(self._key),
        cxx_copy_value(self._map._data[self._key]),
    )


class CxxMapIterator:
  __slots__ = ("_map", "_key", "_is_end")

  def __init__(self,
               m: "CxxUnorderedMap",
               key: Any = None,
               is_end: bool = False):
    self._map = m
    self._key = key
    self._is_end = is_end

  def __cxx_copy__(self) -> "CxxMapIterator":
    return CxxMapIterator(self._map, self._key, self._is_end)

  @property
  def first(self) -> Any:
    if self._is_end:
      raise CxxDcheckFailure("Dereferenced end() map iterator")
    return self._key

  @property
  def second(self) -> Any:
    if self._is_end:
      raise CxxDcheckFailure("Dereferenced end() map iterator")
    return self._map._data[self._key]

  @second.setter
  def second(self, val: Any) -> None:
    if self._is_end:
      raise CxxDcheckFailure("Assigned through end() map iterator")
    self._map._data[self._key] = cxx_copy_value(val)

  def deref(self) -> _CxxMapEntryRef:
    if self._is_end:
      raise CxxDcheckFailure("Dereferenced end() map iterator")
    return _CxxMapEntryRef(self._map, self._key)

  def __eq__(self, other: Any) -> bool:
    if isinstance(other, CxxMapIterator):
      if self._is_end and other._is_end:
        return self._map is other._map
      return (self._map is other._map and self._is_end == other._is_end and
              self._key == other._key)
    return False

  def __ne__(self, other: Any) -> bool:
    return not self.__eq__(other)


class CxxUnorderedMap:
  """Models ``std::unordered_map<K, V>`` with value copy and iterator semantics."""

  def __init__(self, items: Optional[Dict[Any, Any]] = None):
    self._data: Dict[Any, Any] = ({
        k: cxx_copy_value(v) for k, v in items.items()
    } if items else {})

  def __cxx_copy__(self) -> "CxxUnorderedMap":
    return CxxUnorderedMap(self._data)

  def find(self, key: Any) -> CxxMapIterator:
    if key in self._data:
      return CxxMapIterator(self, key, is_end=False)
    return self.end()

  def end(self) -> CxxMapIterator:
    return CxxMapIterator(self, is_end=True)

  def empty(self) -> bool:
    return len(self._data) == 0

  def size(self) -> int:
    return len(self._data)

  def count(self, key: Any) -> int:
    return 1 if key in self._data else 0

  def emplace(self, key: Any, val: Any) -> CxxPair:
    if key in self._data:
      return CxxPair(CxxMapIterator(self, key, is_end=False), False)
    self._data[key] = cxx_copy_value(val)
    return CxxPair(CxxMapIterator(self, key, is_end=False), True)

  def __getitem__(self, key: Any) -> Any:
    if key not in self._data:
      self._data[key] = 0
    return self._data[key]

  def __setitem__(self, key: Any, val: Any) -> None:
    self._data[key] = cxx_copy_value(val)

  def __len__(self) -> int:
    return len(self._data)

  def __iter__(self):
    return iter([_CxxMapEntryRef(self, k) for k in list(self._data.keys())])

  def __repr__(self) -> str:
    return f"CxxUnorderedMap({self._data!r})"


class CxxUnorderedSet:
  """Models ``std::unordered_set<K>`` with value copy."""

  def __init__(self, items: Iterable[Any] = ()):
    self._data: Set[Any] = set(items)

  def __cxx_copy__(self) -> "CxxUnorderedSet":
    return CxxUnorderedSet(self._data)

  def insert(self, key: Any) -> None:
    self._data.add(key)

  def count(self, key: Any) -> int:
    return 1 if key in self._data else 0

  def contains(self, key: Any) -> bool:
    return key in self._data

  def empty(self) -> bool:
    return len(self._data) == 0

  def size(self) -> int:
    return len(self._data)

  def __iter__(self):
    return iter(self._data)


# ---------------------------------------------------------------------------
# BackgroundMergeTask & ConstantPoolPointerForwarder data models
# (Methods are executed directly from Clang-parsed C++ ASTs via BoundMergeRuntime!)
# ---------------------------------------------------------------------------


@dataclass
class NewCompiledDataForCachedSfi:
  cached_sfi: HandleBox
  new_sfi: HandleBox

  def __cxx_copy__(self) -> "NewCompiledDataForCachedSfi":
    return NewCompiledDataForCachedSfi(self.cached_sfi, self.new_sfi)


class ConstantPoolPointerForwarder:
  """Data container for C++ ``ConstantPoolPointerForwarder``."""

  def __init__(self, local_heap: LocalHeap, old_script: HandleBox):
    self.local_heap_ = local_heap
    self.old_script_ = old_script
    self.bytecode_arrays_to_update_ = CxxVector()
    self.scope_infos_to_update_ = CxxUnorderedMap()
    self.has_shared_function_info_to_forward_: bool = False


class BackgroundMergeTask:
  """Data container for C++ ``BackgroundMergeTask``.

  NOTE: Neither ``state_`` nor foreground return values are overwritten by
  Python wrappers; the interpreted C++ ``BeginMergeInBackground`` and
  ``CompleteMergeInForeground`` own ``state_`` transitions and the returned
  ``Handle<SharedFunctionInfo>`` directly.
  """

  kNotStarted = BackgroundMergeState.kNotStarted
  kPendingBackgroundWork = BackgroundMergeState.kPendingBackgroundWork
  kPendingForegroundWork = BackgroundMergeState.kPendingForegroundWork
  kDone = BackgroundMergeState.kDone

  def __init__(self, heap: V8Heap):
    self.heap = heap
    self.state_: int = BackgroundMergeState.kNotStarted
    self._persistent_handles: Optional[PersistentHandles] = None
    self.cached_script_: HandleBox = HandleBox(None, is_maybe=True)
    self.toplevel_sfi_from_cached_script_: HandleBox = HandleBox(
        None, is_maybe=True)
    self.used_new_sfis_ = CxxVector()
    self.used_new_scope_infos_ = CxxVector()
    self.new_compiled_data_for_cached_sfis_ = CxxVector(
        elem_ctor=NewCompiledDataForCachedSfi)
    # Present on CL 1 / CL 2 (removed in CL 3); harmless empty set if unused.
    self.sfis_without_scope_info_ = CxxUnorderedSet()
    heap.active_tasks.append(self)

  @property
  def persistent_handles_(self) -> Optional[PersistentHandles]:
    return self._persistent_handles

  @persistent_handles_.setter
  def persistent_handles_(self, ph: Optional[PersistentHandles]) -> None:
    old = self._persistent_handles
    # Models std::unique_ptr<PersistentHandles>::operator=: replacing or
    # resetting a non-null owned PersistentHandles block destroys the old block.
    if old is not None and old is not ph:
      old.destroy()
    self._persistent_handles = ph

  def _release_persistent_handles_(self) -> Optional[PersistentHandles]:
    """Transfers ``std::unique_ptr`` ownership without destroying the block."""
    ph = self._persistent_handles
    self._persistent_handles = None
    return ph

  def SetUpOnMainThread(self, isolate: Isolate,
                        old_script_handle: HandleBox) -> None:
    """Models ``BackgroundMergeTask::SetUpOnMainThread`` (roots ``cached_script_``)."""
    ph = PersistentHandles(isolate)
    self.persistent_handles_ = ph
    self.state_ = BackgroundMergeState.kPendingBackgroundWork
    self.cached_script_ = ph.NewHandle(old_script_handle.deref())


# ---------------------------------------------------------------------------
# Exact V8 Runtime Type Predicates & Casts (No Substring / HeapObject Hacks!)
# ---------------------------------------------------------------------------

_EXACT_TYPE_MAP: Dict[str, Tuple[type, ...]] = {
    "SharedFunctionInfo": (SharedFunctionInfo,),
    "ScopeInfo": (ScopeInfo,),
    "BytecodeArray": (BytecodeArray,),
    "FixedArray": (FixedArray,),
    "TrustedFixedArray": (FixedArray,),
    "WeakFixedArray": (FixedArray,),
    "ObjectBoilerplateDescription": (ObjectBoilerplateDescription,),
    "FeedbackMetadata": (FeedbackMetadata,),
    "Script": (Script,),
    "String": (String,),
    "Smi": (Smi,),
    "ExposedTrustedObject": (BytecodeArray, FixedArray),
    "TrustedObject": (BytecodeArray, FixedArray),
    "HeapObject": (HeapObject,),
    "Object": (HeapObject, Smi),
}


def normalize_cxx_type_name(type_name: str) -> str:
  """Extracts the exact unqualified C++ class name from a qualified/tagged type."""
  t = type_name.strip()
  for prefix in ("const ", "volatile "):
    if t.startswith(prefix):
      t = t[len(prefix):].strip()
  for suffix in ("&", "*"):
    while t.endswith(suffix):
      t = t[:-1].strip()
  for ns in ("v8::internal::", "i::"):
    t = t.replace(ns, "")
  if t.startswith("Tagged<") and t.endswith(">"):
    t = t[len("Tagged<"):-1].strip()
  return t


def is_cxx_type(val: Any, type_name: str) -> bool:
  """Exact C++ ``Is<T>(val)`` predicate (checks publication & moving-GC epoch!)."""
  t = normalize_cxx_type_name(type_name)
  raw = unwrap_raw(val)
  if raw is None:
    return False
  if t == "TheHole":
    return raw is THE_HOLE
  if t == "Undefined":
    return raw is UNDEFINED
  if t == "UninitializedHole":
    return raw is UNINITIALIZED_HOLE
  if t.startswith("UnionOf<") or t.startswith("Union<"):
    lt = t.find("<")
    inner = t[lt + 1:-1]
    parts = [p.strip() for p in inner.split(",")]
    return any(is_cxx_type(val, p) for p in parts)
  if t == "TrustedFixedArray":
    return isinstance(raw, FixedArray) and raw.is_trusted
  if t == "WeakFixedArray":
    return isinstance(raw, FixedArray) and raw.is_weak_array
  if t == "FixedArray":
    # In V8, ObjectBoilerplateDescription inherits from FixedArray in some
    # places, wait: in ConstantPoolPointerForwarder/VerifyConstantPoolEntry,
    # ObjectBoilerplateDescription is checked BEFORE FixedArray, and
    # FixedArray is NOT WeakFixedArray/TrustedFixedArray unless exact.
    return isinstance(raw, FixedArray) and not raw.is_weak_array
  classes = _EXACT_TYPE_MAP.get(t)
  if classes is None:
    raise CxxDcheckFailure(f"Unknown C++ type in Is<{type_name}>")
  return isinstance(raw, classes)


def cast_cxx_type(val: Any, type_name: str) -> Any:
  """Exact C++ ``Cast<T>(val)`` / ``TrustedCast<T>(val)`` with runtime check."""
  if not is_cxx_type(val, type_name):
    raw = unwrap_for_gc(val)
    raise CxxDcheckFailure(
        f"Cast<{type_name}> failed on value {raw!r} (type={type(raw).__name__})"
    )
  return val


def try_cast_cxx_type(val: Any, type_name: str, out_ref: Any) -> bool:
  """Exact C++ ``TryCast<T>(val, &out)``."""
  if is_cxx_type(val, type_name):
    out_ref.set(val)
    return True
  return False
