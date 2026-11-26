#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for the Clang AST frontend, C++ interpreter, memory model, and GC.

Verifies every C++ semantics corner case (including all H5 audit snippets),
fail-closed AST conversion, ReadOnlyRoots identity, pure C++11 happens-before
memory ordering (non-SC acquire, unsafe publication, data races, per-load MP),
HandleScope/PersistentHandles lifetimes, moving-GC raw pointer detection,
DisallowGarbageCollection enforcement, and ``python3 -O`` immunity.
"""

from __future__ import annotations

import os
import sys
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)

from clang_frontend import (
    _extract_balanced_template_args,
    parse_cxx_snippet,
)
from cxx_interpreter import CxxAstInterpreter
from errors import (
    CxxDcheckFailure,
    DanglingHandleError,
    DataRaceError,
    GcInNoGcScopeError,
    InvariantViolationError,
    RawPointerAcrossSafepointError,
    UnsafePublicationError,
    UnsupportedCxxAstError,
    check_condition,
)
from v8_heap_model import (
    BytecodeArray,
    CxxVector,
    DisallowGarbageCollection,
    FeedbackMetadata,
    FixedArray,
    HandleScope,
    Isolate,
    LocalHandleScope,
    LocalIsolate,
    MakeWeak,
    MaybeObject,
    MemoryContext,
    MemoryLocation,
    MemoryOrder,
    NullTagged,
    READ_ONLY_ROOTS,
    RawTaggedRef,
    ScopeInfo,
    Script,
    SharedFunctionInfo,
    Smi,
    V8Heap,
    cast_cxx_type,
    cxx_handle,
    is_cxx_type,
    kAcquireLoad,
    kRelaxedLoad,
    kReleaseStore,
    unwrap_raw,
)


def _run_snippet(code: str, fn_name: str, *args, ilp_order: str = "ltr"):
  methods = parse_cxx_snippet(code, use_v8_pch=False)
  if fn_name not in methods:
    raise AssertionError(f"Function {fn_name!r} not found in {list(methods)}")
  interp = CxxAstInterpreter(ilp_eval_order=ilp_order)
  return interp.execute_method(methods[fn_name], args=list(args))


class TestCxxInterpreterSemantics(unittest.TestCase):
  """Tests real Clang-parsed C++20 snippets (covers every H5 audit bug)."""

  def test_all_compound_assignments_and_bitwise_ops(self):
    code = """
    int test_ops() {
      int a = 6; a |= 1;      // 7
      int b = 6; b *= 2;      // 12
      int c = 1; c <<= 3;     // 8
      int d = 15; d &= 6;     // 6
      int e = 7; e ^= 2;      // 5
      int f = 16; f >>= 2;    // 4
      int g = 21; g /= 4;     // 5
      int h = 21; h %= 4;     // 1
      int k = 10; k += 3; k -= 2; // 11
      return a * 100000000 + b * 1000000 + c * 100000 + d * 10000
           + e * 1000 + f * 100 + g * 10 + h + (k == 11 ? 0 : -999999);
    }
    """
    res = _run_snippet(code, "test_ops")
    self.assertEqual(res, 712865451)

  def test_compound_assignment_on_subscript_and_member_and_deref(self):
    code = """
    #include <vector>
    struct Box { int val; };
    int test_targets() {
      std::vector<int> v;
      v.push_back(1);
      v.push_back(10);
      v[0] += 5;      // 6
      v[1] *= 3;      // 30
      v[0]++;         // 7
      Box b;
      b.val = 4;
      b.val |= 2;     // 6
      int x = 20;
      int* p = &x;
      *p += 7;        // 27
      return v[0] * 10000 + v[1] * 100 + b.val + x * 1000000;
    }
    """
    res = _run_snippet(code, "test_targets")
    self.assertEqual(res, 27073006)

  def test_cpp_truncating_division_and_modulo(self):
    code = """
    int test_div() {
      int q1 = -7 / 2;   // -3 (not Python -4)
      int r1 = -7 % 2;   // -1 (not Python +1)
      int q2 = 7 / -2;   // -3
      int r2 = 7 % -2;   // +1
      return q1 * 1000 + r1 * 100 + q2 * 10 + r2;
    }
    """
    self.assertEqual(_run_snippet(code, "test_div"), -3129)

  def test_fixed_width_integer_wrapping(self):
    code = """
    #include <cstdint>
    uint64_t test_wrap() {
      uint32_t j = 0u - 1u;      // 4294967295
      uint16_t u16 = 0;
      u16 -= 1;                  // 65535
      uint8_t u8 = 250;
      u8 += 10;                  // 4
      return static_cast<uint64_t>(j) + u16 + u8;
    }
    """
    self.assertEqual(_run_snippet(code, "test_wrap"), 4294967295 + 65535 + 4)

  def test_lexical_block_scoping_and_shadowing(self):
    code = """
    int test_scope() {
      int x = 1;
      {
        int x = 2;
        x = x + 1;
        {
          int x = 100;
          x += 5;
        }
        if (x != 3) return -1;
      }
      return x;
    }
    """
    self.assertEqual(_run_snippet(code, "test_scope"), 1)

  def test_lambda_by_val_mutable_by_ref_and_shadowing(self):
    code = """
    int test_lambdas() {
      int x = 1;
      int y = 2;
      int t = 1;
      auto by_val = [x]() mutable {
        x = 5;
        int t = 9;
        return x + t;
      };
      int r1 = by_val();   // 14, outer x stays 1, outer t stays 1
      auto by_ref = [&y]() {
        y = 7;
        int t = 42;
        return y + t;
      };
      int r2 = by_ref();   // 49, outer y becomes 7, outer t stays 1
      return x * 10000 + y * 1000 + t * 100 + r1 + r2;
    }
    """
    self.assertEqual(
        _run_snippet(code, "test_lambdas"), 10000 + 7000 + 100 + 14 + 49)

  def test_vector_value_copy_and_range_for_by_ref_vs_val(self):
    code = """
    #include <vector>
    int test_vec() {
      std::vector<int> a;
      a.push_back(1);
      std::vector<int> b = a;
      b.push_back(2);
      int a_sz = static_cast<int>(a.size());  // must remain 1 (deep copy!)

      for (int e : a) {
        e = 99;  // by value: must NOT modify a[0]
      }
      int after_val = a[0];  // 1

      for (int& e : a) {
        e = 0;   // by ref: MUST modify a[0]
      }
      int after_ref = a[0];  // 0

      return a_sz * 100 + after_val * 10 + after_ref + static_cast<int>(b.size());
    }
    """
    self.assertEqual(_run_snippet(code, "test_vec"), 100 + 10 + 0 + 2)

  def test_for_expr_init_if_init_cond_var_switch_dowhile_comma(self):
    code = """
    int test_ctrl() {
      int i = 0;
      int s = 0;
      for (i = 3; i < 5; ++i) {
        s += i;  // 3 + 4 = 7
      }
      if (int v = 3) {
        s += v;  // 10
      }
      if (int k = 4; k == 4) {
        s += k;  // 14
      }
      int sw = 0;
      int tag = 1;
      switch (tag) {
        case 1:
          sw += 10;
        case 2:
          sw += 20;
          break;
        default:
          sw += 100;
      }
      int dw = 0;
      do {
        dw += 1;
      } while (dw < 3);
      do {} while (false);  // USE(x)-style idiom
      int cm = (i = 2, i + 5);  // 7
      return s * 1000 + sw * 10 + dw + cm;
    }
    """
    self.assertEqual(
        _run_snippet(code, "test_ctrl"), 14 * 1000 + 30 * 10 + 3 + 7)

  def test_raii_lifo_destruction_on_exit_return_break(self):
    """Verifies lexical scope frames invoke __cxx_exit__ in strict LIFO order."""
    log = []

    class Guard:

      def __init__(self, tag):
        self.tag = tag
        log.append(f"+{tag}")

      def __cxx_exit__(self):
        log.append(f"-{self.tag}")

    interp = CxxAstInterpreter()
    interp.registry["make_guard"] = lambda tag: Guard(tag)
    # Build AST directly for a block with two nested RAII guards and early return.
    stmts = [
        {
            "type": "Decl",
            "name": "g1",
            "qual_type": "Guard",
            "value": {
                "type": "Call",
                "callee": {
                    "type": "Name",
                    "name": "make_guard"
                },
                "args": [{
                    "type": "Literal",
                    "value": "outer"
                }]
            }
        },
        {
            "type":
                "Block",
            "body": [
                {
                    "type": "Decl",
                    "name": "g2",
                    "qual_type": "Guard",
                    "value": {
                        "type": "Call",
                        "callee": {
                            "type": "Name",
                            "name": "make_guard"
                        },
                        "args": [{
                            "type": "Literal",
                            "value": "inner1"
                        }]
                    }
                },
                {
                    "type": "Decl",
                    "name": "g3",
                    "qual_type": "Guard",
                    "value": {
                        "type": "Call",
                        "callee": {
                            "type": "Name",
                            "name": "make_guard"
                        },
                        "args": [{
                            "type": "Literal",
                            "value": "inner2"
                        }]
                    }
                },
                {
                    "type": "Return",
                    "value": {
                        "type": "Literal",
                        "value": 42
                    }
                },
            ]
        },
    ]
    ret = interp.execute_statements(stmts)
    self.assertEqual(ret, 42)
    self.assertEqual(
        log, ["+outer", "+inner1", "+inner2", "-inner2", "-inner1", "-outer"])

  def test_fail_closed_on_unsupported_ast_node(self):
    code = """
    int test_goto() {
      goto end;
    end:
      return 0;
    }
    """
    with self.assertRaises(UnsupportedCxxAstError):
      parse_cxx_snippet(code, use_v8_pch=False)

  def test_balanced_nested_template_extraction(self):
    src = "Cast<Outer<Inner<ScopeInfo>>>(x)"
    lt = src.index("<")
    self.assertEqual(
        _extract_balanced_template_args(src, lt),
        "Outer<Inner<ScopeInfo>>",
    )

  def test_implicit_integral_conversions_and_size_type(self):
    code = """
    #include <vector>
    int test_conv() {
      int a = -1;
      unsigned int b = 1;
      int r1 = (a < b) ? 1 : 0;  // 0: usual arithmetic conversion to unsigned
      std::vector<int> v;
      int r2 = (v.size() - 1 > 100) ? 1 : 0;  // 1: size_type is unsigned long
      return r1 * 10 + r2;
    }
    """
    self.assertEqual(_run_snippet(code, "test_conv"), 1)

  def test_const_ref_binding_to_temporary(self):
    code = """
    int test_cref() {
      const int& r = 1 + 2;
      int x = 4;
      const int& r2 = x + 5;
      return r * 10 + r2;
    }
    """
    self.assertEqual(_run_snippet(code, "test_cref"), 39)

  def test_vector_iterator_arrow_mutation(self):
    code = """
    #include <vector>
    struct Box { int val; };
    int test_iter_arrow() {
      std::vector<Box> v;
      Box b;
      b.val = 1;
      v.push_back(b);
      auto it = v.begin();
      it->val = 5;
      it->val += 2;
      return v[0].val;
    }
    """
    self.assertEqual(_run_snippet(code, "test_iter_arrow"), 7)

  def test_nontrivial_by_value_lambda_capture(self):
    code = """
    #include <vector>
    int test_vec_cap() {
      std::vector<int> v;
      v.push_back(1);
      auto f = [v]() mutable {
        v.push_back(2);
        return static_cast<int>(v.size());
      };
      int r1 = f();  // 2
      int r2 = f();  // 3
      return r1 * 100 + r2 * 10 + static_cast<int>(v.size());
    }
    """
    self.assertEqual(_run_snippet(code, "test_vec_cap"), 231)

  def test_unordered_map_range_for_and_fail_closed_member_error(self):
    code = """
    #include <unordered_map>
    int test_map_iter() {
      std::unordered_map<int, int> m;
      m.emplace(1, 10);
      m.emplace(2, 20);
      int sum = 0;
      for (auto& kv : m) {
        sum += kv.second;
      }
      return sum;
    }
    """
    self.assertEqual(_run_snippet(code, "test_map_iter"), 30)

    agg_code = """
    struct Pair { int a; int b; };
    int test_agg() {
      Pair p{1, 2};
      return p.a;
    }
    """
    with self.assertRaises(UnsupportedCxxAstError):
      _run_snippet(agg_code, "test_agg")


def _make_script(heap: V8Heap, n_slots: int = 2, label: str = "s") -> Script:
  infos = FixedArray(
      heap,
      [MaybeObject.Cleared() for _ in range(n_slots)],
      is_weak_array=True,
      label=f"{label}_infos",
  )
  return Script(heap, infos, label=label)


class TestHeapAndTypeSystem(unittest.TestCase):
  """Tests ReadOnlyRoots singletons, exact type checks, handles, and moving GC."""

  def test_empty_scope_info_singleton_and_null_distinction(self):
    e1 = READ_ONLY_ROOTS.empty_scope_info()
    e2 = READ_ONLY_ROOTS.empty_scope_info()
    self.assertIs(e1, e2)
    self.assertEqual(e1, e2)
    self.assertTrue(e1.IsEmpty())
    self.assertTrue(is_cxx_type(e1, "ScopeInfo"))
    self.assertFalse(is_cxx_type(e1, "TheHole"))
    self.assertTrue(is_cxx_type(READ_ONLY_ROOTS.the_hole_value(), "TheHole"))
    self.assertFalse(is_cxx_type(READ_ONLY_ROOTS.the_hole_value(), "ScopeInfo"))
    null_si = NullTagged("ScopeInfo")
    self.assertNotEqual(null_si, e1)
    self.assertNotEqual(null_si, READ_ONLY_ROOTS.the_hole_value())

  def test_exact_type_checking_no_substring_bugs(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    scr = _make_script(heap)
    sfi = SharedFunctionInfo(heap, 0, scr)
    self.assertTrue(is_cxx_type(sfi, "SharedFunctionInfo"))
    self.assertTrue(is_cxx_type(sfi, "HeapObject"))
    self.assertFalse(is_cxx_type(sfi, "Script"))
    self.assertFalse(is_cxx_type(sfi, "ScopeInfo"))
    self.assertFalse(is_cxx_type(Smi.zero(), "HeapObject"))
    self.assertTrue(is_cxx_type(Smi.zero(), "Smi"))
    with self.assertRaises(CxxDcheckFailure):
      cast_cxx_type(sfi, "Script")

  def test_handle_scope_dangling_and_close_and_escape(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    isolate = Isolate(heap)
    local_iso = LocalIsolate(heap)
    scr = _make_script(heap)
    sfi = SharedFunctionInfo(heap, 0, scr)

    # LocalHandleScope dangling handle
    hs = LocalHandleScope(local_iso.heap())
    h_dangling = cxx_handle(sfi, local_iso)
    self.assertIs(unwrap_raw(h_dangling.deref()), sfi)
    hs.__cxx_exit__()
    with self.assertRaises(DanglingHandleError):
      h_dangling.deref()

    # HandleScope::CloseAndEscape survives inner exit into outer scope
    outer = HandleScope(isolate)
    inner = HandleScope(isolate)
    h_inner = cxx_handle(sfi, isolate)
    escaped = inner.CloseAndEscape(h_inner)
    inner.__cxx_exit__()
    self.assertIs(unwrap_raw(escaped.deref()), sfi)
    outer.__cxx_exit__()
    with self.assertRaises(DanglingHandleError):
      escaped.deref()

  def test_moving_gc_invalidates_raw_tagged_but_not_handle(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    isolate = Isolate(heap)
    scr = _make_script(heap)
    sfi = SharedFunctionInfo(heap, 0, scr)
    hs = HandleScope(isolate)
    h = cxx_handle(sfi, isolate)
    raw = h.deref()
    self.assertIsInstance(raw, RawTaggedRef)
    heap.run_gc(advance_epoch=True)
    with self.assertRaises(RawPointerAcrossSafepointError):
      raw.deref()
    # Re-dereferencing the rooted Handle gets a fresh RawTaggedRef at new epoch!
    fresh = h.deref()
    self.assertIs(unwrap_raw(fresh), sfi)
    hs.__cxx_exit__()

  def test_gc_inside_disallow_gc_raises(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    local_iso = LocalIsolate(heap)
    no_gc = DisallowGarbageCollection(heap)
    with self.assertRaises(GcInNoGcScopeError):
      local_iso.heap().Safepoint()
    with self.assertRaises(GcInNoGcScopeError):
      heap.run_gc()
    no_gc.__cxx_exit__()
    # Now Safepoint succeeds cleanly.
    local_iso.heap().Safepoint()

  def test_weak_slot_clearing_obeys_real_handle_roots_only(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    scr = _make_script(heap, n_slots=2, label="s1")
    scr2 = _make_script(heap, n_slots=1, label="s2")
    heap.add_root(scr)
    heap.add_root(scr2)
    sfi_unrooted = SharedFunctionInfo(heap, 0, scr, label="unrooted")
    sfi_rooted = SharedFunctionInfo(heap, 1, scr, label="rooted")
    sfi_other_unrooted = SharedFunctionInfo(
        heap, 0, scr2, label="other_unrooted")
    scr.infos_raw().set(0, MakeWeak(sfi_unrooted), kReleaseStore)
    scr.infos_raw().set(1, MakeWeak(sfi_rooted), kReleaseStore)
    scr2.infos_raw().set(0, MakeWeak(sfi_other_unrooted), kReleaseStore)

    from v8_heap_model import PersistentHandles
    ph = PersistentHandles(heap)
    heap.bg_local_heap.AttachPersistentHandles(ph)
    _p_handle = heap.bg_local_heap.NewPersistentHandle(sfi_rooted)

    cleared = heap.run_gc()
    self.assertEqual(len(cleared), 2)
    self.assertTrue(scr.infos_raw().get(0, kAcquireLoad).IsCleared())
    self.assertTrue(scr.infos_raw().get(1, kAcquireLoad).IsWeak())
    self.assertTrue(scr2.infos_raw().get(0, kAcquireLoad).IsCleared())
    self.assertTrue(sfi_unrooted.freed)
    self.assertTrue(sfi_other_unrooted.freed)
    self.assertFalse(sfi_rooted.freed)
    with self.assertRaises(DanglingHandleError):
      MakeWeak(sfi_unrooted)
    with self.assertRaises(DanglingHandleError):
      sfi_unrooted._check_published()
    heap.bg_local_heap.DetachPersistentHandles()


class TestCpp11MemoryModel(unittest.TestCase):
  """Tests pure C++11 happens-before, non-SC acquire, unsafe publication, and data races."""

  def test_acquire_can_observe_stale_value_in_oldest_mode(self):
    mem = MemoryContext(mode="oldest")
    loc = MemoryLocation(mem, "old", "x")
    mem.switch_thread(mem.main_thread)
    loc.write("new", MemoryOrder.RELEASE)
    mem.switch_thread(mem.bg_thread)
    # Without prior happens-before, C++11 allows acquire load to return "old"!
    self.assertEqual(loc.read(MemoryOrder.ACQUIRE), "old")

  def test_unsafe_publication_detected_without_acquire(self):
    mem = MemoryContext(mode="per_load_mp")
    heap = V8Heap(mem)
    scr = _make_script(heap)
    mem.begin_background_handoff()
    # Main thread allocates a new SFI and publishes with kReleaseStore.
    mem.switch_thread(mem.main_thread)
    new_sfi = SharedFunctionInfo(
        heap,
        0,
        scr,
        label="pub_sfi",
        newly_allocated=True,
        writer=mem.main_thread)
    scr.infos_raw().set(0, MakeWeak(new_sfi), kReleaseStore)

    # Background thread reads with RELAXED (bug!) -> dereferencing fails!
    mem.switch_thread(mem.bg_thread)
    maybe_relaxed = scr.infos_raw().get(0, kRelaxedLoad)
    with self.assertRaises(UnsafePublicationError):
      maybe_relaxed.GetHeapObjectAssumeWeak()

    # Background thread reads with ACQUIRE (correct!) -> dereferencing succeeds!
    bg2 = mem.bg_thread.snapshot_for_concurrent_reader("bg_acq")
    mem.switch_thread(bg2)
    mem.mode = "latest"
    maybe_acq = scr.infos_raw().get(0, kAcquireLoad)
    self.assertIs(unwrap_raw(maybe_acq.GetHeapObjectAssumeWeak()), new_sfi)

  def test_plain_data_race_detected_and_release_acquire_fixes_it(self):
    mem = MemoryContext(mode="latest")
    data = MemoryLocation(mem, 0, "plain_data")
    flag = MemoryLocation(mem, 0, "atomic_flag")
    mem.begin_background_handoff()

    mem.switch_thread(mem.main_thread)
    data.write(42, MemoryOrder.PLAIN)
    flag.write(1, MemoryOrder.RELEASE)

    # Unsynchronized plain read raises DataRaceError.
    mem.switch_thread(mem.bg_thread)
    with self.assertRaises(DataRaceError):
      data.read(MemoryOrder.PLAIN)

    # After acquiring flag == 1, plain read of data is synchronized and safe!
    self.assertEqual(flag.read(MemoryOrder.ACQUIRE), 1)
    self.assertEqual(data.read(MemoryOrder.PLAIN), 42)

  def test_per_load_mp_catches_missing_release_without_field_hacks(self):
    mem = MemoryContext(mode="per_load_mp")
    payload = MemoryLocation(mem, "old_payload", "a")
    flag = MemoryLocation(mem, "old_flag", "b")
    mem.begin_background_handoff()

    # Buggy writer: writes payload (release), writes flag (RELAXED, missing release!).
    mem.switch_thread(mem.main_thread)
    payload.write("new_payload", MemoryOrder.RELEASE)
    flag.write("new_flag", MemoryOrder.RELAXED)

    # Reader observes flag == "new_flag" (without sync), then reads stale payload!
    mem.switch_thread(mem.bg_thread)
    self.assertEqual(flag.read(MemoryOrder.ACQUIRE), "new_flag")
    self.assertEqual(payload.read(MemoryOrder.ACQUIRE), "old_payload")

  def test_war_data_race_detected_and_handoff_synchronizes(self):
    # Case 1: BG plain read followed by concurrent main plain write -> WAR DataRaceError.
    mem1 = MemoryContext(mode="latest")
    loc1 = MemoryLocation(mem1, 0, "war_plain")
    mem1.begin_background_handoff()
    mem1.switch_thread(mem1.bg_thread)
    self.assertEqual(loc1.read(MemoryOrder.PLAIN), 0)
    mem1.switch_thread(mem1.main_thread)
    with self.assertRaises(DataRaceError):
      loc1.write(1, MemoryOrder.PLAIN)

    # Case 2: BG plain read followed by concurrent main atomic write -> WAR DataRaceError.
    mem2 = MemoryContext(mode="latest")
    loc2 = MemoryLocation(mem2, 0, "war_atomic")
    mem2.begin_background_handoff()
    mem2.switch_thread(mem2.bg_thread)
    self.assertEqual(loc2.read(MemoryOrder.PLAIN), 0)
    mem2.switch_thread(mem2.main_thread)
    with self.assertRaises(DataRaceError):
      loc2.write(1, MemoryOrder.RELEASE)

    # Case 3: BG plain read synchronized via end_background_handoff -> main plain write safe!
    mem3 = MemoryContext(mode="latest")
    loc3 = MemoryLocation(mem3, 0, "war_synced")
    mem3.begin_background_handoff()
    mem3.switch_thread(mem3.bg_thread)
    self.assertEqual(loc3.read(MemoryOrder.PLAIN), 0)
    mem3.end_background_handoff()
    mem3.switch_thread(mem3.main_thread)
    loc3.write(1, MemoryOrder.PLAIN)
    self.assertEqual(loc3.read(MemoryOrder.PLAIN), 1)

  def test_sfi_script_relaxed_load_does_not_synchronize_in_latest_mode(self):
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    s0 = _make_script(heap, label="s0")
    s1 = _make_script(heap, label="s1")
    sfi = SharedFunctionInfo(heap, 0, s0, label="scr_sfi")
    mem.begin_background_handoff()

    mem.switch_thread(mem.main_thread)
    sfi.set_script(s1, kReleaseStore)
    write_ts = sfi._script_loc.history[-1].ts

    mem.switch_thread(mem.bg_thread)
    got_relaxed = unwrap_raw(sfi.script(kRelaxedLoad))
    self.assertIs(got_relaxed, s1)
    self.assertNotIn(write_ts, mem.bg_thread.view)

    got_acq = unwrap_raw(sfi.script(kAcquireLoad))
    self.assertIs(got_acq, s1)
    self.assertIn(write_ts, mem.bg_thread.view)

  def test_per_load_mp_unlatches_after_subsequent_acquire_sync(self):
    mem = MemoryContext(mode="per_load_mp")
    owner1 = object()
    owner2 = object()
    a1 = MemoryLocation(mem, "old_a1", "a1")
    b1 = MemoryLocation(mem, "old_b1", "b1")
    a2 = MemoryLocation(mem, "old_a2", "a2")
    b2 = MemoryLocation(mem, "old_b2", "b2")
    mem.begin_background_handoff()

    mem.switch_thread(mem.main_thread)
    a1.write("new_a1", MemoryOrder.RELEASE, owner=owner1)
    b1.write("new_b1", MemoryOrder.RELAXED, owner=owner1)
    a2.write("new_a2", MemoryOrder.RELEASE, owner=owner2)
    b2.write("new_b2", MemoryOrder.RELEASE, owner=owner2)

    mem.switch_thread(mem.bg_thread)
    # Unsynchronized load on owner1 latches MP anomaly for owner1.
    self.assertEqual(b1.read(MemoryOrder.ACQUIRE, owner=owner1), "new_b1")
    self.assertEqual(a1.read(MemoryOrder.ACQUIRE, owner=owner1), "old_a1")
    # Subsequent acquiring load on owner2 synchronizes and unlatches MP!
    self.assertEqual(b2.read(MemoryOrder.ACQUIRE, owner=owner2), "new_b2")
    self.assertEqual(a2.read(MemoryOrder.ACQUIRE, owner=owner2), "new_a2")

  def test_sfi_default_memory_orders_match_cpp(self):
    mem = MemoryContext(mode="per_load_mp")
    heap = V8Heap(mem)
    scr = _make_script(heap)
    sfi = SharedFunctionInfo(heap, 0, scr)
    mem.begin_background_handoff()

    # Main thread installs ScopeInfo (Release) then FeedbackMetadata (Release).
    mem.switch_thread(mem.main_thread)
    si = ScopeInfo(heap, 100)
    fb = FeedbackMetadata(heap, "fb1")
    sfi.SetScopeInfo(si)
    sfi.outer_scope_info_or_feedback_metadata_.Release_Store(sfi, fb)

    # Untagged raw_outer_scope_info_or_feedback_metadata() is RELAXED (NOT acquire!)
    # matching src/objects/shared-function-info-inl.h:243-251. In per_load_mp,
    # observing fb via untagged load does NOT establish happens-before to SetScopeInfo,
    # so subsequent load of name_or_scope_info can still observe stale sentinel!
    mem.switch_thread(mem.bg_thread)
    self.assertIs(
        unwrap_raw(sfi.raw_outer_scope_info_or_feedback_metadata()), fb)
    self.assertFalse(
        is_cxx_type(sfi.name_or_scope_info(kAcquireLoad), "ScopeInfo"))

    # With explicit kAcquireLoad, it synchronizes with Release_Store so name_or_scope_info
    # MUST observe the new ScopeInfo!
    bg2 = mem.bg_thread.snapshot_for_concurrent_reader("bg_acq2")
    bg2.coherence.clear()
    bg2.view = {0}
    bg2.reset_mp_window()
    mem.switch_thread(bg2)
    self.assertIs(
        unwrap_raw(sfi.raw_outer_scope_info_or_feedback_metadata(kAcquireLoad)),
        fb,
    )
    self.assertIs(unwrap_raw(sfi.name_or_scope_info(kAcquireLoad)), si)

    # Real Clang-parsed SetTrustedData defaults to kReleaseStore!
    from clang_frontend import parse_v8_merge_bundle
    from cxx_interpreter import BoundMergeRuntime
    bundle = parse_v8_merge_bundle()
    iso = Isolate(heap)
    rt = BoundMergeRuntime(bundle, heap, iso, LocalIsolate(heap))
    mem.switch_thread(mem.main_thread)
    cp = FixedArray(heap, [], is_trusted=True)
    bc = BytecodeArray(heap, cp)
    rt.call_sfi_method(sfi, "SetTrustedData", bc)
    self.assertIsNotNone(sfi._trusted_loc.history[-1].rel_view)

  def test_bg_local_heap_destroy_and_lazy_persistent_handles(self):
    """Verifies ~LocalHeap() destroys un-detached handles & lazy EnsurePersistentHandles."""
    from v8_heap_model import BackgroundMergeTask

    # Case 1 (R3-1): omitting DetachPersistentHandles leaves PersistentHandles
    # owned by bg_local_heap; destroying bg_local_heap dangles all handles.
    mem = MemoryContext(mode="latest")
    heap = V8Heap(mem)
    iso = Isolate(heap)
    scr = _make_script(heap, n_slots=1)
    hs = HandleScope(iso)
    try:
      sh = cxx_handle(scr, iso)
      task = BackgroundMergeTask(heap)
      task.SetUpOnMainThread(iso, sh)
      heap.bg_local_heap.AttachPersistentHandles(
          task._release_persistent_handles_())
      self.assertIsNone(task.persistent_handles_)
      extra = heap.bg_local_heap.NewPersistentHandle(scr)
      # Destroy bg_local_heap WITHOUT detaching!
      heap.bg_local_heap.destroy()
      with self.assertRaises(DanglingHandleError):
        task.cached_script_.deref()
      with self.assertRaises(DanglingHandleError):
        extra.deref()
    finally:
      hs.__cxx_exit__()

    # Case 2 (L-a): omitting AttachPersistentHandles lets NewPersistentHandle
    # lazily allocate a fresh block (matching V8 local-heap.h:89-92), and the
    # subsequent unique_ptr assignment from DetachPersistentHandles() destroys
    # the original PersistentHandles block owning cached_script_.
    mem2 = MemoryContext(mode="latest")
    heap2 = V8Heap(mem2)
    iso2 = Isolate(heap2)
    scr2 = _make_script(heap2, n_slots=1)
    hs2 = HandleScope(iso2)
    try:
      sh2 = cxx_handle(scr2, iso2)
      task2 = BackgroundMergeTask(heap2)
      task2.SetUpOnMainThread(iso2, sh2)
      # No AttachPersistentHandles: NewPersistentHandle lazily creates a new block.
      lazy_h = heap2.bg_local_heap.NewPersistentHandle(scr2)
      self.assertIsNotNone(lazy_h.deref())
      # Detaching overwrites task2.persistent_handles_ (destroying original block!).
      task2.persistent_handles_ = heap2.bg_local_heap.DetachPersistentHandles()
      heap2.bg_local_heap.destroy()
      # New handle in detached lazy block is alive, but cached_script_ dangles!
      self.assertIsNotNone(lazy_h.deref())
      with self.assertRaises(DanglingHandleError):
        task2.cached_script_.deref()
    finally:
      hs2.__cxx_exit__()

  def test_unordered_map_value_vs_ref_iteration_and_emplace(self):
    """Verifies by-value vs by-ref range-for copy semantics & CxxPair emplace."""
    code = """
    #include <unordered_map>
    int test_map_semantics() {
      std::unordered_map<int, int> m;
      auto r1 = m.emplace(1, 10);
      auto r2 = m.emplace(1, 99);
      if (!r1.second || r2.second) return -1;
      if (r1.first->second != 10) return -2;
      for (auto kv : m) {
        kv.second = 77;
      }
      if (m[1] != 10) return -3;
      for (auto& kv : m) {
        kv.second = 42;
      }
      return m[1];
    }
    """
    self.assertEqual(_run_snippet(code, "test_map_semantics"), 42)

  def test_check_condition_survives_python_dash_O(self):
    with self.assertRaises(InvariantViolationError):
      check_condition(False, "must fail even under -O")


if __name__ == "__main__":
  unittest.main(verbosity=2)
