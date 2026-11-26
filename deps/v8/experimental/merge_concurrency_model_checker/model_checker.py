# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Adversarial concurrency model checker for V8 ``BackgroundMergeTask``.

Executes real Clang-parsed V8 C++ code (``BeginMergeInBackground``,
``CompleteMergeInForeground``, ``ConstantPoolPointerForwarder``,
``VerifyCodeMerge``, ``VerifyConstantPoolEntry``, and 16 ``SharedFunctionInfo``
methods including ``CopyFrom``) across bounded multi-slot topologies, all four
C++11 memory-model schedules (``latest``, ``oldest``, ``intermediate``,
``per_load_mp``), and both ILP operand evaluation orders (``ltr``, ``rtl``).

Key soundness properties:
* **Synchronous preemption at every boundary**: ``MERGE_MODEL_STEP*``,
  ``SYNCHRONIZATION_POINT*`` (``BeforeGetScopeInfoForMerge``,
  ``BeforeGetOuterScopeInfo``, ``BackgroundMerge_AfterRecordScopeInfos``),
  ``LocalHeap::Safepoint()``, intra-helper shared-heap reads, and every store
  inside ``SharedFunctionInfo::CopyFrom``.
* **Real V8 state transitions only**: Lazy compile strictly executes
  ``SetScopeInfo(kReleaseStore)`` -> ``set_feedback_metadata(kReleaseStore)``
  -> ``set_bytecode_array(kReleaseStore)`` without spuriously rewriting
  ``old_script->infos()``. Bytecode flush executes real
  ``DiscardCompiledMetadata`` (restoring ``outer_scope_info`` and keeping
  ``name_or_scope_info_``).
* **No GC root cheats**: ``GC_CLEAR`` runs real reachability from live
  ``HandleScope``/``LocalHandleScope`` frames, ``PersistentHandles``, and
  explicit JS roots, advancing ``gc_epoch`` to catch raw pointers held across
  safepoints.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Set, Tuple

from clang_frontend import ParsedCxxBundle
from cxx_interpreter import BoundMergeRuntime
from errors import (
    ConcurrentReaderError,
    CxxDcheckFailure,
    DanglingHandleError,
    DuplicateScopeInfoError,
    InvariantViolationError,
    MergeModelStep,
    ModelCheckFailure,
    check_condition,
)
from v8_heap_model import (
    BackgroundMergeState,
    BackgroundMergeTask,
    BytecodeArray,
    ExecutionObserver,
    FeedbackMetadata,
    FixedArray,
    HandleBox,
    HandleScope,
    HeapObject,
    Isolate,
    LocalIsolate,
    MakeWeak,
    MaybeObject,
    MemoryContext,
    MemoryOrder,
    MODULE_SCOPE,
    ObjectBoilerplateDescription,
    SCRIPT_SCOPE,
    ScopeInfo,
    ScopeType,
    Script,
    SharedFunctionInfo,
    Smi,
    String,
    THE_HOLE,
    UNINITIALIZED_HOLE,
    V8Flags,
    V8Heap,
    cxx_handle,
    kAcquireLoad,
    kRelaxedLoad,
    kReleaseStore,
    unwrap_for_gc,
    unwrap_raw,
)

# ---------------------------------------------------------------------------
# Initial slot states & Scenario topology definitions
# ---------------------------------------------------------------------------


class SlotState:
  COMPILED = "COMPILED"  # Compiled SFI with own ScopeInfo, FeedbackMetadata, Bytecode
  UNCOMPILED = "UNCOMPILED"  # Uncompiled SFI with name + outer ScopeInfo (or THE_HOLE)
  FLUSHED = "FLUSHED"  # Flushed SFI: keeps own ScopeInfo, outer restored, kCompileLazy
  CLEARED = "CLEARED"  # Weak slot cleared in old_script->infos()
  SCOPE_PRESENT = "PRESENT"  # Standalone ScopeInfo present in old_script->infos()


@dataclass(frozen=True)
class ScenarioConfig:
  name: str
  topology: str  # "nested_5" or "deep_scope"
  old_states: Tuple[str, ...]
  new_compiled_mask: Tuple[bool, ...]
  new_present_mask: Optional[Tuple[bool, ...]] = None
  parent_has_context: bool = True
  toplevel_has_context: bool = True
  needs_source_positions: bool = False
  module_toplevel: bool = False
  unrooted_old_slots: Tuple[int, ...] = ()


@dataclass(frozen=True)
class MutationAction:
  kind: str  # See apply_mutation()
  slot: int = 1
  extra_slots: Tuple[int, ...] = ()


@dataclass(frozen=True)
class PreemptionSchedule:
  """Maps synchronous interception points to mutations to inject on main thread."""
  name: str
  # Key shapes:
  #   ("STEP", step_name, occ)         -> MERGE_MODEL_STEP*
  #   ("SYNC", sync_name, occ)         -> SYNCHRONIZATION_POINT*
  #   ("SAFEPOINT", idx)               -> LocalHeap::Safepoint()
  #   ("READ", field_name, occ)        -> shared-heap read inside BG
  #   ("BETWEEN_BG_FG", 0)             -> right after BG completes, before FG
  events: Tuple[Tuple[Tuple[Any, ...], MutationAction], ...] = ()
  check_mid_copy_reader: bool = True


@dataclass
class TraceResult:
  scenario: str
  schedule: str
  mem_mode: str
  ilp_order: str
  events_requested: int
  events_applied: int
  steps_seen: List[str] = field(default_factory=list)
  sync_points_seen: List[str] = field(default_factory=list)
  safepoints_seen: int = 0
  concurrent_reader_probes: int = 0


# ---------------------------------------------------------------------------
# Scenario Environment Construction
# ---------------------------------------------------------------------------


class ScenarioEnv:
  """Constructed V8 heap + scripts + canonical ScopeInfo metadata for one trace."""

  def __init__(
      self,
      config: ScenarioConfig,
      bundle: ParsedCxxBundle,
      mem_mode: str = "latest",
      ilp_order: str = "ltr",
  ):
    self.config = config
    self.mem_ctx = MemoryContext(mode=mem_mode)
    self.heap = V8Heap(self.mem_ctx)
    self.isolate = Isolate(
        self.heap, needs_source_positions=config.needs_source_positions)
    self.local_isolate = LocalIsolate(self.heap)
    self.flags = V8Flags(verify_code_merge=False)
    self.runtime = BoundMergeRuntime(
        bundle,
        self.heap,
        self.isolate,
        self.local_isolate,
        flags=self.flags,
        ilp_eval_order=ilp_order,
    )

    self.old_script: Script
    self.new_script: Script
    self.old_sfis: Dict[int, Optional[SharedFunctionInfo]] = {}
    self.new_sfis: Dict[int, Optional[SharedFunctionInfo]] = {}
    self.canonical_old_scopes: Dict[int, ScopeInfo] = {}
    self.slot_scope_spec: Dict[int, Tuple[int, int, int]] = {}
    self.new_scopes: Dict[int, ScopeInfo] = {}
    self.parent_slot: Dict[int, Optional[int]] = {}
    self.slot_uid: Dict[int, int] = {}
    self.expected_feedback_slot: Dict[int, int] = {}
    self.eval_slot: Optional[int] = None
    self.js_roots: Dict[int, HeapObject] = {}
    self.gc_cleared_slots: Set[int] = set()
    self._alloc_counter = 0

    if config.topology == "nested_5":
      self._build_nested_5(config)
    elif config.topology == "deep_scope":
      self._build_deep_scope(config)
    else:
      raise ValueError(f"Unknown topology: {config.topology!r}")

    # Root the two scripts, the deserialized top-level new_sfi (modeling
    # FinalizeScript's live unmerged_sfi handle, which strongly reaches all
    # deserialized new_sfis via constant pools), any standalone deserialized
    # eval ScopeInfo, and JS closure roots for initially-live old_script slots
    # (unless explicitly listed in config.unrooted_old_slots).
    self.heap.add_root(self.old_script)
    self.heap.add_root(self.new_script)
    if self.new_sfis.get(0) is not None:
      self.heap.add_root(self.new_sfis[0])
    # Also root any new_sfis not strongly reachable from new_sfis[0] (if
    # top-level wasn't compiled in new_script) and standalone new eval ScopeInfo.
    for _slot, nsfi in self.new_sfis.items():
      if nsfi is not None and not config.new_compiled_mask[0]:
        self.heap.add_root(nsfi)
    if self.eval_slot is not None:
      ev_new = self.new_script.infos_raw().get_latest_raw(self.eval_slot)
      if isinstance(ev_new,
                    MaybeObject) and ev_new.is_weak and ev_new.obj is not None:
        self.heap.add_root(ev_new.obj)
    unrooted = set(config.unrooted_old_slots)
    old_infos = self.old_script.infos_raw()
    for slot in range(old_infos.length()):
      if slot in unrooted:
        continue
      m = old_infos.get_latest_raw(slot)
      if isinstance(m, MaybeObject) and m.is_weak and isinstance(
          m.obj, HeapObject):
        self.js_roots[slot] = m.obj
        self.heap.add_root(m.obj)

  def _next_id(self) -> int:
    self._alloc_counter += 1
    return self._alloc_counter

  def _make_scope_chain_for_script(
      self,
      prefix: str,
      parent_has_context: bool,
      module_toplevel: bool,
      toplevel_has_context: bool = True,
      *,
      newly_allocated: bool = False,
      writer: Optional[Any] = None,
  ) -> Tuple[Dict[int, ScopeInfo], Dict[int, Optional[int]], Dict[int, int]]:
    h = self.heap
    top_type = MODULE_SCOPE if module_toplevel else SCRIPT_SCOPE
    si0 = ScopeInfo(
        h,
        -2,
        scope_type=top_type,
        context_length=2 if toplevel_has_context else 0,
        outer_scope=None,
        label=f"{prefix}_si0",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    si1 = ScopeInfo(
        h,
        100,
        scope_type=ScopeType.FUNCTION_SCOPE,
        context_length=4 if parent_has_context else 0,
        outer_scope=si0,
        label=f"{prefix}_si1",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    si2 = ScopeInfo(
        h,
        200,
        scope_type=ScopeType.FUNCTION_SCOPE,
        context_length=4,
        outer_scope=si1,
        label=f"{prefix}_si2",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    si3 = ScopeInfo(
        h,
        300,
        scope_type=ScopeType.FUNCTION_SCOPE,
        context_length=0,
        outer_scope=si2,
        label=f"{prefix}_si3",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    si4_eval = ScopeInfo(
        h,
        400,
        scope_type=ScopeType.EVAL_SCOPE,
        context_length=2,
        outer_scope=si1,
        label=f"{prefix}_si4_eval",
        newly_allocated=newly_allocated,
        writer=writer,
    )
    scopes = {0: si0, 1: si1, 2: si2, 3: si3, 4: si4_eval}
    parents = {0: None, 1: 0, 2: 1, 3: 2, 4: 1}
    uids = {0: -2, 1: 100, 2: 200, 3: 300, 4: 400}
    return scopes, parents, uids

  def _build_nested_5(self, config: ScenarioConfig) -> None:
    h = self.heap
    bg = self.mem_ctx.bg_thread
    old_scopes, parents, uids = self._make_scope_chain_for_script(
        "old",
        config.parent_has_context,
        config.module_toplevel,
        config.toplevel_has_context,
    )

    def _outer_scope_with_context(scopes: Dict[int, ScopeInfo],
                                  slot: int) -> Any:
      # Mirrors V8 Scope::GetOuterScopeWithContext(): uncompiled SFIs only store
      # the nearest enclosing ScopeInfo that has a context, or THE_HOLE if none.
      p = parents[slot]
      while p is not None:
        if scopes[p].ContextLength() > 0:
          return scopes[p]
        p = parents[p]
      return THE_HOLE

    self.canonical_old_scopes = old_scopes
    self.slot_scope_spec = {
        s: (si.scope_type(), si.ContextLength(), si.EndPosition())
        for s, si in old_scopes.items()
    }
    self.parent_slot = parents
    self.slot_uid = uids
    self.eval_slot = 4

    old_infos = FixedArray(
        h, [MaybeObject.Cleared()] * 5, label="old_infos", is_weak_array=True)
    new_infos = FixedArray(
        h, [MaybeObject.Cleared()] * 5, label="new_infos", is_weak_array=True)
    self.old_script = Script(h, old_infos, label="old_script")
    self.new_script = Script(h, new_infos, label="new_script")

    # Build old SFIs in reverse order (3..0) so compiled parents retain their
    # existing direct inner SFIs in their constant pools, matching real V8 GC
    # reachability.
    for slot in (3, 2, 1, 0):
      st = config.old_states[slot]
      fb_slot = 10 + slot
      self.expected_feedback_slot[slot] = fb_slot
      if st == SlotState.CLEARED:
        self.old_sfis[slot] = None
        continue
      outer_si = _outer_scope_with_context(old_scopes, slot)
      if st == SlotState.COMPILED:
        old_cp_entries: List[Any] = [old_scopes[slot]]
        if slot < 3 and self.old_sfis.get(slot + 1) is not None:
          old_cp_entries.append(self.old_sfis[slot + 1])
        bc = BytecodeArray(
            h,
            FixedArray(
                h, old_cp_entries, is_trusted=True, label=f"old_cp{slot}"),
            label=f"old_bc{slot}",
        )
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=old_scopes[slot],
            outer_scope_or_fb=FeedbackMetadata(h, f"old_fb{slot}"),
            bytecode=bc,
            unique_id_in_script=uids[slot],
            length=10 + slot,
            formal_parameter_count=1 + slot,
            function_token_offset=4 + slot,
            expected_nof_properties=2 + slot,
            flags2=1,
            feedback_slot=fb_slot,
            label=f"old_sfi{slot}",
        )
      elif st == SlotState.FLUSHED:
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=old_scopes[slot],
            outer_scope_or_fb=outer_si,
            bytecode=None,
            unique_id_in_script=uids[slot],
            length=0,
            formal_parameter_count=1 + slot,
            function_token_offset=0,
            expected_nof_properties=0,
            flags2=0,
            feedback_slot=0,
            label=f"old_sfi{slot}",
        )
      elif st == SlotState.UNCOMPILED:
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=String(h, f"fn_{slot}"),
            outer_scope_or_fb=outer_si,
            bytecode=None,
            unique_id_in_script=uids[slot],
            length=0,
            formal_parameter_count=1 + slot,
            function_token_offset=0,
            expected_nof_properties=0,
            flags2=0,
            feedback_slot=0,
            label=f"old_sfi{slot}",
        )
      else:
        raise ValueError(f"Invalid old SFI slot state: {st!r}")
      self.old_sfis[slot] = sfi
      old_infos.set(slot, MakeWeak(sfi))

    if len(config.old_states
          ) > 4 and config.old_states[4] == SlotState.SCOPE_PRESENT:
      old_infos.set(4, MakeWeak(old_scopes[4]))

    # Build ALL deserialized new objects (ScopeInfos, inner block ScopeInfo,
    # BytecodeArrays, constant pools, ObjectBoilerplateDescription, nested
    # FixedArrays, FeedbackMetadata, name Strings, and SFIs) on bg_thread with
    # newly_allocated=True to model real V8 off-thread deserialization (so
    # foreground publication MUST use kReleaseStore!).
    present_mask = (
        config.new_present_mask if config.new_present_mask is not None else
        (True, True, True, True))
    prev_t = self.mem_ctx.switch_thread(bg)
    try:
      new_scopes, _, _ = self._make_scope_chain_for_script(
          "new",
          config.parent_has_context,
          config.module_toplevel,
          config.toplevel_has_context,
          newly_allocated=True,
          writer=bg,
      )
      self.new_scopes = new_scopes

      # Non-SFI inner block ScopeInfo referenced inside compiled new_cp1 (NOT an
      # SFI's own scope_info and NOT in script->infos()), matching V8's
      # CreateBlockContext so VisitScopeInfo's outer-scope forwarding is directly
      # exercised independently of UpdateScopeInfo / UpdateStandaloneScopeInfo.
      new_si_block = ScopeInfo(
          h,
          150,
          scope_type=ScopeType.BLOCK_SCOPE,
          context_length=2,
          outer_scope=new_scopes[1],
          label="new_si_block",
          newly_allocated=True,
          writer=bg,
      )

      for slot in (3, 2, 1, 0):
        if not present_mask[slot]:
          self.new_sfis[slot] = None
          continue
        is_comp = config.new_compiled_mask[slot]
        fb_slot = 10 + slot
        self.expected_feedback_slot[slot] = fb_slot
        outer_si = _outer_scope_with_context(new_scopes, slot)
        bc = None
        if is_comp:
          cp_entries: List[Any] = [Smi.FromInt(42)]
          # Match real V8 bytecode generation: only functions with a context
          # (ContextLength > 0) reference their own ScopeInfo in their constant
          # pool; contextless leaf functions (slot 3) rely on UpdateScopeInfo.
          if new_scopes[slot].ContextLength() > 0:
            cp_entries.append(new_scopes[slot])
          if slot == 0:
            if self.new_sfis[1] is not None:
              cp_entries.append(self.new_sfis[1])
            if self.new_sfis[3] is not None:
              bp = ObjectBoilerplateDescription(
                  h,
                  [UNINITIALIZED_HOLE, self.new_sfis[3]],
                  label="new_bp0",
                  newly_allocated=True,
                  writer=bg,
              )
              cp_entries.append(bp)
            if self.new_sfis[2] is not None:
              nested_entries: List[Any] = [self.new_sfis[2]]
              if new_scopes[0].ContextLength() > 0:
                nested_entries.append(new_scopes[0])
              nested_fa = FixedArray(
                  h,
                  nested_entries,
                  label="new_nested_fa0",
                  newly_allocated=True,
                  writer=bg,
              )
              cp_entries.append(nested_fa)
          elif slot == 1:
            if self.new_sfis[2] is not None:
              cp_entries.append(self.new_sfis[2])
            if config.parent_has_context:
              cp_entries.append(new_si_block)
          elif slot == 2:
            if self.new_sfis[3] is not None:
              cp_entries.append(self.new_sfis[3])
          cp_arr = FixedArray(
              h,
              cp_entries,
              is_trusted=True,
              label=f"new_cp{slot}",
              newly_allocated=True,
              writer=bg,
          )
          bc = BytecodeArray(
              h,
              cp_arr,
              label=f"new_bc{slot}",
              newly_allocated=True,
              writer=bg,
          )
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.new_script,
            name_or_scope_info=(new_scopes[slot] if is_comp else String(
                h, f"fn_{slot}", newly_allocated=True, writer=bg)),
            outer_scope_or_fb=(FeedbackMetadata(
                h, f"new_fb{slot}", newly_allocated=True, writer=bg)
                               if is_comp else outer_si),
            bytecode=bc,
            unique_id_in_script=uids[slot],
            length=(10 + slot) if is_comp else 0,
            formal_parameter_count=1 + slot,
            function_token_offset=(4 + slot) if is_comp else 0,
            expected_nof_properties=(2 + slot) if is_comp else 0,
            flags2=1 if is_comp else 0,
            feedback_slot=fb_slot if is_comp else 0,
            label=f"new_sfi{slot}",
            newly_allocated=True,
            writer=bg,
        )
        self.new_sfis[slot] = sfi
        new_infos.set(slot, MakeWeak(sfi))

      # Standalone ScopeInfo in new_script->infos()[4] (NOT in any compiled CP, so
      # UpdateStandaloneScopeInfo is the sole path forwarding its outer chain!).
      if len(present_mask) < 5 or present_mask[4]:
        new_infos.set(4, MakeWeak(new_scopes[4]))
    finally:
      self.mem_ctx.switch_thread(prev_t)

  def _build_deep_scope(self, config: ScenarioConfig) -> None:
    """Deep intermediate block-scope chain testing UpdateScopeInfo/Standalone walking."""
    h = self.heap
    bg = self.mem_ctx.bg_thread
    top_type = MODULE_SCOPE if config.module_toplevel else SCRIPT_SCOPE

    def _chain(
        prefix: str,
        *,
        newly_allocated: bool = False,
        writer: Optional[Any] = None) -> Tuple[Dict[int, ScopeInfo], ScopeInfo]:
      si0 = ScopeInfo(
          h,
          -2,
          scope_type=top_type,
          context_length=2,
          label=f"{prefix}_si0",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      si_blk = ScopeInfo(
          h,
          50,
          scope_type=ScopeType.BLOCK_SCOPE,
          context_length=3 if config.parent_has_context else 0,
          outer_scope=si0,
          label=f"{prefix}_si_blk",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      si1 = ScopeInfo(
          h,
          100,
          scope_type=ScopeType.FUNCTION_SCOPE,
          context_length=4 if config.parent_has_context else 0,
          outer_scope=si_blk,
          label=f"{prefix}_si1",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      si_mid = ScopeInfo(
          h,
          150,
          scope_type=ScopeType.BLOCK_SCOPE,
          context_length=2,
          outer_scope=si1,
          label=f"{prefix}_si_mid",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      si2 = ScopeInfo(
          h,
          200,
          scope_type=ScopeType.FUNCTION_SCOPE,
          context_length=2,
          outer_scope=si_mid,
          label=f"{prefix}_si2",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      si3_eval = ScopeInfo(
          h,
          300,
          scope_type=ScopeType.EVAL_SCOPE,
          context_length=1,
          outer_scope=si_mid,
          label=f"{prefix}_si3_eval",
          newly_allocated=newly_allocated,
          writer=writer,
      )
      return {0: si0, 1: si1, 2: si2, 3: si3_eval}, si_mid

    old_scopes, old_mid = _chain("old")
    self.canonical_old_scopes = old_scopes
    self.slot_scope_spec = {
        s: (si.scope_type(), si.ContextLength(), si.EndPosition())
        for s, si in old_scopes.items()
    }
    self.parent_slot = {0: None, 1: 0, 2: 1, 3: 1}
    self.slot_uid = {0: -2, 1: 100, 2: 200, 3: 300}
    self.eval_slot = 3

    old_infos = FixedArray(
        h, [MaybeObject.Cleared()] * 4, label="old_infos", is_weak_array=True)
    new_infos = FixedArray(
        h, [MaybeObject.Cleared()] * 4, label="new_infos", is_weak_array=True)
    self.old_script = Script(h, old_infos, label="old_script")
    self.new_script = Script(h, new_infos, label="new_script")

    for slot in (2, 1, 0):
      st = config.old_states[slot]
      self.expected_feedback_slot[slot] = 10 + slot
      if st == SlotState.CLEARED:
        self.old_sfis[slot] = None
        continue
      outer_si = (
          THE_HOLE if slot == 0 else (old_scopes[0] if slot == 1 else old_mid))
      if st == SlotState.COMPILED:
        old_cp_entries: List[Any] = [old_scopes[slot]]
        if slot < 2 and self.old_sfis.get(slot + 1) is not None:
          old_cp_entries.append(self.old_sfis[slot + 1])
        if slot == 1 and len(
            config.old_states
        ) > 3 and config.old_states[3] == SlotState.SCOPE_PRESENT:
          old_cp_entries.append(old_scopes[3])
        bc = BytecodeArray(
            h,
            FixedArray(
                h, old_cp_entries, is_trusted=True, label=f"old_cp{slot}"),
            label=f"old_bc{slot}",
        )
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=old_scopes[slot],
            outer_scope_or_fb=FeedbackMetadata(h, f"old_fb{slot}"),
            bytecode=bc,
            unique_id_in_script=self.slot_uid[slot],
            feedback_slot=10 + slot,
            label=f"old_sfi{slot}",
        )
      elif st == SlotState.FLUSHED:
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=old_scopes[slot],
            outer_scope_or_fb=outer_si,
            bytecode=None,
            unique_id_in_script=self.slot_uid[slot],
            feedback_slot=0,
            label=f"old_sfi{slot}",
        )
      else:
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.old_script,
            name_or_scope_info=String(h, f"fn_{slot}"),
            outer_scope_or_fb=outer_si,
            bytecode=None,
            unique_id_in_script=self.slot_uid[slot],
            feedback_slot=0,
            label=f"old_sfi{slot}",
        )
      self.old_sfis[slot] = sfi
      old_infos.set(slot, MakeWeak(sfi))

    if len(config.old_states
          ) > 3 and config.old_states[3] == SlotState.SCOPE_PRESENT:
      old_infos.set(3, MakeWeak(old_scopes[3]))

    prev_t = self.mem_ctx.switch_thread(bg)
    try:
      new_scopes, new_mid = _chain("new", newly_allocated=True, writer=bg)
      self.new_scopes = new_scopes
      for slot in (2, 1, 0):
        is_comp = config.new_compiled_mask[slot]
        outer_si = (
            THE_HOLE if slot == 0 else
            (new_scopes[0] if slot == 1 else new_mid))
        bc = None
        if is_comp:
          cp_entries: List[Any] = [new_scopes[slot]]
          if slot == 0:
            cp_entries.append(self.new_sfis[1])
          elif slot == 1:
            cp_entries.extend([self.new_sfis[2], new_scopes[3]])
          cp_arr = FixedArray(
              h,
              cp_entries,
              is_trusted=True,
              label=f"new_cp{slot}",
              newly_allocated=True,
              writer=bg,
          )
          bc = BytecodeArray(
              h,
              cp_arr,
              label=f"new_bc{slot}",
              newly_allocated=True,
              writer=bg,
          )
        sfi = SharedFunctionInfo(
            h,
            slot,
            self.new_script,
            name_or_scope_info=(new_scopes[slot] if is_comp else String(
                h, f"fn_{slot}", newly_allocated=True, writer=bg)),
            outer_scope_or_fb=(FeedbackMetadata(
                h, f"new_fb{slot}", newly_allocated=True, writer=bg)
                               if is_comp else outer_si),
            bytecode=bc,
            unique_id_in_script=self.slot_uid[slot],
            feedback_slot=10 + slot,
            label=f"new_sfi{slot}",
            newly_allocated=True,
            writer=bg,
        )
        self.new_sfis[slot] = sfi
        new_infos.set(slot, MakeWeak(sfi))

      new_infos.set(3, MakeWeak(new_scopes[3]))
    finally:
      self.mem_ctx.switch_thread(prev_t)

  # --- Real V8 Main-Thread Adversarial Mutations ---

  def _resolve_live_canonical_outer_scope(self,
                                          slot: int) -> Optional[ScopeInfo]:
    """Finds the live canonical parent ScopeInfo in old_script (or creates fresh)."""
    p = self.parent_slot.get(slot)
    if p is None:
      return None
    infos = self.old_script.infos_raw()
    p_maybe = infos.get_latest_raw(p)
    if isinstance(p_maybe, MaybeObject) and p_maybe.is_weak and isinstance(
        p_maybe.obj, SharedFunctionInfo):
      p_nos = p_maybe.obj._name_or_scope_loc.latest_raw()
      if isinstance(p_nos, ScopeInfo) and not p_nos.IsEmpty():
        return p_nos
    # Walk existing SFIs in old_script to see if any already points to parent's uid.
    p_uid = self.slot_uid[p]
    for idx in range(infos.length()):
      m = infos.get_latest_raw(idx)
      if not (isinstance(m, MaybeObject) and m.is_weak and m.obj is not None):
        continue
      si: Optional[ScopeInfo] = None
      if isinstance(m.obj, ScopeInfo):
        si = m.obj
      elif isinstance(m.obj, SharedFunctionInfo):
        nos = m.obj._name_or_scope_loc.latest_raw()
        if isinstance(nos, ScopeInfo) and not nos.IsEmpty():
          si = nos
        else:
          out = m.obj._outer_or_fb_loc.latest_raw()
          if isinstance(out, ScopeInfo) and not out.IsEmpty():
            si = out
      while si is not None and not si.IsEmpty():
        if si.UniqueIdInScript() == p_uid:
          return si
        if si.HasOuterScopeInfo():
          si = si._outer_scope_loc.latest_raw()
        else:
          break
    canon = self.canonical_old_scopes.get(p)
    if canon is not None and not canon.freed:
      return canon
    # Original canonical ScopeInfo was GC-collected; allocate a fresh live
    # outer ScopeInfo (using plain spec metadata, never touching freed objects!).
    stype, clen, end_pos = self.slot_scope_spec[p]
    gp = self._resolve_live_canonical_outer_scope(p)
    fresh = ScopeInfo(
        self.heap,
        p_uid,
        scope_type=stype,
        context_length=clen,
        end_position=end_pos,
        outer_scope=gp,
        label=f"recreated_outer_si{p}_{self._next_id()}",
        newly_allocated=True,
        writer=self.mem_ctx.main_thread,
    )
    self.canonical_old_scopes[p] = fresh
    return fresh

  def _find_existing_scope_for_uid(self, uid: int) -> Optional[ScopeInfo]:
    """Checks if a ScopeInfo with ``uid`` is already reachable in ``old_script``."""
    infos = self.old_script.infos_raw()
    for idx in range(infos.length()):
      m = infos.get_latest_raw(idx)
      if not (isinstance(m, MaybeObject) and m.is_weak and m.obj is not None):
        continue
      si: Optional[ScopeInfo] = None
      if isinstance(m.obj, ScopeInfo):
        si = m.obj
      elif isinstance(m.obj, SharedFunctionInfo):
        nos = m.obj._name_or_scope_loc.latest_raw()
        if isinstance(nos, ScopeInfo) and not nos.IsEmpty():
          si = nos
        else:
          out = m.obj._outer_or_fb_loc.latest_raw()
          if isinstance(out, ScopeInfo) and not out.IsEmpty():
            si = out
      while si is not None and not si.IsEmpty():
        if si.UniqueIdInScript() == uid:
          return si
        if si.HasOuterScopeInfo():
          si = si._outer_scope_loc.latest_raw()
        else:
          break
    return None

  def _get_live_old_sfi(self, slot: int) -> Optional[SharedFunctionInfo]:
    infos = self.old_script.infos_raw()
    if slot < 0 or slot >= infos.length():
      return None
    val = infos.get_latest_raw(slot)
    if isinstance(val, MaybeObject) and val.is_weak and isinstance(
        val.obj, SharedFunctionInfo):
      return val.obj
    return None

  def apply_mutation(
      self,
      action: MutationAction,
      *,
      caller_tid: str = "bg",
      allow_gc: bool = False,
  ) -> bool:
    """Applies one real V8 main-thread mutation. Returns True iff state changed."""
    mc = self.mem_ctx
    prev_thread = mc.switch_thread(mc.main_thread)
    try:
      kind = action.kind
      slot = action.slot

      if kind == "SAFEPOINT_GC":
        # Real moving + weak-clearing + poisoning GC at a safepoint (advances
        # gc_epoch, checks no_gc_depth, clears every unreachable weak slot!).
        cleared = self.heap.run_gc(
            check_thread=caller_tid,
            advance_epoch=True,
        )
        old_infos = self.old_script.infos_raw()
        for arr, idx, _obj in cleared:
          if arr is old_infos:
            self.gc_cleared_slots.add(idx)
        return True

      if kind == "GC_CLEAR":
        # Drop JS closure root(s) for the target slot(s) and run a real full
        # mark-compact GC across ALL weak arrays (both old_script & new_script).
        slots_to_clear = set(
            action.extra_slots) if action.extra_slots else {slot}
        dropped_any = False
        for s in slots_to_clear:
          dropped = self.js_roots.pop(s, None)
          if dropped is not None:
            self.heap.remove_root(dropped)
            dropped_any = True
        cleared = self.heap.run_gc(
            check_thread=caller_tid,
            advance_epoch=True,
        )
        old_infos = self.old_script.infos_raw()
        for arr, idx, _obj in cleared:
          if arr is old_infos:
            self.gc_cleared_slots.add(idx)
        return dropped_any or len(cleared) > 0

      if kind == "LAZY_COMPILE_STEP1_SCOPE":
        sfi = self._get_live_old_sfi(slot)
        if sfi is None:
          return False
        if isinstance(sfi._name_or_scope_loc.latest_raw(), ScopeInfo):
          return False
        if sfi._trusted_loc.latest_raw() is not None:
          return False
        # Reuse an already-live ScopeInfo for this uid (real V8 invariant:
        # outer/inner compilation shares live ScopeInfos) or allocate a fresh one.
        existing = self._find_existing_scope_for_uid(self.slot_uid[slot])
        if existing is not None:
          si = existing
        else:
          outer_raw = sfi._outer_or_fb_loc.latest_raw()
          outer_si = (
              outer_raw if isinstance(outer_raw, ScopeInfo) else
              self._resolve_live_canonical_outer_scope(slot))
          stype, clen, end_pos = self.slot_scope_spec[slot]
          si = ScopeInfo(
              self.heap,
              self.slot_uid[slot],
              scope_type=stype,
              context_length=clen,
              end_position=end_pos,
              outer_scope=outer_si,
              label=f"lazy_si{slot}_{self._next_id()}",
              newly_allocated=True,
              writer=mc.main_thread,
          )
        sfi.SetScopeInfo(si)
        return True

      if kind == "LAZY_COMPILE_STEP2_FEEDBACK":
        sfi = self._get_live_old_sfi(slot)
        if sfi is None:
          return False
        # Real V8 invariant: SetScopeInfo ALWAYS precedes set_feedback_metadata!
        if not isinstance(sfi._name_or_scope_loc.latest_raw(), ScopeInfo):
          return False
        if isinstance(sfi._outer_or_fb_loc.latest_raw(), FeedbackMetadata):
          return False
        fb = FeedbackMetadata(
            self.heap,
            f"lazy_fb{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        self.runtime.call_sfi_method(sfi, "set_feedback_metadata", fb,
                                     kReleaseStore)
        return True

      if kind == "LAZY_COMPILE_STEP3_CODE":
        sfi = self._get_live_old_sfi(slot)
        if sfi is None:
          return False
        if not isinstance(sfi._name_or_scope_loc.latest_raw(), ScopeInfo):
          return False
        if not isinstance(sfi._outer_or_fb_loc.latest_raw(), FeedbackMetadata):
          return False
        if sfi._trusted_loc.latest_raw() is not None:
          return False
        si = sfi._name_or_scope_loc.latest_raw()
        cp_entries: List[Any] = [si]
        for child_slot, parent_s in self.parent_slot.items():
          if parent_s == slot and child_slot != self.eval_slot:
            child_sfi = self._get_live_old_sfi(child_slot)
            if child_sfi is not None:
              cp_entries.append(child_sfi)
        cp = FixedArray(
            self.heap,
            cp_entries,
            is_trusted=True,
            label=f"lazy_cp{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        bc = BytecodeArray(
            self.heap,
            cp,
            label=f"lazy_bc{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        sfi.set_length(10 + slot)
        sfi.set_function_token_offset(4 + slot)
        sfi.set_expected_nof_properties(2 + slot)
        sfi.set_flags2(1)
        sfi.set_feedback_slot(self.expected_feedback_slot[slot])
        self.runtime.call_sfi_method(sfi, "set_bytecode_array", bc)
        return True

      if kind == "LAZY_COMPILE":
        # Full real V8 3-step lazy compile (SetScopeInfo -> set_feedback_metadata
        # -> set_bytecode_array). Does NOT rewrite old_script->infos()!
        sfi = self._get_live_old_sfi(slot)
        if sfi is None or sfi._trusted_loc.latest_raw() is not None:
          return False
        c1 = self.apply_mutation(
            MutationAction("LAZY_COMPILE_STEP1_SCOPE", slot))
        c2 = self.apply_mutation(
            MutationAction("LAZY_COMPILE_STEP2_FEEDBACK", slot))
        c3 = self.apply_mutation(
            MutationAction("LAZY_COMPILE_STEP3_CODE", slot))
        return c1 or c2 or c3

      if kind == "BYTECODE_FLUSH":
        # Real V8 SharedFunctionInfo::DiscardCompiledMetadata (shared-function-info.cc:359-395):
        # Restores outer_scope_info from scope_info()->OuterScopeInfo() (or THE_HOLE),
        # keeps name_or_scope_info_ intact, and clears bytecode via SetUntrustedData(kCompileLazy).
        sfi = self._get_live_old_sfi(slot)
        if sfi is None or sfi._trusted_loc.latest_raw() is None:
          return False
        si = sfi._name_or_scope_loc.latest_raw()
        if isinstance(si, ScopeInfo) and si.HasOuterScopeInfo():
          restored_outer: Any = si._outer_scope_loc.latest_raw()
        else:
          restored_outer = THE_HOLE
        sfi.set_raw_outer_scope_info_or_feedback_metadata(restored_outer)
        self.runtime.call_sfi_method(
            sfi,
            "SetUntrustedData",
            Smi.FromEnum(1)  # Builtin::kCompileLazy
        )
        return True

      if kind == "REALLOCATE_SFI":
        # Real V8 outer-function recompile recreating an SFI whose weak slot
        # in old_script->infos() was cleared (published with kReleaseStore!).
        infos = self.old_script.infos_raw()
        if slot < 0 or slot >= infos.length() or slot == self.eval_slot:
          return False
        cur = infos.get_latest_raw(slot)
        if isinstance(cur, MaybeObject) and cur.is_weak and cur.obj is not None:
          return False
        outer_si = self._resolve_live_canonical_outer_scope(slot)
        recreated = SharedFunctionInfo(
            self.heap,
            slot,
            self.old_script,
            name_or_scope_info=String(self.heap, f"re_{slot}"),
            outer_scope_or_fb=outer_si if outer_si is not None else THE_HOLE,
            bytecode=None,
            unique_id_in_script=self.slot_uid[slot],
            length=10 + slot,
            formal_parameter_count=1 + slot,
            feedback_slot=0,
            label=f"realloc_sfi{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        # Matches SharedFunctionInfo::SetScript -> list->set(id, MakeWeak(this), kReleaseStore).
        old_root = self.js_roots.pop(slot, None)
        if old_root is not None:
          self.heap.remove_root(old_root)
        self.js_roots[slot] = recreated
        self.heap.add_root(recreated)
        self.gc_cleared_slots.discard(slot)
        infos.set(slot, MakeWeak(recreated), kReleaseStore)
        return True

      if kind == "CONCURRENT_MERGE_PUBLISH":
        # Models a second BackgroundMergeTask completing on main thread while
        # our task runs BeginMergeInBackground!
        infos = self.old_script.infos_raw()
        if slot < 0 or slot >= infos.length() or slot == self.eval_slot:
          return False
        cur = infos.get_latest_raw(slot)
        existing_si = self._find_existing_scope_for_uid(self.slot_uid[slot])
        outer_si = self._resolve_live_canonical_outer_scope(slot)
        stype, clen, end_pos = self.slot_scope_spec[slot]
        si = existing_si or ScopeInfo(
            self.heap,
            self.slot_uid[slot],
            scope_type=stype,
            context_length=clen,
            end_position=end_pos,
            outer_scope=outer_si,
            label=f"cmerge_si{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        cp = FixedArray(
            self.heap,
            [si],
            is_trusted=True,
            label=f"cmerge_cp{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        bc = BytecodeArray(
            self.heap,
            cp,
            label=f"cmerge_bc{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        fb = FeedbackMetadata(
            self.heap,
            f"cmerge_fb{slot}_{self._next_id()}",
            newly_allocated=True,
            writer=mc.main_thread,
        )
        if not (isinstance(cur, MaybeObject) and cur.is_weak and
                cur.obj is not None):
          other_sfi = SharedFunctionInfo(
              self.heap,
              slot,
              self.old_script,
              name_or_scope_info=si,
              outer_scope_or_fb=fb,
              bytecode=bc,
              unique_id_in_script=self.slot_uid[slot],
              length=10 + slot,
              formal_parameter_count=1 + slot,
              function_token_offset=4 + slot,
              expected_nof_properties=2 + slot,
              flags2=1,
              feedback_slot=self.expected_feedback_slot[slot],
              label=f"cmerge_sfi{slot}_{self._next_id()}",
              newly_allocated=True,
              writer=mc.main_thread,
          )
          old_root = self.js_roots.pop(slot, None)
          if old_root is not None:
            self.heap.remove_root(old_root)
          self.js_roots[slot] = other_sfi
          self.heap.add_root(other_sfi)
          self.gc_cleared_slots.discard(slot)
          infos.set(slot, MakeWeak(other_sfi), kReleaseStore)
          return True
        if isinstance(
            cur.obj,
            SharedFunctionInfo) and cur.obj._trusted_loc.latest_raw() is None:
          src_sfi = SharedFunctionInfo(
              self.heap,
              slot,
              self.old_script,
              name_or_scope_info=si,
              outer_scope_or_fb=fb,
              bytecode=bc,
              unique_id_in_script=self.slot_uid[slot],
              length=10 + slot,
              formal_parameter_count=1 + slot,
              function_token_offset=4 + slot,
              expected_nof_properties=2 + slot,
              flags2=1,
              feedback_slot=self.expected_feedback_slot[slot],
              label=f"cmerge_src{slot}_{self._next_id()}",
              newly_allocated=True,
              writer=mc.main_thread,
          )
          self.runtime.call_sfi_method(cur.obj, "CopyFrom", src_sfi,
                                       self.isolate)
          return True
        return False

      raise ValueError(f"Unknown mutation kind: {kind!r}")
    finally:
      mc.switch_thread(prev_thread)


# ---------------------------------------------------------------------------
# Concurrent Background Reader Probe (runs mid-CopyFrom and after FG writes!)
# ---------------------------------------------------------------------------


def verify_concurrent_background_reader(
    env: ScenarioEnv,
    reader_base_view: Any,
    *,
    target_sfi: Optional[SharedFunctionInfo] = None,
    slot_expecting_scope: Optional[Set[int]] = None,
) -> None:
  """Simulates a concurrent background thread inspecting ``old_script`` / ``target_sfi``.

  Runs under a fresh background ``ThreadMemoryView`` cloned from before FG
  started (so it has NO happens-before edge from FG's stores unless an acquire
  load pairs with a release store!). Exercises real Clang-parsed
  ``TryGetScopeInfoForMerge``, ``TryGetOuterScopeInfo``, ``is_compiled``,
  ``HasScopeInfo``, ``HasFeedbackMetadata``, ``feedback_slot``, and plain
  header fields (``length``, ``formal_parameter_count``, ``unique_id``).
  """
  mc = env.mem_ctx
  rt = env.runtime
  reader_tv = reader_base_view.snapshot_for_concurrent_reader(
      "concurrent_bg_reader")
  prev = mc.switch_thread(reader_tv)
  try:
    old_infos = env.old_script.infos_raw()
    slots_to_check = ([target_sfi.function_literal_id(kRelaxedLoad)]
                      if target_sfi is not None else list(
                          range(old_infos.length())))

    for slot in slots_to_check:
      if slot < 0 or slot >= old_infos.length():
        continue
      reader_tv.reset_mp_window()
      maybe = old_infos.get(slot, kAcquireLoad)
      if not maybe.IsWeak():
        continue
      raw_obj = unwrap_raw(maybe.GetHeapObjectAssumeWeak())
      if isinstance(raw_obj, HeapObject):
        raw_obj._check_published()
      if not isinstance(raw_obj, SharedFunctionInfo):
        continue

      # 1. Probe real parsed TryGetScopeInfoForMerge()!
      reader_tv.reset_mp_window()
      got_si = unwrap_raw(
          rt.call_sfi_method(raw_obj, "TryGetScopeInfoForMerge"))
      if isinstance(got_si, ScopeInfo) and not got_si.IsEmpty():
        got_si._check_published()
      # In "latest" mode (where no concurrent write occurs during this probe),
      # if the reader itself observes FeedbackMetadata via kAcquireLoad, then
      # TryGetScopeInfoForMerge() MUST have observed a non-empty ScopeInfo
      # (not empty_scope_info due to wrong CopyFrom field order or missing retry).
      # In non-SC modes ("oldest"/"intermediate"), observing the consistent
      # pre-CopyFrom snapshot (sentinel + outer_scope) is legal in C++11, while
      # missing Release_Store / kAcquireLoad is caught directly inside the
      # parsed C++ (Cast<ScopeInfo> failure / UnsafePublicationError / DataRaceError).
      if mc.mode == "latest" and isinstance(
          unwrap_raw(
              raw_obj.raw_outer_scope_info_or_feedback_metadata(kAcquireLoad)),
          FeedbackMetadata,
      ):
        if got_si is None or got_si.IsEmpty():
          raise ConcurrentReaderError(
              f"Concurrent reader TryGetScopeInfoForMerge() returned empty "
              f"ScopeInfo on {raw_obj} (slot {slot}) while FeedbackMetadata "
              f"was already observable (broken CopyFrom store ordering or "
              f"missing retry in TryGetScopeInfoForMerge!)")
      if slot_expecting_scope and slot in slot_expecting_scope:
        if got_si is None or got_si.IsEmpty():
          raise ConcurrentReaderError(
              f"Concurrent reader TryGetScopeInfoForMerge() lost ScopeInfo on "
              f"{raw_obj} (slot {slot})")

      # 2. Probe real parsed TryGetOuterScopeInfo()!
      reader_tv.reset_mp_window()
      rt.call_sfi_method(raw_obj, "TryGetOuterScopeInfo")

      # 3. Probe real parsed is_compiled() -> if compiled, ALL compiled fields
      #    (ScopeInfo, FeedbackMetadata, feedback_slot, plain header fields,
      #    script, bytecode CP) MUST already be installed and visible!
      reader_tv.reset_mp_window()
      compiled = rt.call_sfi_method(raw_obj, "is_compiled")
      if compiled:
        if not rt.call_sfi_method(raw_obj, "HasScopeInfo", kAcquireLoad):
          raise ConcurrentReaderError(
              f"Concurrent reader saw is_compiled()==true on {raw_obj} while "
              f"HasScopeInfo(kAcquireLoad)==false!")
        if not rt.call_sfi_method(raw_obj, "HasFeedbackMetadata", kAcquireLoad):
          raise ConcurrentReaderError(
              f"Concurrent reader saw is_compiled()==true on {raw_obj} while "
              f"HasFeedbackMetadata(kAcquireLoad)==false!")
        got_fb_slot = raw_obj.feedback_slot()
        exp_fb_slot = env.expected_feedback_slot.get(slot)
        if exp_fb_slot is not None and got_fb_slot != exp_fb_slot:
          raise ConcurrentReaderError(
              f"Concurrent reader saw is_compiled()==true on {raw_obj} with "
              f"stale feedback_slot={got_fb_slot} != expected {exp_fb_slot} "
              f"(set_feedback_slot happened after SetTrustedData!)")
        # Plain header reads: safe ONLY because SetTrustedData(kReleaseStore)
        # publishes them; raises DataRaceError if written after SetTrustedData
        # or if SetTrustedData dropped kReleaseStore!
        _ = raw_obj.length()
        _ = raw_obj.formal_parameter_count()
        _ = raw_obj.unique_id()
        sfi_script = unwrap_raw(raw_obj.script(kAcquireLoad))
        if sfi_script is not env.old_script:
          raise ConcurrentReaderError(
              f"Concurrent reader saw compiled {raw_obj} pointing to wrong script {sfi_script!r}"
          )
        if raw_obj.HasBytecodeArray():
          bc = unwrap_raw(raw_obj.GetBytecodeArray(env.isolate))
          if isinstance(bc, HeapObject):
            bc._check_published()
          cp = unwrap_raw(bc.constant_pool())
          if isinstance(cp, HeapObject):
            cp._check_published()

          def _check_cp_sfis(arr: Any) -> None:
            if isinstance(arr, HeapObject):
              arr._check_published()
            if isinstance(arr, ObjectBoilerplateDescription):
              vals = [
                  unwrap_raw(arr.value(i))
                  for i in range(arr.boilerplate_properties_count())
              ]
            elif isinstance(arr, FixedArray):
              vals = [unwrap_raw(arr.get(i)) for i in range(arr.length())]
            else:
              return
            for entry in vals:
              if isinstance(entry, SharedFunctionInfo):
                entry._check_published()
                if unwrap_raw(entry.script(kAcquireLoad)) is not env.old_script:
                  raise ConcurrentReaderError(
                      f"Concurrent reader saw CP entry {entry} in {raw_obj} pointing "
                      f"to new_script instead of old_script!")
                cid = entry.function_literal_id(kRelaxedLoad)
                cslot = old_infos.get(cid, kAcquireLoad)
                if not cslot.IsWeak() or unwrap_raw(
                    cslot.GetHeapObjectAssumeWeak()) is not entry:
                  raise ConcurrentReaderError(
                      f"Concurrent reader saw published compiled {raw_obj} referencing "
                      f"child SFI {entry} (id={cid}) not yet installed in old_script->infos()[{cid}] "
                      f"(got {cslot!r}; broken reverse release order or unforwarded CP entry!)"
                  )
              elif isinstance(entry, ScopeInfo) and not entry.IsEmpty():
                entry._check_published()
              elif isinstance(entry,
                              (ObjectBoilerplateDescription, FixedArray)):
                _check_cp_sfis(entry)

          _check_cp_sfis(cp)
  finally:
    mc.switch_thread(prev)


# ---------------------------------------------------------------------------
# Synchronous AdversarialObserver
# ---------------------------------------------------------------------------


class AdversarialObserver(ExecutionObserver):
  """Synchronous observer driving preemption & mid-CopyFrom concurrent probes."""

  def __init__(self, env: ScenarioEnv, schedule: PreemptionSchedule):
    self.env = env
    self.schedule = schedule
    self.step_counts: Dict[str, int] = {}
    self.sync_counts: Dict[str, int] = {}
    self.slot_read_counts: Dict[Tuple[int, str], int] = {}
    self.safepoint_idx: int = 0
    self.steps_seen: List[str] = []
    self.sync_points_seen: List[str] = []
    self.events_applied: int = 0
    self.concurrent_reader_probes: int = 0
    self.in_fg: bool = False
    self.pre_fg_reader_view: Any = None
    self._helper_call_stack: List[Tuple[str, SharedFunctionInfo, bool]] = []
    self._event_map: Dict[Tuple[Any, ...], List[MutationAction]] = {}
    for key, act in schedule.events:
      self._event_map.setdefault(key, []).append(act)

  def _fire_key(
      self,
      key: Tuple[Any, ...],
      *,
      caller_tid: str = "bg",
      allow_gc: bool = False,
  ) -> None:
    actions = self._event_map.get(key)
    if not actions:
      return
    for act in actions:
      if self.env.apply_mutation(act, caller_tid=caller_tid, allow_gc=allow_gc):
        self.events_applied += 1

  def on_merge_step(self, step_name: str, allow_safepoint: bool) -> None:
    occ = self.step_counts.get(step_name, 0)
    self.step_counts[step_name] = occ + 1
    self.steps_seen.append(step_name)
    caller_tid = self.env.mem_ctx.active_thread.tid
    self._fire_key(("STEP", step_name, occ),
                   caller_tid=caller_tid,
                   allow_gc=allow_safepoint)
    if self.in_fg and self.schedule.check_mid_copy_reader and self.pre_fg_reader_view is not None:
      if step_name in (
          MergeModelStep.FG_RELEASE_BACKWARDS_START.value,
          MergeModelStep.FG_COPY_FROM.value,
          MergeModelStep.FG_PUBLISH_NEW_SFI.value,
      ):
        verify_concurrent_background_reader(self.env, self.pre_fg_reader_view)
        self.concurrent_reader_probes += 1

  def on_sync_point(self, name: str, allow_safepoint: bool) -> None:
    occ = self.sync_counts.get(name, 0)
    self.sync_counts[name] = occ + 1
    self.sync_points_seen.append(name)
    caller_tid = self.env.mem_ctx.active_thread.tid
    self._fire_key(("SYNC", name, occ),
                   caller_tid=caller_tid,
                   allow_gc=allow_safepoint)

  def on_safepoint(self, local_heap: Any) -> None:
    idx = self.safepoint_idx
    self.safepoint_idx += 1
    caller_tid = self.env.mem_ctx.active_thread.tid
    self._fire_key(("SAFEPOINT", idx), caller_tid=caller_tid, allow_gc=True)

  def on_heap_read(self, owner: Any, field_name: str, order: str) -> None:
    if self.in_fg:
      return
    if isinstance(owner, SharedFunctionInfo):
      slot = owner._fn_id_loc.latest_raw()
      old_infos = self.env.old_script.infos_raw()
      if 0 <= slot < old_infos.length():
        m = old_infos.get_latest_raw(slot)
        if isinstance(m, MaybeObject) and m.is_weak and m.obj is owner:
          rkey = (slot, field_name)
          occ = self.slot_read_counts.get(rkey, 0)
          self.slot_read_counts[rkey] = occ + 1
          self._fire_key(
              ("READ_SLOT", slot, field_name, occ),
              caller_tid="bg",
              allow_gc=False,
          )

  def on_heap_write(self, owner: Any, field_name: str, value: Any,
                    order: str) -> None:
    if (self.in_fg and self.schedule.check_mid_copy_reader and
        self.pre_fg_reader_view is not None and
        isinstance(owner, SharedFunctionInfo)):
      # Only probe SFIs that belong to old_script (shared visible SFIs being
      # mutated by CopyFrom!).
      old_infos = self.env.old_script.infos_raw()
      slot = owner._fn_id_loc.latest_raw()
      if 0 <= slot < old_infos.length():
        m = old_infos.get_latest_raw(slot)
        if isinstance(m, MaybeObject) and m.is_weak and m.obj is owner:
          verify_concurrent_background_reader(
              self.env, self.pre_fg_reader_view, target_sfi=owner)
          self.concurrent_reader_probes += 1

  @staticmethod
  def _initial_loc_value(loc: Any) -> Any:
    if len(loc.history) > 1 and str(
        loc.history[0].value) == "UNINITIALIZED_MEMORY":
      return loc.history[1].value
    return loc.history[0].value

  def _sfi_always_has_scope_info(self, sfi: SharedFunctionInfo) -> bool:
    """True iff ``sfi`` had a non-empty ScopeInfo (own or outer) at creation.

    In V8, neither own ScopeInfo nor lexical outer ScopeInfo can ever vanish
    under legal transitions (lazy compile, bytecode flush, or CopyFrom).
    Therefore, if an SFI had a reachable ScopeInfo from creation onward, NO
    legal C++11 linearization of ``TryGetScopeInfoForMerge()`` may return
    ``empty_scope_info()``.
    """
    init_nos = self._initial_loc_value(sfi._name_or_scope_loc)
    init_outer = self._initial_loc_value(sfi._outer_or_fb_loc)
    if isinstance(init_nos, ScopeInfo) and not init_nos.IsEmpty():
      return True
    if isinstance(init_outer, ScopeInfo) and not init_outer.IsEmpty():
      return True
    return False

  def _sfi_always_has_outer_scope_info(self, sfi: SharedFunctionInfo) -> bool:
    """True iff ``sfi`` had a non-empty lexical outer ScopeInfo at creation."""
    init_nos = self._initial_loc_value(sfi._name_or_scope_loc)
    init_outer = self._initial_loc_value(sfi._outer_or_fb_loc)
    if isinstance(
        init_nos,
        ScopeInfo) and not init_nos.IsEmpty() and init_nos.HasOuterScopeInfo():
      return True
    if isinstance(init_outer, ScopeInfo) and not init_outer.IsEmpty():
      return True
    return False

  def on_method_enter(self, class_name: str, method_name: str,
                      recv: Any) -> None:
    if class_name == "SharedFunctionInfo" and isinstance(
        recv, SharedFunctionInfo):
      if method_name == "TryGetScopeInfoForMerge":
        self._helper_call_stack.append(
            (method_name, recv, self._sfi_always_has_scope_info(recv)))
      elif method_name == "TryGetOuterScopeInfo":
        self._helper_call_stack.append(
            (method_name, recv, self._sfi_always_has_outer_scope_info(recv)))

  def on_method_return(self, class_name: str, method_name: str, recv: Any,
                       ret_val: Any) -> None:
    if (class_name == "SharedFunctionInfo" and
        method_name in ("TryGetScopeInfoForMerge", "TryGetOuterScopeInfo") and
        self._helper_call_stack):
      mname, sfi, must_be_non_empty = self._helper_call_stack.pop()
      if mname == method_name and sfi is recv and must_be_non_empty:
        raw_ret = unwrap_raw(ret_val)
        if not isinstance(raw_ret, ScopeInfo) or raw_ret.IsEmpty():
          raise ConcurrentReaderError(
              f"Linearizability violation: {sfi}->{method_name}() returned "
              f"empty_scope_info() even though {sfi} had a reachable ScopeInfo "
              f"in every legal state (two-slot handoff TOCTOU / missing retry "
              f"or missing kAcquireLoad!)")


# ---------------------------------------------------------------------------
# Single-Trace Runner & Strict Postcondition Verifier
# ---------------------------------------------------------------------------


def run_trace(
    bundle: ParsedCxxBundle,
    config: ScenarioConfig,
    schedule: PreemptionSchedule,
    *,
    mem_mode: str = "latest",
    ilp_order: str = "ltr",
) -> TraceResult:
  """Executes one complete BG + FG merge trace and verifies all postconditions."""
  env = ScenarioEnv(config, bundle, mem_mode=mem_mode, ilp_order=ilp_order)
  obs = AdversarialObserver(env, schedule)
  env.mem_ctx.observer = obs

  # Outer caller HandleScope (must survive CompleteMergeInForeground return via
  # HandleScope::CloseAndEscape!).
  caller_scope = HandleScope(env.isolate)
  try:
    old_sh = cxx_handle(env.old_script, env.isolate)
    new_sh = cxx_handle(env.new_script, env.isolate)

    task = BackgroundMergeTask(env.heap)
    task.SetUpOnMainThread(env.isolate, old_sh)

    # 1. Background phase (runs real Clang-parsed BeginMergeInBackground).
    env.runtime.run_begin_merge_in_background(task, new_sh)

    # Strict post-BG assertions (NEVER overwritten by wrapper!).
    check_condition(
        task.state_ == BackgroundMergeState.kPendingForegroundWork,
        f"After BeginMergeInBackground expected state_=kPendingForegroundWork "
        f"({BackgroundMergeState.kPendingForegroundWork}), got {task.state_}",
    )
    check_condition(
        env.heap.no_gc_depth.get("bg", 0) == 0,
        f"Leaked DisallowGarbageCollection on bg thread (depth={env.heap.no_gc_depth.get('bg')})",
    )
    check_condition(
        len(env.heap.bg_local_heap._handle_frames) == 0,
        "Leaked LocalHandleScope frame on bg LocalHeap",
    )
    # Real V8 destroys the background LocalIsolate/LocalHeap once the off-thread
    # task completes, freeing any PersistentHandles still attached to it (if
    # DetachPersistentHandles() was forgotten).
    env.heap.bg_local_heap.destroy()

    # Optional main-thread mutation between BG completion and FG start.
    obs._fire_key(("BETWEEN_BG_FG", 0), caller_tid="main", allow_gc=True)

    # Snapshot a pre-FG background reader view BEFORE main_thread synchronizes
    # with bg_thread's task completion (so an independent concurrent reader only
    # sees BG-deserialized new_sfis/ScopeInfos/BytecodeArrays if FG publishes
    # them via kReleaseStore!).
    obs.pre_fg_reader_view = env.mem_ctx.main_thread.snapshot_for_concurrent_reader(
        "pre_fg_snap")
    env.mem_ctx.concurrent_readers.append(obs.pre_fg_reader_view)
    # Real V8 BG->FG task handoff happens-before edge (DetachPersistentHandles /
    # task join before CompleteMergeInForeground starts on main thread).
    env.mem_ctx.end_background_handoff()
    obs.in_fg = True

    # 2. Foreground phase (runs real Clang-parsed CompleteMergeInForeground).
    ret_handle = env.runtime.run_complete_merge_in_foreground(task, new_sh)

    # Strict post-FG assertions (state_ & CloseAndEscape return handle!).
    check_condition(
        task.state_ == BackgroundMergeState.kDone,
        f"After CompleteMergeInForeground expected state_=kDone "
        f"({BackgroundMergeState.kDone}), got {task.state_}",
    )
    check_condition(
        env.heap.no_gc_depth.get("main", 0) == 0,
        f"Leaked DisallowGarbageCollection on main thread (depth={env.heap.no_gc_depth.get('main')})",
    )
    check_condition(
        task.persistent_handles_ is not None and
        not task.persistent_handles_.destroyed,
        f"BackgroundMergeTask lost live PersistentHandles across BG->FG handoff "
        f"(persistent_handles_={task.persistent_handles_!r})",
        DanglingHandleError,
    )
    check_condition(
        isinstance(ret_handle, HandleBox) and not ret_handle.is_null(),
        f"CompleteMergeInForeground returned null/invalid handle: {ret_handle!r}",
        DanglingHandleError,
    )
    ret_sfi = unwrap_raw(ret_handle.deref())
    top_maybe = env.old_script.infos_raw().get_latest_raw(0)
    check_condition(
        isinstance(top_maybe, MaybeObject) and top_maybe.is_weak and
        ret_sfi is top_maybe.obj,
        f"CompleteMergeInForeground returned {ret_sfi!r}, expected old_script.infos[0]={top_maybe!r}",
    )

    # 3. Execute REAL Clang-parsed VerifyCodeMerge(isolate, old_script)!
    env.runtime.call_verify_code_merge(env.isolate, old_sh)

    # 4. Verify NeedsSourcePositions side effects when enabled.
    if config.needs_source_positions:
      check_condition(
          env.old_script.line_ends_initialized,
          "Expected Script::InitLineEnds to run when NeedsSourcePositions()==true",
      )

    # 5. Final concurrent reader sanity check on finished old_script.
    if schedule.check_mid_copy_reader:
      verify_concurrent_background_reader(env, obs.pre_fg_reader_view)
      obs.concurrent_reader_probes += 1

    # 6. Verify that any GC-cleared unpaired slot (no deserialized counterpart
    #    in new_script, and not later recreated) stays Cleared post-merge and is
    #    NEVER resurrected from a stale weak copy in new_script->infos()!
    if config.new_present_mask is not None:
      for slot in env.gc_cleared_slots:
        if slot < len(
            config.new_present_mask) and not config.new_present_mask[slot]:
          m = env.old_script.infos_raw().get_latest_raw(slot)
          check_condition(
              isinstance(m, MaybeObject) and m.IsCleared(),
              f"Post-merge slot {slot} was GC-cleared with no new counterpart "
              f"but resurrected as {m!r}!",
          )

    # 7. Verify bytecode retention for newly compiled slots (unless the main
    #    thread flushed, GC-cleared, or concurrently reallocated that slot:
    #    in V8, if a cleared slot is concurrently reallocated on the main
    #    thread after BG chose new_sfi, FG intentionally keeps the existing
    #    uncompiled old_sfi and forwards CP pointers to it rather than
    #    copying bytecode into it).
    slots_with_bytecode_disruption = {
        act.slot
        for _k, act in schedule.events
        if act.kind in ("BYTECODE_FLUSH", "GC_CLEAR", "REALLOCATE_SFI")
    } | env.gc_cleared_slots
    for slot, has_new_bc in enumerate(config.new_compiled_mask):
      if not has_new_bc or slot in slots_with_bytecode_disruption:
        continue
      m = env.old_script.infos_raw().get_latest_raw(slot)
      check_condition(
          isinstance(m, MaybeObject) and m.is_weak and
          isinstance(m.obj, SharedFunctionInfo),
          f"Post-merge slot {slot} missing SharedFunctionInfo (got {m!r})",
      )
      check_condition(
          m.obj.HasBytecodeArray(),
          f"Post-merge slot {slot} ({m.obj}) lost newly compiled BytecodeArray!",
      )

    return TraceResult(
        scenario=config.name,
        schedule=schedule.name,
        mem_mode=mem_mode,
        ilp_order=ilp_order,
        events_requested=len(schedule.events),
        events_applied=obs.events_applied,
        steps_seen=obs.steps_seen,
        sync_points_seen=obs.sync_points_seen,
        safepoints_seen=obs.safepoint_idx,
        concurrent_reader_probes=obs.concurrent_reader_probes,
    )
  finally:
    caller_scope.__cxx_exit__()


# ---------------------------------------------------------------------------
# Comprehensive Adversarial Scenario & Schedule Generators
# ---------------------------------------------------------------------------


def build_adversarial_scenarios() -> List[ScenarioConfig]:
  """Builds diverse topologies covering ALL SFI/ScopeInfo/CP/flag states."""
  C = SlotState.COMPILED
  U = SlotState.UNCOMPILED
  F = SlotState.FLUSHED
  X = SlotState.CLEARED
  P = SlotState.SCOPE_PRESENT
  return [
      # 1. Mixed uncompiled + compiled + standalone eval present
      ScenarioConfig(
          "nested_mixed_eval_present",
          "nested_5",
          (C, U, U, X, P),
          (True, True, True, True),
      ),
      # 2. Top-level CLEARED (CL 2 regression test shape!) + uncompiled children
      ScenarioConfig(
          "nested_slot0_cleared",
          "nested_5",
          (X, U, U, U, X),
          (True, True, True, True),
      ),
      # 3. Top-level FLUSHED + flushed/uncompiled children + eval cleared
      ScenarioConfig(
          "nested_slot0_flushed",
          "nested_5",
          (F, F, U, C, X),
          (True, True, True, False),
      ),
      # 4. All uncompiled in old, all compiled in new (maximizes CopyFrom + lazy races)
      ScenarioConfig(
          "nested_all_uncompiled",
          "nested_5",
          (U, U, U, U, X),
          (True, True, True, True),
      ),
      # 5. All cleared in old (maximizes REALLOCATE_SFI + BG_USE_NEW_SFI + GC)
      ScenarioConfig(
          "nested_all_cleared",
          "nested_5",
          (X, X, X, X, X),
          (True, True, True, True),
      ),
      # 6. Real V8 S1a topology (ConcurrentLazyCompileDuringTryGetScopeInfoForMerge):
      #    outer cleared in old, inner uncompiled in both, outer compiled in new
      ScenarioConfig(
          "nested_s1a_outer_cleared_inner_uncomp",
          "nested_5",
          (X, X, U, X, X),
          (True, True, False, False),
      ),
      # 7. Real V8 S4 topology (ConcurrentLazyCompileAfterRecordScopeInfos):
      #    outer uncompiled in old / uncompiled in new, child cleared in old / compiled in new
      ScenarioConfig(
          "nested_s4_outer_uncomp_child_cleared",
          "nested_5",
          (X, U, X, X, X),
          (True, False, True, False),
      ),
      # 8. Real V8 59121a5f058 topology: old_sfi3 is uncompiled in old_script
      #    while new_script did NOT deserialize slot 3 (so BG allocates NO
      #    persistent handle for old_sfi3, stores Smi::zero(), and GC can clear
      #    old_script->infos()[3] before FG rescan!).
      ScenarioConfig(
          "nested_old_uncomp_new_absent",
          "nested_5",
          (C, U, U, U, X),
          (True, True, False, False),
          new_present_mask=(True, True, True, False),
      ),
      # 9. Real V8 8afc1d18dcf topology: script & outer function have no context
      #    (so uncompiled old_sfi1 has outer_scope_info == THE_HOLE and compiled
      #    new_sfi1's ScopeInfo is not referenced in any constant pool, making
      #    UpdateScopeInfo the sole path forwarding new_si1->outer_scope_info to
      #    old_si0!).
      ScenarioConfig(
          "nested_no_outer_ctx",
          "nested_5",
          (C, U, U, X, X),
          (True, True, False, False),
          parent_has_context=False,
          toplevel_has_context=False,
          new_present_mask=(True, True, False, False, False),
      ),
      # 10. NeedsSourcePositions + MODULE_SCOPE (exercises Script::InitLineEnds,
      #     EnsureSourcePositionsAvailable, and VerifyScopeInfo MODULE_SCOPE path)
      ScenarioConfig(
          "nested_module_srcpos",
          "nested_5",
          (C, U, C, X, X),
          (True, True, False, True),
          needs_source_positions=True,
          module_toplevel=True,
      ),
      # 11. Real V8 weak-clearing regression (R2-1): flushed unpaired old_sfi3
      #     and unpaired standalone eval ScopeInfo in slot 4, both absent from
      #     new_script -> GC between BG and FG clears BOTH old_script->infos()
      #     and new_script->infos() so FG must NOT resurrect collected objects.
      ScenarioConfig(
          "nested_weak_unpaired_gc",
          "nested_5",
          (C, U, U, F, P),
          (True, True, True, False),
          new_present_mask=(True, True, True, False, False),
      ),
      # 12. Deep scope chain WITH parent context
      ScenarioConfig(
          "deep_scope_with_ctx",
          "deep_scope",
          (C, U, X, X),
          (True, True, True),
          parent_has_context=True,
      ),
      # 13. Deep scope chain WITHOUT parent context
      ScenarioConfig(
          "deep_scope_no_ctx",
          "deep_scope",
          (C, F, U, P),
          (True, True, True),
          parent_has_context=False,
      ),
  ]


def build_adversarial_schedules(
    config: ScenarioConfig) -> List[PreemptionSchedule]:
  """Builds targeted, non-vacuous single- and multi-event schedules for ``config``."""
  schedules: List[PreemptionSchedule] = [
      PreemptionSchedule("clean_no_mutation", ()),
  ]
  num_sfi_slots = 4 if config.topology == "nested_5" else 3
  eval_slot = 4 if config.topology == "nested_5" else 3
  total_slots = num_sfi_slots + 1
  present_mask = (
      config.new_present_mask
      if config.new_present_mask is not None else tuple([True] * total_slots))
  if len(present_mask) < total_slots:
    present_mask = present_mask + tuple([True] *
                                        (total_slots - len(present_mask)))

  pure_uncomp_slots = [
      s for s in range(num_sfi_slots)
      if config.old_states[s] == SlotState.UNCOMPILED
  ]
  uncomp_slots = [
      s for s in range(num_sfi_slots)
      if config.old_states[s] in (SlotState.UNCOMPILED, SlotState.FLUSHED)
  ]
  comp_slots = [
      s for s in range(num_sfi_slots)
      if config.old_states[s] == SlotState.COMPILED
  ]
  cleared_slots = [
      s for s in range(num_sfi_slots)
      if config.old_states[s] == SlotState.CLEARED
  ]

  # Exact counts of reachable BG markers/sync points for this topology (prunes
  # guaranteed-vacuous schedules up front while preserving every live path).
  old_live_sfi_count = sum(1 for s in range(num_sfi_slots)
                           if config.old_states[s] != SlotState.CLEARED)
  old_live_any_count = sum(1 for s in range(total_slots)
                           if s < len(config.old_states) and
                           config.old_states[s] != SlotState.CLEARED)
  pair_count = sum(
      1 for s in range(num_sfi_slots)
      if present_mask[s] and config.old_states[s] != SlotState.CLEARED)
  rec_comp_count = sum(
      1 for s in range(num_sfi_slots)
      if present_mask[s] and config.old_states[s] in
      (SlotState.UNCOMPILED, SlotState.FLUSHED) and config.new_compiled_mask[s])
  outer_helper_count = sum(
      1 for s in range(num_sfi_slots)
      if present_mask[s] and config.old_states[s] == SlotState.UNCOMPILED and
      config.new_compiled_mask[s])
  has_bg_fwd = pair_count > 0 or old_live_any_count > 0
  bg_fwd_bc_count = sum(1 for s in range(num_sfi_slots)
                        if present_mask[s] and config.new_compiled_mask[s] and
                        config.old_states[s] != SlotState.COMPILED)

  # 1. Intra-helper SYNCHRONIZATION_POINT preemptions (catches S1a, S1b, S2, S4, M7v, M9v!)
  sync_specs = [
      ("BeforeGetScopeInfoForMerge", min(2, old_live_sfi_count)),
      ("BeforeGetOuterScopeInfo", min(2, outer_helper_count)),
      ("BackgroundMerge_AfterRecordScopeInfos", min(2, old_live_any_count)),
  ]
  for sync_name, max_occ in sync_specs:
    for occ in range(max_occ):
      for s in uncomp_slots[:3]:
        schedules.append(
            PreemptionSchedule(
                f"sync_{sync_name}_{occ}_lazy_s{s}",
                ((("SYNC", sync_name, occ), MutationAction("LAZY_COMPILE",
                                                           s)),),
            ))
        # Race real parsed SharedFunctionInfo::CopyFrom (via
        # CONCURRENT_MERGE_PUBLISH) directly inside the sync point!
        schedules.append(
            PreemptionSchedule(
                f"sync_{sync_name}_{occ}_copyfrom_s{s}",
                ((("SYNC", sync_name, occ),
                  MutationAction("CONCURRENT_MERGE_PUBLISH", s)),),
            ))
      for s in pure_uncomp_slots[:3]:
        # Step-1-only (SetScopeInfo WITHOUT set_feedback_metadata) tests the
        # retry/fallback kAcquireLoad when outer_scope_info == THE_HOLE (M7v, M9v)!
        if sync_name in ("BeforeGetScopeInfoForMerge",
                         "BeforeGetOuterScopeInfo"):
          schedules.append(
              PreemptionSchedule(
                  f"sync_{sync_name}_{occ}_step1_s{s}",
                  ((("SYNC", sync_name, occ),
                    MutationAction("LAZY_COMPILE_STEP1_SCOPE", s)),),
              ))
        # Partial 2-step lazy compile (SetScopeInfo + FeedbackMetadata, NOT yet bytecode!)
        schedules.append(
            PreemptionSchedule(
                f"sync_{sync_name}_{occ}_step12_s{s}",
                (
                    (("SYNC", sync_name, occ),
                     MutationAction("LAZY_COMPILE_STEP1_SCOPE", s)),
                    (("SYNC", sync_name, occ),
                     MutationAction("LAZY_COMPILE_STEP2_FEEDBACK", s)),
                ),
            ))

  # 2. Moving GC at EVERY LocalHeap::Safepoint() index AND all 6 safepoint-capable
  #    MERGE_MODEL_STEP_WITH_SAFEPOINT markers (catches S7, N4, N28, N44, N55!).
  if has_bg_fwd:
    for sp_idx in range(min(4, max(1, bg_fwd_bc_count))):
      schedules.append(
          PreemptionSchedule(
              f"safepoint_{sp_idx}_gc",
              ((("SAFEPOINT", sp_idx), MutationAction("SAFEPOINT_GC", 0)),),
          ))
  schedules.append(
      PreemptionSchedule(
          "sfpt_bg_init_gc",
          ((("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
            MutationAction("SAFEPOINT_GC", 0)),),
      ))
  for occ in (0, 1):
    schedules.append(
        PreemptionSchedule(
            f"sfpt_bg_inspect_{occ}_gc",
            ((("STEP", MergeModelStep.BG_INSPECT_SLOT.value, occ),
              MutationAction("SAFEPOINT_GC", 0)),),
        ))
  if has_bg_fwd:
    schedules.append(
        PreemptionSchedule(
            "sfpt_bg_fwd_gc",
            ((("STEP", MergeModelStep.BG_FORWARD_POINTERS.value, 0),
              MutationAction("SAFEPOINT_GC", 0)),),
        ))
  schedules.append(
      PreemptionSchedule(
          "sfpt_fg_rescan_gc",
          ((("STEP", MergeModelStep.FG_RESCAN_START.value, 0),
            MutationAction("SAFEPOINT_GC", 0)),),
      ))
  # Ensure FG_FORWARD_POINTERS actually runs by triggering a concurrent
  # LAZY_COMPILE or REALLOCATE_SFI between BG and FG right before it!
  if uncomp_slots:
    schedules.append(
        PreemptionSchedule(
            "sfpt_fg_fwd_gc",
            (
                (("BETWEEN_BG_FG", 0),
                 MutationAction("LAZY_COMPILE", uncomp_slots[0])),
                (("STEP", MergeModelStep.FG_FORWARD_POINTERS.value, 0),
                 MutationAction("SAFEPOINT_GC", 0)),
            ),
        ))
  elif cleared_slots:
    schedules.append(
        PreemptionSchedule(
            "sfpt_fg_fwd_gc",
            (
                (("BETWEEN_BG_FG", 0),
                 MutationAction("REALLOCATE_SFI", cleared_slots[0])),
                (("STEP", MergeModelStep.FG_FORWARD_POINTERS.value, 0),
                 MutationAction("SAFEPOINT_GC", 0)),
            ),
        ))
  schedules.append(
      PreemptionSchedule(
          "sfpt_fg_release_gc",
          ((("STEP", MergeModelStep.FG_RELEASE_BACKWARDS_START.value, 0),
            MutationAction("SAFEPOINT_GC", 0)),),
      ))

  # 3. Moving GC & weak clearing at safepoint-safe MERGE_MODEL_STEP points and
  #    between BG and FG (catches S8, S9, R2-1 weak.py!).
  for gc_target in range(total_slots):
    if gc_target < len(config.old_states
                      ) and config.old_states[gc_target] != SlotState.CLEARED:
      schedules.append(
          PreemptionSchedule(
              f"between_bg_fg_gc_clear_s{gc_target}",
              ((("BETWEEN_BG_FG", 0), MutationAction("GC_CLEAR", gc_target)),),
          ))
      if has_bg_fwd:
        schedules.append(
            PreemptionSchedule(
                f"bg_fwd_gc_clear_s{gc_target}",
                ((("STEP", MergeModelStep.BG_FORWARD_POINTERS.value, 0),
                  MutationAction("GC_CLEAR", gc_target)),),
            ))
      schedules.append(
          PreemptionSchedule(
              f"bg_init_gc_clear_s{gc_target}",
              ((("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
                MutationAction("GC_CLEAR", gc_target)),),
          ))

  # 4. Concurrent LAZY_COMPILE / FLUSH / REALLOC / CONCURRENT_MERGE at BG steps
  bg_step_keys: List[Tuple[Any, ...]] = [
      ("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
      ("BETWEEN_BG_FG", 0),
  ]
  if has_bg_fwd:
    bg_step_keys.append(("STEP", MergeModelStep.BG_FORWARD_POINTERS.value, 0))
  for occ in range(num_sfi_slots):
    bg_step_keys.append(("STEP", MergeModelStep.BG_INSPECT_SLOT.value, occ))
  for occ in range(min(2, pair_count)):
    bg_step_keys.append(("STEP", MergeModelStep.BG_PAIR_SFI.value, occ))
  for occ in range(min(2, rec_comp_count)):
    bg_step_keys.append(
        ("STEP", MergeModelStep.BG_RECORD_COMPILED_DATA.value, occ))
  for occ in range(min(2, old_live_any_count)):
    bg_step_keys.append(
        ("STEP", MergeModelStep.BG_RECORD_OLD_SCOPE_INFO.value, occ))

  for key in bg_step_keys:
    klabel = f"{key[1]}_{key[2]}" if len(key) == 3 else f"{key[0]}"
    for s in uncomp_slots[:3]:
      schedules.append(
          PreemptionSchedule(
              f"{klabel}_lazy_s{s}",
              ((key, MutationAction("LAZY_COMPILE", s)),),
          ))
    for s in comp_slots[:2]:
      schedules.append(
          PreemptionSchedule(
              f"{klabel}_flush_s{s}",
              ((key, MutationAction("BYTECODE_FLUSH", s)),),
          ))
    for s in cleared_slots[:3]:
      schedules.append(
          PreemptionSchedule(
              f"{klabel}_realloc_s{s}",
              ((key, MutationAction("REALLOCATE_SFI", s)),),
          ))
      schedules.append(
          PreemptionSchedule(
              f"{klabel}_cmerge_s{s}",
              ((key, MutationAction("CONCURRENT_MERGE_PUBLISH", s)),),
          ))

  # 5. Multi-event compound races (GC_CLEAR -> REALLOCATE_SFI, REALLOCATE -> LAZY_COMPILE,
  #    FLUSH -> GC_CLEAR -> REALLOCATE, LAZY_COMPILE -> FLUSH, concurrent merge + lazy!)
  for s in range(num_sfi_slots):
    if config.old_states[s] == SlotState.CLEARED:
      schedules.append(
          PreemptionSchedule(
              f"compound_realloc_then_lazy_s{s}",
              (
                  (("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
                   MutationAction("REALLOCATE_SFI", s)),
                  (("STEP", MergeModelStep.BG_FORWARD_POINTERS.value, 0),
                   MutationAction("LAZY_COMPILE", s)),
              ),
          ))
      schedules.append(
          PreemptionSchedule(
              f"compound_realloc_mid_bg_s{s}",
              ((("STEP", MergeModelStep.BG_INSPECT_SLOT.value, s),
                MutationAction("REALLOCATE_SFI", s)),),
          ))
      # Specifically exercises ConstantPoolPointerForwarder::VisitSharedFunctionInfo
      # and VisitObjectBoilerplateDescription reading old_script_->infos()->get(id)
      # after slot `s` was concurrently reallocated AFTER BG_INSPECT_SLOT!
      if has_bg_fwd:
        schedules.append(
            PreemptionSchedule(
                f"compound_realloc_at_bg_fwd_s{s}",
                ((("STEP", MergeModelStep.BG_FORWARD_POINTERS.value, 0),
                  MutationAction("REALLOCATE_SFI", s)),),
            ))
    elif config.old_states[s] == SlotState.COMPILED:
      schedules.append(
          PreemptionSchedule(
              f"compound_flush_gc_realloc_s{s}",
              (
                  (("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
                   MutationAction("BYTECODE_FLUSH", s)),
                  (("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
                   MutationAction("GC_CLEAR", s)),
                  (("STEP", MergeModelStep.BG_INSPECT_SLOT.value, s),
                   MutationAction("REALLOCATE_SFI", s)),
              ),
          ))
    else:
      schedules.append(
          PreemptionSchedule(
              f"compound_gc_realloc_lazy_s{s}",
              (
                  (("STEP", MergeModelStep.BG_INIT_TOPLEVEL.value, 0),
                   MutationAction("GC_CLEAR", s)),
                  (("STEP", MergeModelStep.BG_INSPECT_SLOT.value, s),
                   MutationAction("REALLOCATE_SFI", s)),
                  (("BETWEEN_BG_FG", 0), MutationAction("LAZY_COMPILE", s)),
              ),
          ))
      schedules.append(
          PreemptionSchedule(
              f"compound_lazy_then_flush_s{s}",
              (
                  (("STEP", MergeModelStep.BG_INSPECT_SLOT.value, s),
                   MutationAction("LAZY_COMPILE", s)),
                  (("BETWEEN_BG_FG", 0), MutationAction("BYTECODE_FLUSH", s)),
              ),
          ))
      schedules.append(
          PreemptionSchedule(
              f"compound_cmerge_s{s}",
              ((("STEP", MergeModelStep.BG_INSPECT_SLOT.value, s),
                MutationAction("CONCURRENT_MERGE_PUBLISH", s)),),
          ))

  # 6. Per-object/per-slot inter-read preemption between consecutive shared-heap
  #    reads on the exact target old_sfi!
  for s in pure_uncomp_slots[:2]:
    for read_occ in (0, 1):
      schedules.append(
          PreemptionSchedule(
              f"read_slot{s}_name_or_scope_{read_occ}_lazy",
              ((("READ_SLOT", s, "name_or_scope_info_", read_occ),
                MutationAction("LAZY_COMPILE", s)),),
          ))
    schedules.append(
        PreemptionSchedule(
            f"read_slot{s}_name_or_scope_0_step1",
            ((("READ_SLOT", s, "name_or_scope_info_", 0),
              MutationAction("LAZY_COMPILE_STEP1_SCOPE", s)),),
        ))
    schedules.append(
        PreemptionSchedule(
            f"read_slot{s}_outer_or_fb_0_lazy",
            ((("READ_SLOT", s, "outer_scope_info_or_feedback_metadata_", 0),
              MutationAction("LAZY_COMPILE", s)),),
        ))

  # Deduplicate by event sequence so we never run identical schedules twice!
  deduped: List[PreemptionSchedule] = []
  seen_events: Set[Any] = set()
  for sc in schedules:
    if sc.events not in seen_events:
      seen_events.add(sc.events)
      deduped.append(sc)
  return deduped


@dataclass
class SuiteStats:
  total_traces: int = 0
  non_vacuous_traces: int = 0
  vacuous_traces: int = 0
  concurrent_reader_probes: int = 0
  safepoints_hit: int = 0
  sync_points_hit: Set[str] = field(default_factory=set)
  steps_hit: Set[str] = field(default_factory=set)


def run_adversarial_suite(
    bundle: ParsedCxxBundle,
    *,
    scenarios: Optional[List[ScenarioConfig]] = None,
    mem_modes: Tuple[str, ...] = MemoryContext.VALID_MODES,
    include_rtl: bool = True,
    quick: bool = False,
) -> SuiteStats:
  """Runs the full adversarial trace exploration across scenarios & memory modes."""
  sc_list = scenarios if scenarios is not None else build_adversarial_scenarios(
  )
  if quick:
    # Fast subset covering every mutation class, sync point, all 6 safepoint
    # markers, CopyFrom, NeedsSourcePositions (N3), weak unpaired GC (R2-1),
    # S1a/S1b/S3b/S4/M7v/M9v/59121a5f058/8afc1d18dcf topologies, and all 4
    # memory modes (used per mutant in TEST 6!).
    sc_list = sc_list[:11]
  stats = SuiteStats()

  for cfg in sc_list:
    scheds = build_adversarial_schedules(cfg)
    if quick:
      # Keep all sync-point, safepoint-marker, LocalHeap::Safepoint, GC,
      # realloc-at-fwd, compound, per-slot read, and clean schedules while
      # trimming redundant middle-slot duplicates.
      kept: List[PreemptionSchedule] = []
      for sch in scheds:
        if (not sch.events or sch.name.startswith((
            "sync_",
            "sfpt_",
            "safepoint_0",
            "safepoint_1",
            "between_bg_fg_gc_clear_",
            "bg_fwd_gc_clear_",
            "BETWEEN_BG_FG_lazy_",
            "BG_FORWARD_POINTERS_0_lazy_",
            "BG_FORWARD_POINTERS_0_realloc_",
            "compound_realloc_mid_bg_",
            "compound_gc_realloc_lazy_",
            "compound_flush_gc_realloc_",
            "compound_cmerge_",
            "BG_INIT_TOPLEVEL_0_realloc_",
            "BG_INSPECT_SLOT_0_lazy_",
            "BG_INSPECT_SLOT_1_lazy_",
            "BG_INSPECT_SLOT_3_realloc_",
            "read_slot",
        ))):
          kept.append(sch)
      scheds = kept

    for mode in mem_modes:
      for sch in scheds:
        res = run_trace(bundle, cfg, sch, mem_mode=mode, ilp_order="ltr")
        stats.total_traces += 1
        if res.events_requested == 0 or res.events_applied > 0:
          stats.non_vacuous_traces += 1
        else:
          stats.vacuous_traces += 1
        stats.concurrent_reader_probes += res.concurrent_reader_probes
        stats.safepoints_hit += res.safepoints_seen
        stats.sync_points_hit.update(res.sync_points_seen)
        stats.steps_hit.update(res.steps_seen)

    # Also test RTL ILP argument/operand evaluation order on adversarial schedules!
    if include_rtl:
      rtl_scheds = [
          sch for sch in scheds
          if not sch.events or sch.name.startswith(("sync_", "compound_"))
      ][:12]
      for sch in rtl_scheds:
        res = run_trace(
            bundle, cfg, sch, mem_mode="per_load_mp", ilp_order="rtl")
        stats.total_traces += 1
        if res.events_requested == 0 or res.events_applied > 0:
          stats.non_vacuous_traces += 1
        else:
          stats.vacuous_traces += 1
        stats.concurrent_reader_probes += res.concurrent_reader_probes

  return stats
