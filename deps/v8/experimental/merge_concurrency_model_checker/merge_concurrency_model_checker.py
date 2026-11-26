#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Scenario-based concurrency model checker for V8 ``BackgroundMergeTask``.

Executes real Clang C++20 ASTs parsed directly from V8 source files:
* ``src/codegen/compiler.cc`` (``BackgroundMergeTask::BeginMergeInBackground``,
  ``CompleteMergeInForeground``, ``ConstantPoolPointerForwarder``,
  ``VerifyCodeMerge``, ``VerifyConstantPoolEntry``)
* ``src/objects/shared-function-info-inl.h`` (15 ``SharedFunctionInfo`` scope /
  data / bytecode / compilation accessors)
* ``src/objects/shared-function-info.cc`` (``SharedFunctionInfo::CopyFrom``,
  including ``#if DEBUG`` ``memcmp`` checks via ``-DDEBUG``)
* ``src/codegen/background-merge-task.h``

What this tool verifies:
1. Static marker-before-hazard precedence, exact ordered ``MERGE_MODEL_STEP*``
   layout, safepoint-inside-``DisallowGarbageCollection`` hygiene, and
   ``*_TEST_ONLY`` macro hygiene (``TEST 0`` / ``TEST 1``).
2. Bounded multi-slot topologies (``nested_5`` and ``deep_scope``), including
   slot-0 cleared/flushed/uncompiled states, ``ObjectBoilerplateDescription``,
   nested ``FixedArray`` constant pools, ``Smi`` entries, ``UNINITIALIZED_HOLE``,
   ``MODULE_SCOPE``, and ``NeedsSourcePositions`` (``TEST 2`` / ``TEST 4``).
3. Adversarial main-thread interleavings at every ``MERGE_MODEL_STEP*``,
   ``SYNCHRONIZATION_POINT*`` (``BeforeGetScopeInfoForMerge``,
   ``BeforeGetOuterScopeInfo``, ``BackgroundMerge_AfterRecordScopeInfos``),
   ``LocalHeap::Safepoint()`` index, intra-helper shared-heap reads, and every
   store inside ``SharedFunctionInfo::CopyFrom`` across four C++11
   happens-before memory-model schedules (``latest``, ``oldest``,
   ``intermediate``, ``per_load_mp``) and both ILP operand evaluation orders
   (``ltr``, ``rtl``) (``TEST 3`` / ``TEST 5``).
4. Moving GC reachability from real ``HandleScope`` / ``LocalHandleScope``
   frames, ``PersistentHandles``, and JS roots, plus ``DisallowGarbageCollection``
   enforcement and raw-``Tagged<>`` invalidation across safepoints.
5. Regression coverage of 15 historical bugs plus up to 23 audit-derived
   single-fault C++ mutants with strict expected failure classes (``TEST 6``).

Explicitly OUT OF SCOPE (not verified by this tool):
* Two full ``BackgroundMergeTask`` instances racing against each other end to
  end on the same cached ``Script`` (only single-slot concurrent ``CopyFrom`` /
  publish mutations are modeled).
* GC write barriers, incremental/concurrent marking invariants, and V8 sandbox
  ``TrustedPointerTable`` invariants.
* ``BackgroundMergeTask::SetUpOnMainThread``, ``InterpreterData``, Sparkplug
  ``BaselineCode``, ``DebugInfo``, ``UncompiledDataWithPreparseData``,
  concurrent ``ScopeInfo::SetFunctionName``, non-atomic
  ``ScopeInfo::outer_scope_info()`` mutations, and exact
  ``StartPosition + IsDefaultConstructor`` ``UniqueIdInScript()`` calculation.
"""

from __future__ import annotations

import argparse
import copy
import os
import sys
import time
from typing import Any, Dict, List, Optional, Sequence, Tuple, Type

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
  sys.path.insert(0, _THIS_DIR)

from clang_frontend import (
    ParsedCxxBundle,
    parse_v8_merge_bundle,
    read_repo_file,
)
from cxx_interpreter import BoundMergeRuntime
from errors import (
    ConcurrencySyncMismatchError,
    ConcurrentReaderError,
    CxxDcheckFailure,
    CxxHarnessError,
    DanglingHandleError,
    DataRaceError,
    DuplicateScopeInfoError,
    EXPECTED_BG_STEPS,
    EXPECTED_FG_STEPS,
    GcInNoGcScopeError,
    InvariantViolationError,
    MergeModelStep,
    ModelCheckFailure,
    RawPointerAcrossSafepointError,
    UnsafePublicationError,
    check_condition,
)
from hazard_checker import check_cxx_synchronization
from model_checker import (
    PreemptionSchedule,
    ScenarioConfig,
    SlotState,
    build_adversarial_scenarios,
    run_adversarial_suite,
    run_trace,
)
from v8_heap_model import (
    BytecodeArray,
    ExecutionObserver,
    FeedbackMetadata,
    FixedArray,
    Isolate,
    LocalIsolate,
    MakeWeak,
    MaybeObject,
    MemoryContext,
    MemoryOrder,
    ScopeInfo,
    Script,
    SharedFunctionInfo,
    String,
    THE_HOLE,
    V8Heap,
    kAcquireLoad,
    kRelaxedLoad,
    kReleaseStore,
    unwrap_raw,
)


def _replace_exact(src: str,
                   old: str,
                   new: str,
                   *,
                   expected_count: int = 1,
                   label: str = "") -> str:
  """Replaces ``old`` with ``new`` only when it occurs ``expected_count`` times."""
  actual = src.count(old)
  if actual != expected_count:
    tag = f" ({label})" if label else ""
    raise CxxHarnessError(
        f"Exact replacement mismatch{tag}: expected {expected_count} match(es), "
        f"found {actual} for pattern:\n{old!r}")
  return src.replace(old, new)


def _remove_marker_from_ast(
    stmts: List[Dict[str, Any]],
    target_step: str) -> Tuple[List[Dict[str, Any]], int]:
  """Returns a deep-copied AST with every ``MergeModelStep(target_step)`` removed."""
  cloned = copy.deepcopy(stmts)
  removed = 0

  def _strip(node: Any) -> Any:
    nonlocal removed
    if isinstance(node, list):
      out = []
      for item in node:
        if (isinstance(item, dict) and item.get("type") == "MergeModelStep" and
            item.get("step") == target_step):
          removed += 1
          continue
        out.append(_strip(item))
      return out
    if isinstance(node, dict):
      for k, v in list(node.items()):
        if isinstance(v, (list, dict)):
          node[k] = _strip(v)
    return node

  return _strip(cloned), removed


# ---------------------------------------------------------------------------
# TEST 0 & TEST 1: Static Marker & Hazard Verification + Negative Self-Tests
# ---------------------------------------------------------------------------


def run_test_0_sync_check(bundle: ParsedCxxBundle) -> Dict[str, Any]:
  print("[TEST 0: Static C++ Synchronization, Hazard Precedence & Macro Check]")
  info = check_cxx_synchronization(bundle, check_macros=True)
  print(f"  Ordered Markers Verified: {info['total_markers']} "
        f"(BG={len(info['bg_markers'])}, FG={len(info['fg_markers'])})")
  print(
      "  Marker-Before-Hazard, No-GC Safepoint, and *_TEST_ONLY Macros: PASSED")
  return info


def run_test_1_negative_sync_self_tests(bundle: ParsedCxxBundle) -> None:
  print("\n[TEST 1: Negative Synchronization-Checker Self-Tests]")

  # 1A. Every single one of the 14 MERGE_MODEL_STEP* markers deleted individually.
  all_markers = list(EXPECTED_BG_STEPS) + list(EXPECTED_FG_STEPS)
  for step in all_markers:
    is_bg = step.startswith("BG_")
    mut_bg = bundle.bg_stmts
    mut_fg = bundle.fg_stmts
    if is_bg:
      mut_bg, count = _remove_marker_from_ast(bundle.bg_stmts, step)
    else:
      mut_fg, count = _remove_marker_from_ast(bundle.fg_stmts, step)
    check_condition(
        count >= 1,
        f"Expected marker {step} to exist in AST before deletion",
        CxxHarnessError,
    )
    mut_bundle = ParsedCxxBundle(
        bg_stmts=mut_bg,
        fg_stmts=mut_fg,
        fwd_methods=bundle.fwd_methods,
        vcm_stmts=bundle.vcm_stmts,
        vcm_helpers=bundle.vcm_helpers,
        sfi_methods=bundle.sfi_methods,
        raw_compiler_cc=bundle.raw_compiler_cc,
        raw_sfi_inl=bundle.raw_sfi_inl,
        raw_sfi_cc=bundle.raw_sfi_cc,
        git_rev=bundle.git_rev,
    )
    rejected = False
    try:
      check_cxx_synchronization(mut_bundle, check_macros=False)
    except ConcurrencySyncMismatchError:
      rejected = True
    check_condition(
        rejected,
        f"check_cxx_synchronization failed to reject deleting marker {step}!",
    )

  # 1B. Bogus extra marker prepended to FG.
  bogus_fg = [{
      "type": "MergeModelStep",
      "step": "FG_BOGUS_MARKER",
      "allow_safepoint": False
  }] + list(bundle.fg_stmts)
  bogus_bundle = ParsedCxxBundle(
      bg_stmts=bundle.bg_stmts,
      fg_stmts=bogus_fg,
      fwd_methods=bundle.fwd_methods,
      vcm_stmts=bundle.vcm_stmts,
      vcm_helpers=bundle.vcm_helpers,
      sfi_methods=bundle.sfi_methods,
      raw_compiler_cc=bundle.raw_compiler_cc,
      raw_sfi_inl=bundle.raw_sfi_inl,
      raw_sfi_cc=bundle.raw_sfi_cc,
      git_rev=bundle.git_rev,
  )
  rejected = False
  try:
    check_cxx_synchronization(bogus_bundle, check_macros=False)
  except ConcurrencySyncMismatchError:
    rejected = True
  check_condition(
      rejected, "check_cxx_synchronization failed to reject FG_BOGUS_MARKER!")

  # 1C. Real Clang-parsed C++ negative layout, control-flow, no_gc, and macro tests.
  c_orig = bundle.raw_compiler_cc
  bmt_orig = read_repo_file(
      "src/codegen/background-merge-task.h", git_rev=bundle.git_rev)

  # CL 1 omits kAcquireLoad on the toplevel infos()->get() load; CL 2+ adds it.
  toplevel_get_line = (
      "        old_script->infos()->get(kFunctionLiteralIdTopLevel, kAcquireLoad);"
      if "old_script->infos()->get(kFunctionLiteralIdTopLevel, kAcquireLoad);"
      in c_orig else
      "        old_script->infos()->get(kFunctionLiteralIdTopLevel);")

  real_cxx_negatives: List[Tuple[str, str]] = [
      (
          "swap BG_PAIR_SFI <-> BG_USE_NEW_SFI",
          _replace_exact(
              _replace_exact(
                  _replace_exact(
                      c_orig,
                      "MERGE_MODEL_STEP(BG_PAIR_SFI);",
                      "__TMP_SWAP_MARKER__;",
                      label="swap1_a",
                  ),
                  "MERGE_MODEL_STEP(BG_USE_NEW_SFI);",
                  "MERGE_MODEL_STEP(BG_PAIR_SFI);",
                  label="swap1_b",
              ),
              "__TMP_SWAP_MARKER__;",
              "MERGE_MODEL_STEP(BG_USE_NEW_SFI);",
              label="swap1_c",
          ),
      ),
      (
          "swap BG_INSPECT_SLOT <-> BG_RECORD_COMPILED_DATA",
          _replace_exact(
              _replace_exact(
                  _replace_exact(
                      c_orig,
                      "MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INSPECT_SLOT);",
                      "__TMP_SWAP_MARKER__;",
                      label="swap2_a",
                  ),
                  "MERGE_MODEL_STEP(BG_RECORD_COMPILED_DATA);",
                  "MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INSPECT_SLOT);",
                  label="swap2_b",
              ),
              "__TMP_SWAP_MARKER__;",
              "MERGE_MODEL_STEP(BG_RECORD_COMPILED_DATA);",
              label="swap2_c",
          ),
      ),
      (
          "move BG_INIT_TOPLEVEL after old_script->infos()->get",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);\n"
              "  {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_toplevel_sfi =\n"
              f"{toplevel_get_line}",
              "  {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_toplevel_sfi =\n"
              f"{toplevel_get_line}\n"
              "    MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              label="bg_init_after_get",
          ),
      ),
      (
          "unmarked hazard before first marker",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              "  forwarder.RecordScopeInfos(old_script->infos()->get(1, kAcquireLoad));\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              label="unmarked_before_first",
          ),
      ),
      (
          "unmarked premature publish in FG rescan loop",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);\n"
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_info = old_script->infos()->get(i);\n"
              "    Tagged<MaybeObject> maybe_new_info = new_script->infos()->get(i);",
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);\n"
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_info = old_script->infos()->get(i);\n"
              "    Tagged<MaybeObject> maybe_new_info = new_script->infos()->get(i);\n"
              "    old_script->infos()->set(i, maybe_new_info, kReleaseStore);",
              label="premature_publish_rescan",
          ),
      ),
      (
          "second unmarked CopyFrom after FG_COPY_FROM",
          _replace_exact(
              c_orig,
              "          MERGE_MODEL_STEP(FG_COPY_FROM);\n"
              "          sfi->CopyFrom(*compiled_data_it->new_sfi, isolate);",
              "          MERGE_MODEL_STEP(FG_COPY_FROM);\n"
              "          sfi->CopyFrom(*compiled_data_it->new_sfi, isolate);\n"
              "          sfi->CopyFrom(*compiled_data_it->new_sfi, isolate);",
              label="second_copy_from",
          ),
      ),
      (
          "unmarked RangeFor hazard at root",
          _replace_exact(
              c_orig,
              "  persistent_handles_ = local_heap->DetachPersistentHandles();",
              "  for (DirectHandle<SharedFunctionInfo> x : used_new_sfis_) {\n"
              "    forwarder.UpdateScopeInfo(*x);\n"
              "  }\n"
              "  persistent_handles_ = local_heap->DetachPersistentHandles();",
              label="unmarked_range_for",
          ),
      ),
      (
          "unmarked While + If.init hazard at root",
          _replace_exact(
              c_orig,
              "  persistent_handles_ = local_heap->DetachPersistentHandles();",
              "  while (false) {\n"
              "    if (Tagged<MaybeObject> m = old_script->infos()->get(0, kAcquireLoad); m.IsWeak()) {}\n"
              "  }\n"
              "  persistent_handles_ = local_heap->DetachPersistentHandles();",
              label="unmarked_while_if_init",
          ),
      ),
      (
          "MERGE_MODEL_STEP_WITH_SAFEPOINT inside DisallowGarbageCollection",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);\n"
              "  {\n"
              "    DisallowGarbageCollection no_gc;",
              "  {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              label="safepoint_in_no_gc",
          ),
      ),
      (
          "production SYNCHRONIZATION_POINT macro regression",
          _replace_exact(
              c_orig,
              '#else\n#define MERGE_MODEL_STEP(step_name) \\\n'
              '  SYNCHRONIZATION_POINT_TEST_ONLY("BackgroundMerge_" #step_name)',
              '#else\n#define MERGE_MODEL_STEP(step_name) \\\n'
              '  SYNCHRONIZATION_POINT("BackgroundMerge_" #step_name)',
              label="prod_macro_regression",
          ),
      ),
      (
          "H1: TryCast out-param taint propagation before FG_RESCAN_START",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              "  Tagged<SharedFunctionInfo> leaked;\n"
              "  if (TryCast(*toplevel_sfi_from_cached_script_.ToHandleChecked(), &leaked)) {\n"
              "    leaked->set_age(0);\n"
              "  }\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              label="h1_trycast_outparam",
          ),
      ),
      (
          "H2: benign Decl before block leaking coverage to sibling unmarked block",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);\n"
              "  {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_toplevel_sfi =\n"
              f"{toplevel_get_line}\n"
              "    if (maybe_old_toplevel_sfi.IsWeak()) {\n"
              "      Tagged<SharedFunctionInfo> old_toplevel_sfi = Cast<SharedFunctionInfo>(\n"
              "          maybe_old_toplevel_sfi.GetHeapObjectAssumeWeak());\n"
              "      toplevel_sfi_from_cached_script_ =\n"
              "          local_heap->NewPersistentHandle(old_toplevel_sfi);\n"
              "    }\n"
              "  }",
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);\n"
              "  int benign_pad = 0;\n"
              "  {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_toplevel_sfi =\n"
              f"{toplevel_get_line}\n"
              "    if (maybe_old_toplevel_sfi.IsWeak()) {\n"
              "      Tagged<SharedFunctionInfo> old_toplevel_sfi = Cast<SharedFunctionInfo>(\n"
              "          maybe_old_toplevel_sfi.GetHeapObjectAssumeWeak());\n"
              "      toplevel_sfi_from_cached_script_ =\n"
              "          local_heap->NewPersistentHandle(old_toplevel_sfi);\n"
              "    }\n"
              "  }\n"
              "  {\n"
              "    old_script->infos()->get(benign_pad, kAcquireLoad);\n"
              "  }",
              label="h2_sibling_block_leak",
          ),
      ),
      (
          "H3: early maybe_toplevel_sfi read before FG_RESCAN_START",
          _replace_exact(
              _replace_exact(
                  c_orig,
                  "  Tagged<MaybeObject> maybe_toplevel_sfi =\n"
                  "      old_script->infos()->get(kFunctionLiteralIdTopLevel);",
                  "  maybe_toplevel_sfi =\n"
                  "      old_script->infos()->get(kFunctionLiteralIdTopLevel);",
                  expected_count=1,
                  label="h3_early_toplevel_a",
              ),
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              "  Tagged<MaybeObject> maybe_toplevel_sfi =\n"
              "      old_script->infos()->get(kFunctionLiteralIdTopLevel);\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              label="h3_early_toplevel_b",
          ),
      ),
      (
          "H4: extra unmarked old_script->infos()->get inside nested if in BG loop",
          _replace_exact(
              c_orig,
              "    if (maybe_new_sfi.IsWeak() &&\n"
              "        Is<SharedFunctionInfo>(maybe_new_sfi.GetHeapObjectAssumeWeak())) {",
              "    if (maybe_new_sfi.IsWeak() &&\n"
              "        Is<SharedFunctionInfo>(maybe_new_sfi.GetHeapObjectAssumeWeak())) {\n"
              "      old_script->infos()->get(i, kAcquireLoad);",
              label="h4_nested_extra_get",
          ),
      ),
      (
          "L3: unlisted shared SFI method unique_id() before FG_RESCAN_START",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              "  toplevel_sfi_from_cached_script_.ToHandleChecked()->unique_id();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              label="l3_unlisted_method",
          ),
      ),
      (
          "L3: unmarked hazard inside lambda before BG_INIT_TOPLEVEL",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              "  [&]() { old_script->infos()->get(0, kAcquireLoad); }();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              label="l3_lambda_hazard",
          ),
      ),
      (
          "R3: unmarked hazard hidden inside CHECK() arg before BG_INIT_TOPLEVEL",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              "  CHECK(old_script->infos()->get(0, kAcquireLoad).IsWeak());\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INIT_TOPLEVEL);",
              label="r3_check_arg_hazard",
          ),
      ),
      (
          "R3: unmarked SharedFunctionInfo::length() before FG_RESCAN_START",
          _replace_exact(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              "  toplevel_sfi_from_cached_script_.ToHandleChecked()->length();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);",
              label="r3_sfi_length_hazard",
          ),
      ),
      (
          "R3: impure old_script_ access inside ConstantPoolPointerForwarder::HasAnythingToForward",
          _replace_exact(
              c_orig,
              "  bool HasAnythingToForward() const {\n"
              "    return has_shared_function_info_to_forward_ ||\n"
              "           !scope_infos_to_update_.empty();\n"
              "  }",
              "  bool HasAnythingToForward() const {\n"
              "    old_script_->infos()->get(0);\n"
              "    return has_shared_function_info_to_forward_ ||\n"
              "           !scope_infos_to_update_.empty();\n"
              "  }",
              label="r3_impure_forwarder_helper",
          ),
      ),
  ]

  for desc, mutated_cc in real_cxx_negatives:
    mb = parse_v8_merge_bundle(
        git_rev=bundle.git_rev,
        compiler_content=mutated_cc,
        sfi_inl_content=bundle.raw_sfi_inl,
        sfi_cc_content=bundle.raw_sfi_cc,
        bmt_h_content=bmt_orig,
    )
    rejected = False
    try:
      check_cxx_synchronization(mb, check_macros=True)
    except ConcurrencySyncMismatchError:
      rejected = True
    check_condition(
        rejected,
        f"Negative sync self-test failed to reject: {desc}",
    )

  # L3: non-TEST_ONLY SYNCHRONIZATION_POINT in shared-function-info-inl.h.
  sfi_inl_macro_negatives: List[Tuple[str, str]] = []
  if 'SYNCHRONIZATION_POINT_TEST_ONLY("BeforeGetScopeInfoForMerge")' in bundle.raw_sfi_inl:
    sfi_inl_macro_negatives.append((
        "L3: non-TEST_ONLY BeforeGetScopeInfoForMerge in shared-function-info-inl.h",
        _replace_exact(
            bundle.raw_sfi_inl,
            'SYNCHRONIZATION_POINT_TEST_ONLY("BeforeGetScopeInfoForMerge")',
            'SYNCHRONIZATION_POINT("BeforeGetScopeInfoForMerge")',
            label="l3_sfi_inl_macro",
        ),
    ))
  else:
    sfi_inl_macro_negatives.append((
        "L3: non-TEST_ONLY SYNCHRONIZATION_POINT in shared-function-info-inl.h",
        _replace_exact(
            bundle.raw_sfi_inl,
            "Tagged<ScopeInfo> SharedFunctionInfo::TryGetScopeInfoForMerge() const {",
            'Tagged<ScopeInfo> SharedFunctionInfo::TryGetScopeInfoForMerge() const {\n'
            '  SYNCHRONIZATION_POINT("BogusNonTestOnlySyncPoint");',
            label="l3_sfi_inl_macro_fallback",
        ),
    ))

  if 'SYNCHRONIZATION_POINT("BeforeGetOuterScopeInfo")' in bundle.raw_sfi_inl:
    sfi_inl_macro_negatives.append((
        "R3: non-TEST_ONLY SYNCHRONIZATION_POINT_WITH_SAFEPOINT on BeforeGetOuterScopeInfo",
        _replace_exact(
            bundle.raw_sfi_inl,
            'SYNCHRONIZATION_POINT("BeforeGetOuterScopeInfo")',
            'SYNCHRONIZATION_POINT_WITH_SAFEPOINT("BeforeGetOuterScopeInfo")',
            label="r3_sfi_inl_safepoint_macro",
        ),
    ))

  for desc, mutated_inl in sfi_inl_macro_negatives:
    mb = parse_v8_merge_bundle(
        git_rev=bundle.git_rev,
        compiler_content=c_orig,
        sfi_inl_content=mutated_inl,
        sfi_cc_content=bundle.raw_sfi_cc,
        bmt_h_content=bmt_orig,
    )
    rejected = False
    try:
      check_cxx_synchronization(mb, check_macros=True)
    except ConcurrencySyncMismatchError:
      rejected = True
    check_condition(
        rejected,
        f"Negative sync self-test failed to reject: {desc}",
    )

  real_count = len(real_cxx_negatives) + len(sfi_inl_macro_negatives)
  total_neg = len(all_markers) + 1 + real_count
  print(f"  Negative Sync Self-Tests Rejected: {total_neg}/{total_neg} "
        f"({len(all_markers)} single-marker deletions + bogus marker + "
        f"{real_count} real Clang layout/taint/no_gc/macro tests)")


# ---------------------------------------------------------------------------
# TEST 2: Baseline Clean Merge Across All Topologies
# ---------------------------------------------------------------------------


def run_test_2_clean_merges(bundle: ParsedCxxBundle) -> None:
  print("\n[TEST 2: Baseline Clean Merge Across All Topologies & Flags]")
  scenarios = build_adversarial_scenarios()
  clean = PreemptionSchedule("clean", ())
  all_steps: set[str] = set()
  for cfg in scenarios:
    res = run_trace(bundle, cfg, clean, mem_mode="latest", ilp_order="ltr")
    all_steps.update(res.steps_seen)
  check_condition(
      MergeModelStep.BG_INIT_TOPLEVEL.value in all_steps and
      MergeModelStep.BG_INSPECT_SLOT.value in all_steps and
      MergeModelStep.BG_FORWARD_POINTERS.value in all_steps and
      MergeModelStep.FG_RESCAN_START.value in all_steps and
      MergeModelStep.FG_RELEASE_BACKWARDS_START.value in all_steps,
      f"Missing expected baseline steps in clean merge run: {sorted(all_steps)}",
  )
  print(
      f"  Clean Scenarios Verified: {len(scenarios)}/{len(scenarios)} "
      f"(including MODULE_SCOPE, NeedsSourcePositions, ObjectBoilerplateDescription, "
      f"nested FixedArray, Smi, UNINITIALIZED_HOLE, deep_scope +/- ctx)")
  print("  Baseline Clean Merge & Real VerifyCodeMerge: PASSED")


# ---------------------------------------------------------------------------
# TEST 3: Adversarial Interleaving & C++11 Memory-Model Exploration
# ---------------------------------------------------------------------------


def run_test_3_adversarial(bundle: ParsedCxxBundle,
                           *,
                           quick: bool = False) -> None:
  mode_label = "QUICK" if quick else "FULL"
  print(
      f"\n[TEST 3: Adversarial Interleaving & C++11 Memory-Model Exploration ({mode_label})]"
  )
  t0 = time.time()
  stats = run_adversarial_suite(bundle, quick=quick)
  elapsed = time.time() - t0
  print(f"  Elapsed: {elapsed:.1f}s")
  print(f"  Non-vacuous Traces: {stats.non_vacuous_traces} "
        f"(vacuous: {stats.vacuous_traces}, total: {stats.total_traces})")
  print(
      f"  Concurrent Reader Probes (mid-CopyFrom + FG release): {stats.concurrent_reader_probes}"
  )
  print(f"  LocalHeap::Safepoint() Hits: {stats.safepoints_hit}")
  print(
      f"  Sync Points Hit ({len(stats.sync_points_hit)}): {sorted(stats.sync_points_hit)}"
  )
  print(f"  MERGE_MODEL_STEP Markers Hit: {len(stats.steps_hit)}/14")

  check_condition(
      stats.non_vacuous_traces > 0 and stats.concurrent_reader_probes > 0,
      "Adversarial suite did not run non-vacuous traces or concurrent reader probes!",
  )
  check_condition(
      stats.safepoints_hit > 0,
      "Expected LocalHeap::Safepoint() to execute during adversarial suite!",
  )
  expected_syncs = {"BeforeGetScopeInfoForMerge", "BeforeGetOuterScopeInfo"}
  if "BackgroundMerge_AfterRecordScopeInfos" in bundle.raw_compiler_cc:
    expected_syncs.add("BackgroundMerge_AfterRecordScopeInfos")
  missing_syncs = expected_syncs - stats.sync_points_hit
  check_condition(
      not missing_syncs,
      f"Adversarial suite never hit required SYNCHRONIZATION_POINT(s): {sorted(missing_syncs)}",
  )
  check_condition(
      len(stats.steps_hit) == 14,
      f"Expected all 14 MERGE_MODEL_STEP markers to execute, only hit {sorted(stats.steps_hit)}",
  )
  print(
      "  Adversarial Suite (Memory Ordering, GC/Handles, Mid-CopyFrom Readers): PASSED"
  )


# ---------------------------------------------------------------------------
# TEST 4: Exhaustive 8/8 Distinct Compilation Mask Sweep
# ---------------------------------------------------------------------------


def run_test_4_compile_masks(bundle: ParsedCxxBundle) -> None:
  print("\n[TEST 4: 8/8 Distinct Compilation Mask Distribution Sweep]")
  clean = PreemptionSchedule("clean", ())
  seen_configs: set[Tuple[str, ...]] = set()
  for mask_val in range(8):
    # Bit 0 is NOT forced on: tests all 8 distinct combinations of
    # COMPILED vs UNCOMPILED across slots 0, 1, 2 (including uncompiled top-level!).
    old_states = (
        SlotState.COMPILED if (mask_val & 1) else SlotState.UNCOMPILED,
        SlotState.COMPILED if (mask_val & 2) else SlotState.UNCOMPILED,
        SlotState.COMPILED if (mask_val & 4) else SlotState.UNCOMPILED,
        SlotState.UNCOMPILED,
        SlotState.CLEARED,
    )
    check_condition(old_states not in seen_configs, "Duplicate mask in TEST 4!")
    seen_configs.add(old_states)
    cfg = ScenarioConfig(
        name=f"mask_{mask_val:03b}",
        topology="nested_5",
        old_states=old_states,
        new_compiled_mask=(True, True, True, True),
    )
    run_trace(bundle, cfg, clean, mem_mode="latest", ilp_order="ltr")
  print(
      f"  Distinct Compilation Distributions Verified: {len(seen_configs)}/8 (with CP, scopes, state_, return handle & VerifyCodeMerge)"
  )


# ---------------------------------------------------------------------------
# TEST 5: Memory Ordering, ILP & Intra-Helper Preemption Micro-Tests
# ---------------------------------------------------------------------------


def run_test_5_micro_tests(bundle: ParsedCxxBundle) -> None:
  print(
      "\n[TEST 5: C++11 Memory Ordering, ILP & Intra-Helper Preemption Micro-Tests]"
  )

  # 5a / 5b: kRelaxedLoad vs kAcquireLoad synchronization on SFI::script (both in latest mode!)
  mc = MemoryContext(mode="latest")
  heap = V8Heap(mc)
  isolate = Isolate(heap)
  s0 = Script(heap, [], label="s0")
  s1 = Script(heap, [], label="s1")
  sfi = SharedFunctionInfo(
      heap,
      1,
      s0,
      name_or_scope_info=String(heap, "f"),
      outer_scope_or_fb=THE_HOLE,
      bytecode=None,
      unique_id_in_script=1,
      label="micro_sfi",
  )
  mc.switch_thread(mc.main_thread)
  sfi.set_script(s1, kReleaseStore)
  write_ts = sfi._script_loc.history[-1].ts

  mc.switch_thread(mc.bg_thread)
  _ = sfi.script(kRelaxedLoad)
  sync_relaxed = write_ts in mc.bg_thread.view
  check_condition(
      not sync_relaxed,
      f"Expected kRelaxedLoad NOT to synchronize (got {sync_relaxed})")
  print(f"  [5a] Relaxed Load Synchronized: {sync_relaxed} (Expected: False)")

  _ = sfi.script(kAcquireLoad)
  sync_acquire = write_ts in mc.bg_thread.view
  check_condition(sync_acquire,
                  f"Expected kAcquireLoad to synchronize (got {sync_acquire})")
  print(f"  [5b] Acquire Load Synchronized: {sync_acquire} (Expected: True)")

  # 5c: FixedArray slot acquire/release synchronization
  mc_arr = MemoryContext(mode="latest")
  h_arr = V8Heap(mc_arr)
  arr = FixedArray(
      h_arr, [MaybeObject(None), MaybeObject(None)], label="micro_arr")
  s_arr = Script(h_arr, [], label="s_arr")
  sfi_arr = SharedFunctionInfo(
      h_arr,
      1,
      s_arr,
      name_or_scope_info=String(h_arr, "g"),
      outer_scope_or_fb=THE_HOLE,
      bytecode=None,
      unique_id_in_script=1,
      label="arr_sfi",
  )
  mc_arr.switch_thread(mc_arr.main_thread)
  arr.set(1, MakeWeak(sfi_arr), kReleaseStore)
  arr_ts = arr._slots[1].history[-1].ts
  mc_arr.switch_thread(mc_arr.bg_thread)
  _ = arr.get(1, kAcquireLoad)
  arr_sync = arr_ts in mc_arr.bg_thread.view
  check_condition(arr_sync,
                  "Expected FixedArray::get(kAcquireLoad) to synchronize!")
  print(
      f"  [5c] FixedArray kAcquireLoad Synchronized: {arr_sync} (Expected: True)"
  )

  # 5d: Genuine C++11 Message Passing (MP) litmus anomaly under per_load_mp
  #     (two separate fields: name_or_scope_info_ + trusted_data_, NO split-field hack!)
  mc_mp = MemoryContext(mode="per_load_mp")
  h_mp = V8Heap(mc_mp)
  sc_mp = Script(h_mp, [], label="sc_mp")
  si_mp = ScopeInfo(h_mp, unique_id=10, label="si_mp")
  bc_mp = BytecodeArray(
      h_mp, FixedArray(h_mp, [], label="cp_mp"), label="bc_mp")
  sfi_mp = SharedFunctionInfo(
      h_mp,
      1,
      sc_mp,
      name_or_scope_info=String(h_mp, "mp"),
      outer_scope_or_fb=THE_HOLE,
      bytecode=None,
      unique_id_in_script=10,
      label="sfi_mp",
  )
  mc_mp.switch_thread(mc_mp.main_thread)
  # Broken publisher: relaxed stores instead of release-store on code install!
  sfi_mp._name_or_scope_loc.write(
      si_mp, MemoryOrder.RELAXED, thread=mc_mp.main_thread)
  sfi_mp._trusted_loc.write(
      bc_mp, MemoryOrder.RELAXED, thread=mc_mp.main_thread)

  mc_mp.switch_thread(mc_mp.bg_thread)
  mc_mp.bg_thread.reset_mp_window()
  saw_compiled = sfi_mp._trusted_loc.read(
      MemoryOrder.ACQUIRE, thread=mc_mp.bg_thread) is not None
  saw_scope = isinstance(
      sfi_mp._name_or_scope_loc.read(
          MemoryOrder.ACQUIRE, thread=mc_mp.bg_thread), ScopeInfo)
  check_condition(
      saw_compiled and not saw_scope,
      f"Expected per_load_mp litmus anomaly (saw_compiled=True, saw_scope=False), "
      f"got saw_compiled={saw_compiled}, saw_scope={saw_scope}",
  )
  print(
      f"  [5d] C++11 MP Litmus (relaxed publish -> acquire reader): "
      f"saw_compiled={saw_compiled}, saw_scope={saw_scope} (anomaly detected)")

  # 5e.1: ILP operand/argument evaluation order ('rtl' vs 'ltr') invariance
  cfg0 = build_adversarial_scenarios()[0]
  clean = PreemptionSchedule("clean", ())
  res_ltr = run_trace(bundle, cfg0, clean, mem_mode="latest", ilp_order="ltr")
  res_rtl = run_trace(bundle, cfg0, clean, mem_mode="latest", ilp_order="rtl")
  check_condition(
      res_ltr.steps_seen == res_rtl.steps_seen,
      f"ILP rtl diverged from ltr: {res_rtl.steps_seen} != {res_ltr.steps_seen}",
  )
  print("  [5e.1] ILP Evaluation Order ('rtl' vs 'ltr'): INVARIANT")

  # 5e.2: Intra-helper preemption at BeforeGetScopeInfoForMerge & BeforeGetOuterScopeInfo
  #       using V8's REAL 3-step lazy compile (SetScopeInfo -> set_feedback_metadata -> bytecode)
  mc_sp = MemoryContext(mode="latest")
  h_sp = V8Heap(mc_sp)
  iso_sp = Isolate(h_sp)
  rt_sp = BoundMergeRuntime(bundle, h_sp, iso_sp, LocalIsolate(h_sp))
  sc_sp = Script(h_sp, [], label="sc_sp")
  expected_outer = ScopeInfo(h_sp, 30, context_length=4, label="outer_30")
  own_scope = ScopeInfo(
      h_sp, 31, outer_scope=expected_outer, context_length=4, label="own_31")
  fb_sp = FeedbackMetadata(h_sp, tag="fb_31")
  bc_sp = BytecodeArray(
      h_sp, FixedArray(h_sp, [], label="cp_31"), label="bc_31")
  sfi_m1 = SharedFunctionInfo(
      h_sp,
      1,
      sc_sp,
      name_or_scope_info=String(h_sp, "m1"),
      outer_scope_or_fb=expected_outer,
      bytecode=None,
      unique_id_in_script=31,
      label="sfi_m1",
  )
  sfi_m2 = SharedFunctionInfo(
      h_sp,
      2,
      sc_sp,
      name_or_scope_info=String(h_sp, "m2"),
      outer_scope_or_fb=expected_outer,
      bytecode=None,
      unique_id_in_script=31,
      label="sfi_m2",
  )
  syncs_hit: List[str] = []

  class _SyncPreemptObserver(ExecutionObserver):

    def on_sync_point(self, name: str, allow_safepoint: bool) -> None:
      syncs_hit.append(name)
      target = sfi_m1 if name == "BeforeGetScopeInfoForMerge" else sfi_m2
      prev = mc_sp.switch_thread(mc_sp.main_thread)
      try:
        # REAL V8 lazy compile order: SetScopeInfo -> set_feedback_metadata -> bytecode!
        target.SetScopeInfo(own_scope)
        rt_sp.call_sfi_method(target, "set_feedback_metadata", fb_sp,
                              kReleaseStore)
        rt_sp.call_sfi_method(target, "set_bytecode_array", bc_sp)
      finally:
        mc_sp.switch_thread(prev)

  mc_sp.observer = _SyncPreemptObserver()
  mc_sp.switch_thread(mc_sp.bg_thread)
  got_for_merge = unwrap_raw(
      rt_sp.call_sfi_method(sfi_m1, "TryGetScopeInfoForMerge"))
  got_outer = unwrap_raw(rt_sp.call_sfi_method(sfi_m2, "TryGetOuterScopeInfo"))
  check_condition(
      "BeforeGetScopeInfoForMerge" in syncs_hit and
      "BeforeGetOuterScopeInfo" in syncs_hit,
      f"Expected both SFI sync points to fire, saw {syncs_hit}",
  )
  check_condition(
      got_for_merge is own_scope,
      f"TryGetScopeInfoForMerge() returned {got_for_merge!r}, expected {own_scope!r}!",
  )
  check_condition(
      got_outer is expected_outer,
      f"TryGetOuterScopeInfo() returned {got_outer!r}, expected {expected_outer!r}!",
  )
  print(
      "  [5e.2] Real 3-Step Lazy Compile Mid-Helper Preemption: "
      "TryGetScopeInfoForMerge()==own_scope, TryGetOuterScopeInfo()==expected_outer (SAFE)"
  )

  # 5e.3: Intra-expression TOCTOU between HasOuterScopeInfo() and GetOuterScopeInfo()
  mc_t = MemoryContext(mode="latest")
  h_t = V8Heap(mc_t)
  iso_t = Isolate(h_t)
  rt_t = BoundMergeRuntime(bundle, h_t, iso_t, LocalIsolate(h_t))
  sc_t = Script(h_t, [], label="sc_t")
  outer_t = ScopeInfo(h_t, 40, context_length=4, label="outer_40")
  own_t = ScopeInfo(
      h_t, 41, outer_scope=outer_t, context_length=4, label="own_41")
  fb_t = FeedbackMetadata(h_t, tag="fb_41")
  sfi_t = SharedFunctionInfo(
      h_t,
      1,
      sc_t,
      name_or_scope_info=String(h_t, "toctou"),
      outer_scope_or_fb=outer_t,
      bytecode=None,
      unique_id_in_script=41,
      label="sfi_toctou",
  )

  class _ToctouObserver(ExecutionObserver):

    def __init__(self):
      self.fired = False

    def on_heap_read(self, owner: Any, field_name: str, order: str) -> None:
      if not self.fired and owner is sfi_t and field_name == "outer_scope_info_or_feedback_metadata_":
        self.fired = True
        prev = mc_t.switch_thread(mc_t.main_thread)
        try:
          sfi_t.SetScopeInfo(own_t)
          rt_t.call_sfi_method(sfi_t, "set_feedback_metadata", fb_t,
                               kReleaseStore)
        finally:
          mc_t.switch_thread(prev)

  mc_t.observer = _ToctouObserver()
  mc_t.switch_thread(mc_t.bg_thread)
  caught_toctou = False
  try:
    if rt_t.call_sfi_method(sfi_t, "HasOuterScopeInfo"):
      rt_t.call_sfi_method(sfi_t, "GetOuterScopeInfo")
  except CxxDcheckFailure:
    caught_toctou = True
  check_condition(
      caught_toctou,
      "Expected mid-expression TOCTOU (HasOuterScopeInfo -> GetOuterScopeInfo) to raise CxxDcheckFailure!",
  )
  print(
      "  [5e.3] Intra-Expression Micro-Load TOCTOU (HasOuterScopeInfo -> GetOuterScopeInfo): CAUGHT"
  )


# ---------------------------------------------------------------------------
# TEST 6: Historical Bug Reintroduction & Audit Mutant Verification
# ---------------------------------------------------------------------------


def _build_fault_catalog(
    c_orig: str, inl_orig: str, cc_orig: str
) -> List[Tuple[str, str, str, str, Tuple[Type[ModelCheckFailure], ...], bool]]:
  """Returns ``(name, compiler_cc, sfi_inl, sfi_cc, expected_exc_types, keep_sync_check)``.

  Every mutation uses ``_replace_exact`` (fails closed on partial or multi-site
  matches), preserves real ``MERGE_MODEL_STEP*`` markers, and declares the exact
  ``ModelCheckFailure`` subclass(es) that must catch it. Any ``CxxHarnessError``
  or Python runtime error fails ``TEST 6`` immediately.
  """
  rx = _replace_exact
  rec_ret = "return false;" if "bool RecordScopeInfos(" in c_orig else "return;"
  is_cl3_smi = "new_script->infos()->set(i, Smi::zero());" in c_orig

  faults: List[Tuple[str, str, str, str, Tuple[Type[ModelCheckFailure], ...], bool]] = [
      # --- 15 Historical V8 BackgroundMergeTask Bugs (cleanly isolated) ---
      (
          "Hist#01 81b2acf3bbf (Unsafe ScopeInfo read in RecordScopeInfos)",
          rx(
              c_orig,
              "      scope_info = Cast<SharedFunctionInfo>(info)->TryGetScopeInfoForMerge();",
              "      Tagged<SharedFunctionInfo> old_sfi = Cast<SharedFunctionInfo>(info);\n"
              "      if (!old_sfi->scope_info()->IsEmpty()) {\n"
              "        scope_info = old_sfi->scope_info();\n"
              "      } else if (old_sfi->HasOuterScopeInfo()) {\n"
              "        scope_info = old_sfi->GetOuterScopeInfo();\n"
              "      } else {\n"
              f"        {rec_ret}\n"
              "      }",
              label="81b2acf3bbf",
          ),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure, DuplicateScopeInfoError, UnsafePublicationError),
          True,
      ),
      (
          "Hist#02 7fd662da8a5 (Unsafe outer ScopeInfo TOCTOU in BeginMergeInBackground)",
          rx(
              c_orig,
              "            Tagged<ScopeInfo> outer_info = old_sfi->TryGetOuterScopeInfo();\n"
              "            if (!outer_info->IsEmpty()) {",
              "            if (old_sfi->HasOuterScopeInfo()) {\n"
              "              Tagged<ScopeInfo> outer_info = old_sfi->GetOuterScopeInfo();",
              label="7fd662da8a5",
          ),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure, DuplicateScopeInfoError, UnsafePublicationError),
          True,
      ),
      (
          "Hist#03 8afc1d18dcf (Concurrent compile outer ScopeInfo forwarding)",
          rx(
              rx(
                  c_orig,
                  "      forwarder.UpdateScopeInfo(*new_compiled_data.new_sfi);",
                  "      forwarder.InstallOwnScopeInfo(*new_compiled_data.new_sfi);",
                  label="8afc_1",
              ),
              "      forwarder.UpdateScopeInfo(sfi);",
              "      forwarder.InstallOwnScopeInfo(sfi);",
              label="8afc_2",
          ),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#04 40af3fedf4d (Standalone ScopeInfo outer forwarding)",
          rx(
              c_orig,
              "      forwarder.UpdateStandaloneScopeInfo(*new_scope);",
              "      DCHECK(true);",
              expected_count=2,
              label="40af3fedf4d",
          ),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#05 4e31386ca3e (Track uncompiled SFIs without ScopeInfo)",
          (rx(
              c_orig,
              "        new_script->infos()->set(i, Smi::zero());",
              "        new_script->infos()->set(i, maybe_old_info);",
              label="4e31_cl3",
          ) if is_cl3_smi else rx(
              c_orig,
              "        sfis_without_scope_info_.insert(i);",
              "        DCHECK(true);",
              label="4e31_cl2",
          )),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#06 7cacf0331ea (Foreground reverse release order)",
          rx(
              rx(
                  rx(
                      rx(
                          c_orig,
                          "  auto compiled_data_it = new_compiled_data_for_cached_sfis_.rbegin();",
                          "  auto compiled_data_it = new_compiled_data_for_cached_sfis_.begin();",
                          label="7cac_1",
                      ),
                      "    uint32_t i = old_script_infos_len - 1 - index;",
                      "    uint32_t i = index;",
                      label="7cac_2",
                  ),
                  "compiled_data_it != new_compiled_data_for_cached_sfis_.rend()",
                  "compiled_data_it != new_compiled_data_for_cached_sfis_.end()",
                  label="7cac_3",
              ),
              "compiled_data_it->cached_sfi->function_literal_id(kRelaxedLoad) >=\n"
              "              static_cast<int32_t>(i)",
              "compiled_data_it->cached_sfi->function_literal_id(kRelaxedLoad) <=\n"
              "              static_cast<int32_t>(i)",
              label="7cac_4",
          ),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure, ConcurrentReaderError),
          True,
      ),
      (
          "Hist#07 cdb22573497 (ObjectBoilerplateDescription SFI forwarding)",
          rx(
              c_orig,
              "      VisitObjectBoilerplateDescription(\n"
              "          Cast<ObjectBoilerplateDescription>(heap_obj));",
              "      DCHECK(true);",
              label="cdb22573497",
          ),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure,),
          True,
      ),
      (
          "Hist#08 ab5770f121a (Recreated SFI scope info reattachment - isolated)",
          rx(
              c_orig,
              "      forwarder.UpdateScopeInfo(*new_compiled_data.new_sfi);",
              "      DCHECK(true);",
              label="ab5770f121a",
          ),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#09 c8d1ca8a2c4 (HasBytecodeArray guard before GetBytecodeArray - single site)",
          rx(
              c_orig,
              "        used_new_sfis_.push_back(local_heap->NewPersistentHandle(new_sfi));\n"
              "        if (new_sfi->HasBytecodeArray()) {",
              "        used_new_sfis_.push_back(local_heap->NewPersistentHandle(new_sfi));\n"
              "        if (true) {",
              label="c8d1ca8a2c4",
          ),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure,),
          True,
      ),
      (
          "Hist#10 59121a5f058 (Dereferencing cleared weak ref in rescan)",
          (rx(
              c_orig,
              "      if (maybe_old_info.IsWeak() &&\n"
              "          !Cast<SharedFunctionInfo>(maybe_old_info.GetHeapObjectAssumeWeak())",
              "      if (true &&\n"
              "          !Cast<SharedFunctionInfo>(maybe_old_info.GetHeapObjectAssumeWeak())",
              label="5912_cl3",
          ) if is_cl3_smi else rx(
              c_orig,
              "      if (sfis_without_scope_info_.contains(i) && maybe_old_info.IsWeak()) {",
              "      if (sfis_without_scope_info_.contains(i)) {",
              label="5912_cl2",
          )),
          inl_orig,
          cc_orig,
          (CxxDcheckFailure,),
          True,
      ),
      (
          "Hist#11 Missing kReleaseStore on foreground SFI publication",
          rx(
              c_orig,
              "      old_script->infos()->set(i, maybe_new_info, kReleaseStore);",
              "      old_script->infos()->set(i, maybe_new_info);",
              label="fg_pub_release",
          ),
          inl_orig,
          cc_orig,
          (UnsafePublicationError,),
          True,
      ),
      (
          "Hist#12 CopyFrom1 (Revert outer_scope_info_or_feedback_metadata_.Release_Store to plain store)",
          c_orig,
          inl_orig,
          rx(
              cc_orig,
              "  outer_scope_info_or_feedback_metadata_.Release_Store(\n"
              "      this, other->raw_outer_scope_info_or_feedback_metadata(kAcquireLoad));",
              "  set_outer_scope_info_or_feedback_metadata(\n"
              "      other->outer_scope_info_or_feedback_metadata());",
              label="CopyFrom1_outer_plain",
          ),
          (DataRaceError, UnsafePublicationError, ConcurrentReaderError),
          True,
      ),
      (
          "Hist#13 VisitScopeInfo (own ScopeInfo constant-pool replacement)",
          rx(
              c_orig,
              "        constant_pool->set(i, *it->second);",
              "        DCHECK(true);",
              label="visit_si_own",
          ),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#14 VisitScopeInfo (outer ScopeInfo forwarding)",
          rx(
              c_orig,
              "        scope_info->set_outer_scope_info(*it->second);",
              "        DCHECK(true);",
              label="visit_si_outer",
          ),
          inl_orig,
          cc_orig,
          (DuplicateScopeInfoError,),
          True,
      ),
      (
          "Hist#15 CopyFrom2 (Premature code installation before ScopeInfo/feedback_slot)",
          c_orig,
          inl_orig,
          rx(
              rx(
                  cc_orig,
                  "  // Install code last to ensure that the entire SFI is properly initialized if\n"
                  "  // it's compiled.\n"
                  "  if (other->HasTrustedData()) {\n"
                  "    SetTrustedData(\n"
                  "        TrustedCast<ExposedTrustedObject>(other->GetTrustedData(isolate)));\n"
                  "  } else {\n"
                  "    SetUntrustedData(other->GetUntrustedData());\n"
                  "  }",
                  "  // moved earlier",
                  label="CopyFrom2_rm",
              ),
              "  set_name_or_scope_info(other->name_or_scope_info(kAcquireLoad),\n"
              "                         kReleaseStore);",
              "  if (other->HasTrustedData()) {\n"
              "    SetTrustedData(\n"
              "        TrustedCast<ExposedTrustedObject>(other->GetTrustedData(isolate)));\n"
              "  } else {\n"
              "    SetUntrustedData(other->GetUntrustedData());\n"
              "  }\n"
              "  set_name_or_scope_info(other->name_or_scope_info(kAcquireLoad),\n"
              "                         kReleaseStore);",
              label="CopyFrom2_ins",
          ),
          (ConcurrentReaderError, DataRaceError),
          True,
      ),
      # --- Audit-Derived Concurrency, Memory-Ordering, GC & Handle Mutants ---
      (
          "Audit S1a (Delete TryGetScopeInfoForMerge retry)",
          c_orig,
          rx(
              inl_orig,
              "  maybe_scope_info = name_or_scope_info(kAcquireLoad);\n"
              "  if (IsScopeInfo(maybe_scope_info)) {\n"
              "    return Cast<ScopeInfo>(maybe_scope_info);\n"
              "  }",
              "  // deleted retry",
              label="S1a",
          ),
          cc_orig,
          (ConcurrentReaderError,),
          True,
      ),
      (
          "Audit S1b (Delete TryGetOuterScopeInfo fallback)",
          c_orig,
          rx(
              inl_orig,
              "  if (Tagged<ScopeInfo> scope_info;\n"
              "      TryCast(name_or_scope_info(kAcquireLoad), &scope_info)) {\n"
              "    if (scope_info->HasOuterScopeInfo()) {\n"
              "      return scope_info->OuterScopeInfo();\n"
              "    }\n"
              "  }",
              "  // deleted fallback",
              label="S1b",
          ),
          cc_orig,
          (ConcurrentReaderError,),
          True,
      ),
      (
          "Audit S2a (Drop kAcquireLoad in TryGetScopeInfoForMerge)",
          c_orig,
          rx(
              inl_orig,
              "  Tagged<Object> maybe_outer_scope_info_or_feedback =\n"
              "      raw_outer_scope_info_or_feedback_metadata(kAcquireLoad);",
              "  Tagged<Object> maybe_outer_scope_info_or_feedback =\n"
              "      raw_outer_scope_info_or_feedback_metadata();",
              label="S2a",
          ),
          cc_orig,
          (UnsafePublicationError, DataRaceError, ConcurrentReaderError),
          True,
      ),
      (
          "Audit S2b (Drop kAcquireLoad in TryGetOuterScopeInfo)",
          c_orig,
          rx(
              inl_orig,
              "  if (Tagged<ScopeInfo> outer_scope_info;\n"
              "      TryCast(raw_outer_scope_info_or_feedback_metadata(kAcquireLoad),\n"
              "              &outer_scope_info)) {",
              "  if (Tagged<ScopeInfo> outer_scope_info;\n"
              "      TryCast(raw_outer_scope_info_or_feedback_metadata(),\n"
              "              &outer_scope_info)) {",
              label="S2b",
          ),
          cc_orig,
          (UnsafePublicationError, DataRaceError, ConcurrentReaderError),
          True,
      ),
      (
          "Audit S3 (Drop kAcquireLoad on old_script->infos()->get in BG loop)",
          rx(
              c_orig,
              "    Tagged<MaybeObject> maybe_old_info =\n"
              "        old_script->infos()->get(i, kAcquireLoad);",
              "    Tagged<MaybeObject> maybe_old_info =\n"
              "        old_script->infos()->get(i);",
              label="S3",
          ),
          inl_orig,
          cc_orig,
          (UnsafePublicationError,),
          True,
      ),
      (
          "Audit S3b (Drop kAcquireLoad in ConstantPoolPointerForwarder)",
          rx(
              c_orig,
              "    Tagged<MaybeObject> maybe_old_sfi = old_script_->infos()->get(\n"
              "        sfi->function_literal_id(kRelaxedLoad), kAcquireLoad);",
              "    Tagged<MaybeObject> maybe_old_sfi = old_script_->infos()->get(\n"
              "        sfi->function_literal_id(kRelaxedLoad));",
              label="S3b",
          ),
          inl_orig,
          cc_orig,
          (UnsafePublicationError,),
          True,
      ),
      (
          "Audit S5 (Local handle instead of NewPersistentHandle for compiled_data)",
          rx(
              c_orig,
              "          new_compiled_data_for_cached_sfis_.push_back(\n"
              "              {old_sfi_handle, local_heap->NewPersistentHandle(new_sfi)});",
              "          new_compiled_data_for_cached_sfis_.push_back(\n"
              "              {handle(old_sfi, local_heap), local_heap->NewPersistentHandle(new_sfi)});",
              label="S5",
          ),
          inl_orig,
          cc_orig,
          (DanglingHandleError,),
          True,
      ),
      (
          "Audit S6 (Local handle instead of NewPersistentHandle for used_new_sfis_)",
          rx(
              c_orig,
              "        used_new_sfis_.push_back(local_heap->NewPersistentHandle(new_sfi));",
              "        used_new_sfis_.push_back(handle(new_sfi, local_heap));",
              label="S6",
          ),
          inl_orig,
          cc_orig,
          (DanglingHandleError,),
          True,
      ),
      (
          "Audit S7 (Hoist raw constant_pool pointer across local_heap_->Safepoint())",
          rx(
              c_orig,
              "    for (DirectHandle<BytecodeArray> entry : bytecode_arrays_to_update_) {\n"
              "      local_heap_->Safepoint();\n"
              "      DisallowGarbageCollection no_gc;\n"
              "      IterateConstantPool(entry->constant_pool());\n"
              "    }",
              "    for (DirectHandle<BytecodeArray> entry : bytecode_arrays_to_update_) {\n"
              "      Tagged<TrustedFixedArray> raw_cp = entry->constant_pool();\n"
              "      local_heap_->Safepoint();\n"
              "      DisallowGarbageCollection no_gc;\n"
              "      IterateConstantPool(raw_cp);\n"
              "    }",
              label="S7",
          ),
          inl_orig,
          cc_orig,
          (RawPointerAcrossSafepointError,),
          True,
      ),
      (
          "Audit S9_b2b082c8bad (Drop old_sfi persistent handle before HasBytecodeArray)",
          rx(
              rx(
                  c_orig,
                  "        Handle<SharedFunctionInfo> old_sfi_handle =\n"
                  "            local_heap->NewPersistentHandle(old_sfi);\n"
                  "        if (old_sfi->HasBytecodeArray()) {",
                  "        if (old_sfi->HasBytecodeArray()) {",
                  label="S9_1",
              ),
              "              {old_sfi_handle, local_heap->NewPersistentHandle(new_sfi)});",
              "              {local_heap->NewPersistentHandle(old_sfi), local_heap->NewPersistentHandle(new_sfi)});",
              label="S9_2",
          ),
          inl_orig,
          cc_orig,
          (ConcurrentReaderError, CxxDcheckFailure, DuplicateScopeInfoError),
          True,
      ),
      (
          "Audit S10 (Foreground returns null Handle<SharedFunctionInfo>)",
          rx(
              c_orig,
              "  return handle_scope.CloseAndEscape(result);",
              "  return Handle<SharedFunctionInfo>();",
              label="S10",
          ),
          inl_orig,
          cc_orig,
          (DanglingHandleError, InvariantViolationError),
          True,
      ),
      (
          "Audit S11 (BG leaves state_ = kDone instead of kPendingForegroundWork)",
          rx(
              c_orig,
              "  state_ = kPendingForegroundWork;",
              "  state_ = kDone;",
              label="S11",
          ),
          inl_orig,
          cc_orig,
          (InvariantViolationError,),
          True,
      ),
      (
          "Audit S12 (FG omits state_ = kDone)",
          rx(
              c_orig,
              "  state_ = kDone;\n\n  if (isolate->NeedsSourcePositions()) {",
              "  if (isolate->NeedsSourcePositions()) {",
              label="S12",
          ),
          inl_orig,
          cc_orig,
          (InvariantViolationError,),
          True,
      ),
      (
          "Audit CopyFrom3 (Write outer_scope_info_or_feedback_metadata_ before name_or_scope_info)",
          c_orig,
          inl_orig,
          rx(
              cc_orig,
              "  set_name_or_scope_info(other->name_or_scope_info(kAcquireLoad),\n"
              "                         kReleaseStore);\n\n"
              "  outer_scope_info_or_feedback_metadata_.Release_Store(\n"
              "      this, other->raw_outer_scope_info_or_feedback_metadata(kAcquireLoad));",
              "  outer_scope_info_or_feedback_metadata_.Release_Store(\n"
              "      this, other->raw_outer_scope_info_or_feedback_metadata(kAcquireLoad));\n\n"
              "  set_name_or_scope_info(other->name_or_scope_info(kAcquireLoad),\n"
              "                         kReleaseStore);",
              label="CopyFrom3",
          ),
          (ConcurrentReaderError,),
          True,
      ),
      (
          "Audit CopyFrom4 (Omit set_length in CopyFrom -> caught by #if DEBUG memcmp)",
          c_orig,
          inl_orig,
          rx(
              cc_orig,
              "  set_length(other->length());",
              "  // omitted set_length",
              label="CopyFrom4",
          ),
          (CxxDcheckFailure,),
          True,
      ),
      (
          "Audit CopyFrom5 (Move set_feedback_slot after SetTrustedData -> pre-CL-2 race)",
          c_orig,
          inl_orig,
          rx(
              rx(
                  cc_orig,
                  "  set_feedback_slot(other->feedback_slot());\n",
                  "",
                  label="CopyFrom5_rm",
              ),
              "  } else {\n"
              "    SetUntrustedData(other->GetUntrustedData());\n"
              "  }\n",
              "  } else {\n"
              "    SetUntrustedData(other->GetUntrustedData());\n"
              "  }\n"
              "  set_feedback_slot(other->feedback_slot());\n",
              label="CopyFrom5_ins",
          ),
          (ConcurrentReaderError, DataRaceError),
          True,
      ),
      (
          "Audit N3 (Raw top-level SFI held across allocating Script::InitLineEnds)",
          rx(
              c_orig,
              "  Handle<SharedFunctionInfo> result = handle(\n"
              "      Cast<SharedFunctionInfo>(maybe_toplevel_sfi.GetHeapObjectAssumeWeak()),\n"
              "      isolate);\n\n"
              "  state_ = kDone;\n\n"
              "  if (isolate->NeedsSourcePositions()) {\n"
              "    Script::InitLineEnds(isolate, old_script);\n"
              "    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, result);\n"
              "  }",
              "  Tagged<SharedFunctionInfo> raw_top =\n"
              "      Cast<SharedFunctionInfo>(maybe_toplevel_sfi.GetHeapObjectAssumeWeak());\n\n"
              "  state_ = kDone;\n\n"
              "  if (isolate->NeedsSourcePositions()) {\n"
              "    Script::InitLineEnds(isolate, old_script);\n"
              "  }\n"
              "  Handle<SharedFunctionInfo> result = handle(raw_top, isolate);\n"
              "  if (isolate->NeedsSourcePositions()) {\n"
              "    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, result);\n"
              "  }",
              label="N3",
          ),
          inl_orig,
          cc_orig,
          (RawPointerAcrossSafepointError,),
          True,
      ),
      (
          "Audit N4 (Raw WeakFixedArray held across FG_RELEASE_BACKWARDS_START safepoint)",
          rx(
              c_orig,
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RELEASE_BACKWARDS_START);\n"
              "  for (uint32_t index = 0; index < old_script_infos_len; ++index) {\n"
              "    uint32_t i = old_script_infos_len - 1 - index;\n"
              "    Tagged<MaybeObject> maybe_old_info = old_script->infos()->get(i);",
              "  Tagged<WeakFixedArray> raw_old_infos = old_script->infos();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RELEASE_BACKWARDS_START);\n"
              "  for (uint32_t index = 0; index < old_script_infos_len; ++index) {\n"
              "    uint32_t i = old_script_infos_len - 1 - index;\n"
              "    Tagged<MaybeObject> maybe_old_info = raw_old_infos->get(i);",
              label="N4",
          ),
          inl_orig,
          cc_orig,
          (RawPointerAcrossSafepointError,),
          True,
      ),
      (
          "Audit N28 (Raw new_script->infos() hoisted across BG_INSPECT_SLOT safepoint)",
          rx(
              c_orig,
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INSPECT_SLOT);\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_new_sfi = new_script->infos()->get(i);",
              "  Tagged<WeakFixedArray> raw_new_infos = new_script->infos();\n"
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    MERGE_MODEL_STEP_WITH_SAFEPOINT(BG_INSPECT_SLOT);\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_new_sfi = raw_new_infos->get(i);",
              label="N28",
          ),
          inl_orig,
          cc_orig,
          (RawPointerAcrossSafepointError,),
          True,
      ),
      (
          "Audit N55 (Raw new_script->infos() hoisted across FG_RESCAN_START safepoint)",
          rx(
              c_orig,
              "  uint32_t old_script_infos_len = old_script->infos()->ulength().value();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);\n"
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_info = old_script->infos()->get(i);\n"
              "    Tagged<MaybeObject> maybe_new_info = new_script->infos()->get(i);",
              "  uint32_t old_script_infos_len = old_script->infos()->ulength().value();\n"
              "  Tagged<WeakFixedArray> raw_new_infos = new_script->infos();\n"
              "  MERGE_MODEL_STEP_WITH_SAFEPOINT(FG_RESCAN_START);\n"
              "  for (uint32_t i = 0; i < old_script_infos_len; ++i) {\n"
              "    DisallowGarbageCollection no_gc;\n"
              "    Tagged<MaybeObject> maybe_old_info = old_script->infos()->get(i);\n"
              "    Tagged<MaybeObject> maybe_new_info = raw_new_infos->get(i);",
              label="N55",
          ),
          inl_orig,
          cc_orig,
          (RawPointerAcrossSafepointError,),
          True,
      ),
      (
          "Audit M7v (Drop kAcquireLoad on TryGetScopeInfoForMerge retry)",
          c_orig,
          rx(
              inl_orig,
              "  maybe_scope_info = name_or_scope_info(kAcquireLoad);\n"
              "  if (IsScopeInfo(maybe_scope_info)) {\n"
              "    return Cast<ScopeInfo>(maybe_scope_info);\n"
              "  }",
              "  maybe_scope_info = name_or_scope_info_.load();\n"
              "  if (IsScopeInfo(maybe_scope_info)) {\n"
              "    return Cast<ScopeInfo>(maybe_scope_info);\n"
              "  }",
              label="M7v",
          ),
          cc_orig,
          (UnsafePublicationError,),
          True,
      ),
      (
          "Audit M9v (Drop kAcquireLoad on TryGetOuterScopeInfo fallback)",
          c_orig,
          rx(
              inl_orig,
              "  if (Tagged<ScopeInfo> scope_info;\n"
              "      TryCast(name_or_scope_info(kAcquireLoad), &scope_info)) {\n"
              "    if (scope_info->HasOuterScopeInfo()) {\n"
              "      return scope_info->OuterScopeInfo();\n"
              "    }\n"
              "  }",
              "  if (Tagged<ScopeInfo> scope_info;\n"
              "      TryCast(name_or_scope_info_.load(), &scope_info)) {\n"
              "    if (scope_info->HasOuterScopeInfo()) {\n"
              "      return scope_info->OuterScopeInfo();\n"
              "    }\n"
              "  }",
              label="M9v",
          ),
          cc_orig,
          (UnsafePublicationError,),
          True,
      ),
      (
          "Audit R3_1 (Delete DetachPersistentHandles at end of BG -> dangling handles on bg LocalHeap destruction)",
          rx(
              c_orig,
              "  persistent_handles_ = local_heap->DetachPersistentHandles();",
              "  // BUG: forgot DetachPersistentHandles(); bg LocalHeap destroys handles!",
              label="R3_1",
          ),
          inl_orig,
          cc_orig,
          (DanglingHandleError,),
          True,
      ),
      (
          "Audit R3_La (Delete AttachPersistentHandles at start of BG -> lazy block overwrites cached_script_)",
          rx(
              c_orig,
              "  local_heap->AttachPersistentHandles(std::move(persistent_handles_));",
              "  // BUG: forgot AttachPersistentHandles(); Detach overwrites & frees original PersistentHandles!",
              label="R3_La",
          ),
          inl_orig,
          cc_orig,
          (DanglingHandleError,),
          True,
      ),
  ]

  # CL 3-only S4 mutant: reintroducing double-sample HasScopeInfo() race after
  # BackgroundMerge_AfterRecordScopeInfos.
  if "if (!recorded_own_scope_info) {" in c_orig:
    faults.append((
        "Audit S4 (Double-sample HasScopeInfo() after RecordScopeInfos)",
        rx(
            c_orig,
            "      if (!recorded_own_scope_info) {",
            "      if (Is<SharedFunctionInfo>(maybe_old_info.GetHeapObjectAssumeWeak()) &&\n"
            "          !Cast<SharedFunctionInfo>(maybe_old_info.GetHeapObjectAssumeWeak())->HasScopeInfo()) {",
            label="S4",
        ),
        inl_orig,
        cc_orig,
        (DuplicateScopeInfoError,),
        True,
    ))

  return faults


def run_test_6_fault_injections(bundle: ParsedCxxBundle) -> None:
  print(
      "\n[TEST 6: Unmutated Control + Historical & Audit Mutant Verification]")
  t0 = time.time()

  # Step 6.0: Unmutated control WITH real markers checked!
  check_cxx_synchronization(bundle, check_macros=True)
  ctrl_stats = run_adversarial_suite(bundle, quick=True)
  print(
      f"  [Control] Unmutated target PASS "
      f"(traces={ctrl_stats.total_traces}, non-vacuous={ctrl_stats.non_vacuous_traces}, "
      f"probes={ctrl_stats.concurrent_reader_probes})")

  bmt_orig = read_repo_file(
      "src/codegen/background-merge-task.h", git_rev=bundle.git_rev)
  faults = _build_fault_catalog(bundle.raw_compiler_cc, bundle.raw_sfi_inl,
                                bundle.raw_sfi_cc)

  caught_count = 0
  for name, c_mut, inl_mut, cc_mut, expected_excs, keep_sync in faults:
    mut_bundle = parse_v8_merge_bundle(
        git_rev=bundle.git_rev,
        compiler_content=c_mut,
        sfi_inl_content=inl_mut,
        sfi_cc_content=cc_mut,
        bmt_h_content=bmt_orig,
    )
    if keep_sync:
      # Verify every mutant preserves real MERGE_MODEL_STEP markers!
      check_cxx_synchronization(mut_bundle, check_macros=True)
    try:
      run_adversarial_suite(mut_bundle, quick=True)
    except expected_excs as exc:
      caught_count += 1
      print(f"  CAUGHT   {name}")
      print(
          f"           -> {type(exc).__name__}: {str(exc).splitlines()[0][:110]}"
      )
    except ModelCheckFailure as exc:
      raise CxxHarnessError(
          f"Fault {name!r} raised unexpected ModelCheckFailure {type(exc).__name__} "
          f"(expected {[e.__name__ for e in expected_excs]}): {exc}") from exc
    except Exception as exc:
      raise CxxHarnessError(
          f"Fault {name!r} crashed the harness/interpreter with {type(exc).__name__} "
          f"instead of a real ModelCheckFailure: {exc}") from exc
    else:
      raise InvariantViolationError(
          f"SURVIVED! Mutant {name!r} passed all adversarial traces!")

  elapsed = time.time() - t0
  print(
      f"  Fault Injection & Audit Mutant Verification: PASSED "
      f"({caught_count}/{len(faults)} caught with exact expected failure classes in {elapsed:.1f}s)"
  )


# ---------------------------------------------------------------------------
# Entry Point & CLI
# ---------------------------------------------------------------------------


def run_all_tests(
    *,
    cxx_path: Optional[str] = None,
    git_rev: Optional[str] = None,
    verify_sync_markers: Optional[bool] = None,
    verify_historical_bugs: bool = False,
    quick: bool = False,
) -> None:
  target = (f"git revision '{git_rev}'" if git_rev else
            (cxx_path or "working tree src/codegen/compiler.cc"))
  print("=" * 74)
  print("V8 BackgroundMergeTask Concurrency Model Checker (Clang C++20 AST)")
  print(f"Target: {target}")
  print("=" * 74)

  bundle = parse_v8_merge_bundle(cxx_path=cxx_path, git_rev=git_rev)
  if verify_sync_markers is None:
    verify_sync_markers = True

  if verify_sync_markers:
    run_test_0_sync_check(bundle)
    run_test_1_negative_sync_self_tests(bundle)
  else:
    print(
        "[TEST 0 & TEST 1: Static Marker Verification SKIPPED (--no-verify-sync-markers)]"
    )

  run_test_2_clean_merges(bundle)
  run_test_3_adversarial(bundle, quick=quick)
  run_test_4_compile_masks(bundle)
  run_test_5_micro_tests(bundle)

  if verify_historical_bugs:
    run_test_6_fault_injections(bundle)

  print("\n" + "=" * 74)
  print(f"ALL VERIFICATION SUITES PASSED FOR {target.upper()}")
  print("=" * 74)


def main(argv: Optional[Sequence[str]] = None) -> int:
  parser = argparse.ArgumentParser(
      description=(
          "Scenario-based Clang C++20 AST concurrency model checker for V8 "
          "BackgroundMergeTask."))
  parser.add_argument(
      "--git-rev",
      type=str,
      default=None,
      help="Git revision to extract all 4 V8 C++ source/header files from.",
  )
  parser.add_argument(
      "--cxx-path",
      type=str,
      default=None,
      help="Path to src/codegen/compiler.cc (defaults to working tree).",
  )
  parser.add_argument(
      "--check-sync",
      action="store_true",
      help="Verify MERGE_MODEL_STEP markers, hazards, and macros, then exit.",
  )
  parser.add_argument(
      "--dump-ast",
      action="store_true",
      help="Dump a summary of parsed Clang AST methods and exit.",
  )
  parser.add_argument(
      "--no-verify-sync-markers",
      action="store_true",
      help="Skip TEST 0 and TEST 1 marker verification.",
  )
  parser.add_argument(
      "--verify-sync-markers",
      action="store_true",
      help="Explicitly require TEST 0 and TEST 1 marker verification (default).",
  )
  parser.add_argument(
      "--verify-historical-bugs",
      action="store_true",
      help="Run TEST 6 (unmutated control + 15 historical bugs + audit mutants).",
  )
  parser.add_argument(
      "--quick",
      action="store_true",
      help="Use the reduced schedule subset in TEST 3 for faster local iteration.",
  )
  args = parser.parse_args(argv)

  verify_sync = True
  if args.no_verify_sync_markers:
    verify_sync = False
  elif args.verify_sync_markers:
    verify_sync = True

  if args.check_sync:
    bundle = parse_v8_merge_bundle(cxx_path=args.cxx_path, git_rev=args.git_rev)
    try:
      info = check_cxx_synchronization(bundle, check_macros=True)
    except ConcurrencySyncMismatchError as exc:
      print(f"Synchronization errors detected:\n{exc}", file=sys.stderr)
      return 1
    print(f"OK: {info['total_markers']} ordered markers, hazard precedence, "
          f"no_gc safepoints, and *_TEST_ONLY macros verified.")
    return 0

  if args.dump_ast:
    bundle = parse_v8_merge_bundle(cxx_path=args.cxx_path, git_rev=args.git_rev)
    print(f"BeginMergeInBackground statements: {len(bundle.bg_stmts)}")
    print(f"CompleteMergeInForeground statements: {len(bundle.fg_stmts)}")
    print(f"VerifyCodeMerge statements: {len(bundle.vcm_stmts)}")
    print(f"VerifyCodeMerge helpers: {sorted(bundle.vcm_helpers.keys())}")
    print(
        f"ConstantPoolPointerForwarder methods ({len(bundle.fwd_methods)}): {sorted(bundle.fwd_methods.keys())}"
    )
    print(
        f"SharedFunctionInfo methods ({len(bundle.sfi_methods)}): {sorted(bundle.sfi_methods.keys())}"
    )
    return 0

  run_all_tests(
      cxx_path=args.cxx_path,
      git_rev=args.git_rev,
      verify_sync_markers=verify_sync,
      verify_historical_bugs=args.verify_historical_bugs,
      quick=args.quick,
  )
  return 0


if __name__ == "__main__":
  sys.exit(main())
