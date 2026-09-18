// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// Peelable loops carry a %AssertPeeled() marker that the MaglevLoopPeeler strips
// when it peels the loop. If peeling stops firing the marker survives to the
// Turbolev backend and the compile fails, so these positively assert that
// peeling happened instead of only checking results. Loops the peeler does not
// peel yet carry the inverse %AssertNotPeeled() marker (which fails the compile
// if they ever start peeling) plus a TODO(victorgomes) note.

// Canonical counted for-loop: header is a BranchIfInt32Compare, backedge
// JumpLoop. Exercises the peel-exit-merge (PEM) path (two-tail clone).
function counted_loop(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(counted_loop);
assertEquals(45, counted_loop(10));
assertEquals(45, counted_loop(10));
%OptimizeFunctionOnNextCall(counted_loop);
assertEquals(4950, counted_loop(100));
assertEquals(0, counted_loop(0));
assertEquals(0, counted_loop(1) === undefined ? 0 : counted_loop(1));
assertOptimized(counted_loop);

// Empty loop (n=0) — peeled iteration's branch must skip the body.
function empty_loop(n) {
  let acc = 42;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    acc *= 2;
  }
  return acc;
}
%PrepareFunctionForOptimization(empty_loop);
assertEquals(168, empty_loop(2));
assertEquals(168, empty_loop(2));
%OptimizeFunctionOnNextCall(empty_loop);
assertEquals(42, empty_loop(0));
assertEquals(84, empty_loop(1));
assertEquals(168, empty_loop(2));
assertEquals(43008, empty_loop(10));
assertOptimized(empty_loop);

// while-style loop with the condition evaluated at the header.
function while_loop(n) {
  let i = 0;
  let r = 0;
  while (i < n) {
    %AssertPeeled();
    r = r + i + 1;
    i++;
  }
  return r;
}
%PrepareFunctionForOptimization(while_loop);
assertEquals(55, while_loop(10));
assertEquals(55, while_loop(10));
%OptimizeFunctionOnNextCall(while_loop);
assertEquals(0, while_loop(0));
assertEquals(1, while_loop(1));
assertEquals(5050, while_loop(100));
assertOptimized(while_loop);

// Loop where the header phi is read after the loop — exercises the
// rewiring of the header phi's pre-header input to the PEM phi.
function read_after_loop(n) {
  let i = 0;
  while (i < n) {
    %AssertPeeled();
    i = i + 1;
  }
  return i;
}
%PrepareFunctionForOptimization(read_after_loop);
assertEquals(5, read_after_loop(5));
assertEquals(5, read_after_loop(5));
%OptimizeFunctionOnNextCall(read_after_loop);
assertEquals(0, read_after_loop(0));
assertEquals(7, read_after_loop(7));
assertOptimized(read_after_loop);

// Loop with internal if/else (merge inside the body). Exercises the
// internal-merge cloning path: per-merge state construction via
// NewForPeelExit and phi cloning at the internal merge.
function with_if_else(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i % 2 === 0) s += i;
    else s -= i;
  }
  return s;
}
%PrepareFunctionForOptimization(with_if_else);
assertEquals(-5, with_if_else(10));
assertEquals(-5, with_if_else(10));
%OptimizeFunctionOnNextCall(with_if_else);
assertEquals(0, with_if_else(0));
assertEquals(0, with_if_else(1));
assertEquals(-5, with_if_else(10));
assertEquals(-50, with_if_else(100));
assertOptimized(with_if_else);

// Body where one side of the internal branch doesn't update the loop
// variable — exercises phi merging for slots untouched on one path.
function one_sided_update(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i > 3) s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(one_sided_update);
assertEquals(39, one_sided_update(10));
assertEquals(39, one_sided_update(10));
%OptimizeFunctionOnNextCall(one_sided_update);
assertEquals(0, one_sided_update(3));
assertEquals(39, one_sided_update(10));
assertOptimized(one_sided_update);

// Loop whose header reads an object property as the bound. The header
// contains a LoadTaggedField (non-pure: can_read). Under the old
// topology the body-skipped path would re-execute the header and load
// the bound twice; with the new PEM topology the cloned header's exit
// branch goes directly to PEM, bypassing the original header, so the
// load fires exactly once for an empty loop. The test verifies the
// result across n=0/1/100; correctness is preserved regardless of
// whether peeling fires, but with the new topology this loop is
// peelable.
function header_load_bound(obj) {
  let s = 0;
  for (let i = 0; i < obj.lim; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(header_load_bound);
assertEquals(45, header_load_bound({lim: 10}));
assertEquals(45, header_load_bound({lim: 10}));
%OptimizeFunctionOnNextCall(header_load_bound);
assertEquals(45, header_load_bound({lim: 10}));
assertEquals(0, header_load_bound({lim: 0}));
assertEquals(0, header_load_bound({lim: 1}));
assertEquals(4950, header_load_bound({lim: 100}));
assertOptimized(header_load_bound);

// Multi-exit: counted loop with an early break. Two exit edges: header
// exit (i >= n) and break exit (i > threshold). Both converge at the
// same post-loop block — exercises the multi-pred-merge-target PEM
// handling.
function break_loop(n, threshold) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i > threshold) break;
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(break_loop);
assertEquals(45, break_loop(10, 100));  // No break, full loop.
assertEquals(45, break_loop(10, 100));
%OptimizeFunctionOnNextCall(break_loop);
assertEquals(0, break_loop(0, 100));    // Body skipped (n=0).
assertEquals(0, break_loop(1, 100));    // One iteration.
assertEquals(15, break_loop(10, 5));    // Break at i=6 (0+1+2+3+4+5).
assertEquals(0, break_loop(10, -1));    // Break at i=0.
assertEquals(45, break_loop(10, 100));
assertOptimized(break_loop);

// Break path writes a local read after the loop. The local's value
// differs between the header-exit and break-exit paths, so loop_end's
// existing phi at the break slot needs the PEM phi update.
function break_returns_local(n) {
  let result = -1;
  for (let i = 0; i < n; i++) {
    // TODO(victorgomes): not peeled yet (exit target has phis).
    %AssertNotPeeled();
    if (i === 5) {
      result = i * 2;
      break;
    }
    result = i;
  }
  return result;
}
%PrepareFunctionForOptimization(break_returns_local);
assertEquals(10, break_returns_local(10));
assertEquals(10, break_returns_local(10));
%OptimizeFunctionOnNextCall(break_returns_local);
assertEquals(-1, break_returns_local(0));   // Body skipped.
assertEquals(0, break_returns_local(1));
assertEquals(10, break_returns_local(6));   // Break at i=5; result = 10.
assertEquals(10, break_returns_local(100));
assertOptimized(break_returns_local);

// Two break statements at different points, both to the same loop_end.
// Three exit edges total (header + two breaks). Verifies that multiple
// PEMs sharing the same merge target at different slots work.
function double_break(a, b, n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    // TODO(victorgomes): not peeled yet (exit target has phis).
    %AssertNotPeeled();
    if (i === a) break;
    s += i;
    if (i === b) break;
  }
  return s;
}
%PrepareFunctionForOptimization(double_break);
assertEquals(45, double_break(100, 100, 10));
assertEquals(45, double_break(100, 100, 10));
%OptimizeFunctionOnNextCall(double_break);
assertEquals(0, double_break(100, 100, 0));   // Body skipped.
assertEquals(0, double_break(0, 100, 10));    // First break at i=0.
assertEquals(0, double_break(100, 0, 10));    // Second break at i=0 (after s += 0).
assertEquals(10, double_break(5, 100, 10));   // First break at i=5: 0+1+2+3+4=10.
assertEquals(10, double_break(100, 4, 10));   // Second break at i=4: 0+1+2+3+4=10.
assertEquals(45, double_break(100, 100, 10));
assertOptimized(double_break);

// Labelled break from inner targets outer's post-loop. The innermost
// peel-candidate is `inner`; `break outer` from inside `inner` is an
// exit of `inner` (target offset is outside inner's range). Verifies
// the bytecode-offset predicate handles labelled exits.
function labelled_break_outer(n, m, threshold) {
  let s = 0;
  outer: for (let i = 0; i < n; i++) {
    inner: for (let j = 0; j < m; j++) {
      // TODO(victorgomes): not peeled yet (exit target has phis).
      %AssertNotPeeled();
      if (j === threshold) break outer;
      s += j;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(labelled_break_outer);
assertEquals(30, labelled_break_outer(3, 5, 100));  // No break: 3 * (0+1+2+3+4) = 30.
assertEquals(30, labelled_break_outer(3, 5, 100));
%OptimizeFunctionOnNextCall(labelled_break_outer);
assertEquals(0, labelled_break_outer(0, 5, 100));   // Outer skipped.
assertEquals(0, labelled_break_outer(3, 0, 100));   // Inner skipped.
assertEquals(3, labelled_break_outer(3, 5, 3));     // Break at j=3 in first iter: 0+1+2.
assertOptimized(labelled_break_outer);

// Labelled continue to outer. From inside inner, `continue outer`
// targets outer's continue point — outside inner's range, so detected
// as an exit edge from inner.
function labelled_continue_outer(n, m, skip) {
  let s = 0;
  outer: for (let i = 0; i < n; i++) {
    inner: for (let j = 0; j < m; j++) {
      %AssertPeeled();
      if (j === skip) continue outer;
      s += j;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(labelled_continue_outer);
assertEquals(30, labelled_continue_outer(3, 5, 100));  // No continue.
assertEquals(30, labelled_continue_outer(3, 5, 100));
%OptimizeFunctionOnNextCall(labelled_continue_outer);
assertEquals(0, labelled_continue_outer(0, 5, 100));
assertEquals(0, labelled_continue_outer(3, 0, 100));
assertEquals(9, labelled_continue_outer(3, 5, 3));     // Each iter: 0+1+2 = 3, then continue.
assertOptimized(labelled_continue_outer);

// Labelled continue to inner only — target inner's own header, NOT an
// exit edge. Sanity check that the bytecode-offset predicate doesn't
// over-classify in-loop continues.
function labelled_continue_inner_only(n, skip) {
  let s = 0;
  inner: for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i === skip) continue inner;
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(labelled_continue_inner_only);
assertEquals(45, labelled_continue_inner_only(10, -1));  // No continue fires.
assertEquals(45, labelled_continue_inner_only(10, -1));
%OptimizeFunctionOnNextCall(labelled_continue_inner_only);
assertEquals(0, labelled_continue_inner_only(0, -1));
assertEquals(40, labelled_continue_inner_only(10, 5));   // skip i=5: 45 - 5 = 40.
assertOptimized(labelled_continue_inner_only);

// Inlined-loop peeling. The loop lives in an inlined helper whose
// `MaglevCompilationUnit` differs from the toplevel; the peeler must
// thread the right unit through `NewForPeelExit` (header_load's frame
// layout depends on the helper's parameter/register count, not the
// caller's). Without proper plumbing the peel produces a corrupted
// frame state and crashes during turboshaft translation.
function inlined_sum(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
function caller_simple(n) {
  return inlined_sum(n);
}
%PrepareFunctionForOptimization(inlined_sum);
%PrepareFunctionForOptimization(caller_simple);
assertEquals(45, caller_simple(10));
assertEquals(45, caller_simple(10));
%OptimizeFunctionOnNextCall(caller_simple);
assertEquals(0, caller_simple(0));     // Body skipped on n=0.
assertEquals(0, caller_simple(1));
assertEquals(45, caller_simple(10));
assertEquals(4950, caller_simple(100));
assertOptimized(caller_simple);

// Inlined helper with MORE parameters than the caller — exercises the
// register-count divergence between toplevel and inlined units. If the
// peeler uses the toplevel unit's layout, `ForEachValue` iterates the
// wrong number of slots and either reads out of bounds or misaligns
// registers.
function inlined_many_args(a, b, c, n) {
  let s = a + b + c;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
function caller_many_args(n) {
  return inlined_many_args(1, 2, 3, n);
}
%PrepareFunctionForOptimization(inlined_many_args);
%PrepareFunctionForOptimization(caller_many_args);
assertEquals(51, caller_many_args(10));  // 1+2+3+45 = 51.
assertEquals(51, caller_many_args(10));
%OptimizeFunctionOnNextCall(caller_many_args);
assertEquals(6, caller_many_args(0));    // 1+2+3+0 = 6.
assertEquals(6, caller_many_args(1));    // 1+2+3+0 = 6.
assertEquals(51, caller_many_args(10));
assertOptimized(caller_many_args);

// Two-level inlining: helper inlined into intermediate, intermediate
// inlined into caller. The loop's unit is the innermost helper's,
// reached through two layers of inlined frames.
function inlined_inner(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
function inlined_middle(n) {
  return inlined_inner(n);
}
function caller_two_level(n) {
  return inlined_middle(n);
}
%PrepareFunctionForOptimization(inlined_inner);
%PrepareFunctionForOptimization(inlined_middle);
%PrepareFunctionForOptimization(caller_two_level);
assertEquals(45, caller_two_level(10));
assertEquals(45, caller_two_level(10));
%OptimizeFunctionOnNextCall(caller_two_level);
assertEquals(0, caller_two_level(0));
assertEquals(45, caller_two_level(10));
assertEquals(4950, caller_two_level(100));
assertOptimized(caller_two_level);

// Nested loops: only the INNERMOST is a peel candidate; the outer loop
// contains the inner one's blocks so it's not innermost. With the
// PEM-per-exit structure, the inner peel inserts its PEM and edge-
// splits between the inner header and the inner exit's downstream,
// which is still inside the outer loop.
function nested_basic(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < n; j++) {
      %AssertPeeled();
      s += j;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(nested_basic);
assertEquals(450, nested_basic(10));  // 10 * (0+1+..+9) = 10*45 = 450.
assertEquals(450, nested_basic(10));
%OptimizeFunctionOnNextCall(nested_basic);
assertEquals(0, nested_basic(0));
assertEquals(0, nested_basic(1));     // 1 * 0 = 0.
assertEquals(450, nested_basic(10));
assertEquals(495000, nested_basic(100)); // 100 * 4950.
assertOptimized(nested_basic);

// Nested loops where the outer's iteration count depends on the inner's
// running sum. The inner sum escapes via the outer header phi for s
// (read-after-loop pattern from the inner exit's POV).
function nested_inner_sum_outer_uses(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < i; j++) {
      %AssertPeeled();
      s += j;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(nested_inner_sum_outer_uses);
assertEquals(120, nested_inner_sum_outer_uses(10)); // sum of 0+0+1+...+(8) iterated.
assertEquals(120, nested_inner_sum_outer_uses(10));
%OptimizeFunctionOnNextCall(nested_inner_sum_outer_uses);
assertEquals(0, nested_inner_sum_outer_uses(0));
assertEquals(0, nested_inner_sum_outer_uses(1));    // inner runs 0 iterations.
assertEquals(120, nested_inner_sum_outer_uses(10));
assertOptimized(nested_inner_sum_outer_uses);

// Continue at the start of body: skip even iterations. The continue
// targets the inner's own header (NOT an exit edge), so the peeler
// sees this as a single-exit loop with internal merges from the
// continue-fall-through paths.
function continue_skip_evens(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if ((i & 1) === 0) continue;
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(continue_skip_evens);
assertEquals(25, continue_skip_evens(10));  // 1+3+5+7+9 = 25.
assertEquals(25, continue_skip_evens(10));
%OptimizeFunctionOnNextCall(continue_skip_evens);
assertEquals(0, continue_skip_evens(0));
assertEquals(0, continue_skip_evens(1));    // i=0 skipped.
assertEquals(25, continue_skip_evens(10));
assertOptimized(continue_skip_evens);

// Inlined helper called inside the loop body. The helper itself is
// inlined into the loop's body block, so the cloner sees the inlined
// helper's nodes as ordinary body nodes. Exercises the cloner over
// inlined-frame deopt chains (CloneDeoptFrame walks parents through
// InlinedArgumentsDeoptFrame even when no arg-count mismatch).
function double_it(x) { return x + x; }
function calls_helper_in_loop(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += double_it(i);
  }
  return s;
}
%PrepareFunctionForOptimization(double_it);
%PrepareFunctionForOptimization(calls_helper_in_loop);
assertEquals(90, calls_helper_in_loop(10));  // 2*(0+1+..+9) = 90.
assertEquals(90, calls_helper_in_loop(10));
%OptimizeFunctionOnNextCall(calls_helper_in_loop);
assertEquals(0, calls_helper_in_loop(0));
assertEquals(0, calls_helper_in_loop(1));
assertEquals(90, calls_helper_in_loop(10));
assertEquals(9900, calls_helper_in_loop(100));
assertOptimized(calls_helper_in_loop);

// Inlined helper whose body contains a break loop. The helper is
// inlined into the caller; the caller's optimized graph contains the
// helper's break-loop as part of its body. Peeler runs on the
// (inlined) break-loop, exercising the multi-exit code path through
// the per-target PEM grouping inside an inlined unit.
function helper_with_break(n, t) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    if (i > t) break;
    s += i;
  }
  return s;
}
function caller_inlined_break(n, t) {
  return helper_with_break(n, t);
}
%PrepareFunctionForOptimization(helper_with_break);
%PrepareFunctionForOptimization(caller_inlined_break);
assertEquals(45, caller_inlined_break(10, 100));
assertEquals(45, caller_inlined_break(10, 100));
%OptimizeFunctionOnNextCall(caller_inlined_break);
assertEquals(0, caller_inlined_break(0, 100));
assertEquals(15, caller_inlined_break(10, 5));    // 0+1+..+5 = 15.
assertEquals(0, caller_inlined_break(10, -1));    // Break at i=0.
assertOptimized(caller_inlined_break);

// Inlined helper containing a NESTED loop. The peeler must classify
// only the innermost loop as a peel candidate and peel it inside the
// inlined unit. Exercises (a) inlined-unit plumbing AND (b) the
// nested-loop "innermost only" classification.
function helper_nested(n, m) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < m; j++) {
      %AssertPeeled();
      s += i + j;
    }
  }
  return s;
}
function caller_inlined_nested(n, m) {
  return helper_nested(n, m);
}
%PrepareFunctionForOptimization(helper_nested);
%PrepareFunctionForOptimization(caller_inlined_nested);
assertEquals(45, caller_inlined_nested(5, 3));   // sum i in 0..4, j in 0..2 of (i+j) = 45.
assertEquals(45, caller_inlined_nested(5, 3));
%OptimizeFunctionOnNextCall(caller_inlined_nested);
assertEquals(0, caller_inlined_nested(0, 5));
assertEquals(0, caller_inlined_nested(5, 0));
assertEquals(45, caller_inlined_nested(5, 3));
assertOptimized(caller_inlined_nested);

// Loop whose bound is an inlined helper call. The bound is computed
// once before the loop (loop-invariant) but the helper returns a value
// that flows into the header's comparison.
function compute_bound(x) { return x + 1; }
function loop_with_helper_bound(n) {
  let lim = compute_bound(n);
  let s = 0;
  for (let i = 0; i < lim; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(compute_bound);
%PrepareFunctionForOptimization(loop_with_helper_bound);
assertEquals(55, loop_with_helper_bound(10));  // lim=11; 0+1+..+10 = 55.
assertEquals(55, loop_with_helper_bound(10));
%OptimizeFunctionOnNextCall(loop_with_helper_bound);
assertEquals(0, loop_with_helper_bound(-1));   // lim=0.
assertEquals(0, loop_with_helper_bound(0));    // lim=1; one iteration: s+=0.
assertEquals(55, loop_with_helper_bound(10));
assertOptimized(loop_with_helper_bound);

// for-loop with an empty user body. The `i++` update still lives in the body
// block (not the header), so the body holds the increment nodes
// (Int32AddWithOverflow + Int32ToNumber) and nothing else. Exercises the
// minimal-body case.
function empty_body(n) {
  let i = 0;
  for (; i < n; i++) %AssertPeeled();
  return i;
}
%PrepareFunctionForOptimization(empty_body);
assertEquals(10, empty_body(10));
assertEquals(10, empty_body(10));
%OptimizeFunctionOnNextCall(empty_body);
assertEquals(0, empty_body(0));
assertEquals(1, empty_body(1));
assertEquals(100, empty_body(100));
assertOptimized(empty_body);

// Loop using two header phis where one is unused after the loop.
// Exercises the case where header phi rewires only affect a subset
// of escaping values.
function unused_phi(n) {
  let s = 0;
  let unused = 100;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i;
    unused = unused * 2;  // Computed but discarded.
  }
  return s;
}
%PrepareFunctionForOptimization(unused_phi);
assertEquals(45, unused_phi(10));
assertEquals(45, unused_phi(10));
%OptimizeFunctionOnNextCall(unused_phi);
assertEquals(0, unused_phi(0));
assertEquals(45, unused_phi(10));
assertOptimized(unused_phi);

// While-true loop with explicit break — a do-while-style pattern. The
// header is an unconditional branch and the exit comes solely from a
// break inside the body. Exercises the case where the header has no
// natural exit edge.
function while_true_break(n) {
  let s = 0;
  let i = 0;
  while (true) {
    %AssertPeeled();
    if (i >= n) break;
    s += i;
    i++;
  }
  return s;
}
%PrepareFunctionForOptimization(while_true_break);
assertEquals(45, while_true_break(10));
assertEquals(45, while_true_break(10));
%OptimizeFunctionOnNextCall(while_true_break);
assertEquals(0, while_true_break(0));
assertEquals(0, while_true_break(1));   // s=0 at i=0, then i=1>=1 → break.
assertEquals(45, while_true_break(10));
assertOptimized(while_true_break);

// Loop variable used as bitwise op operand. Exercises that bitwise
// nodes (Int32BitwiseXor / And / Or) are clone-safe.
function bitwise_ops(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s = s ^ (i & 0xFF);
  }
  return s;
}
%PrepareFunctionForOptimization(bitwise_ops);
assertEquals(1, bitwise_ops(10));  // 0^0^1^2^3^4^5^6^7^8^9 = 1.
assertEquals(1, bitwise_ops(10));
%OptimizeFunctionOnNextCall(bitwise_ops);
assertEquals(0, bitwise_ops(0));
assertEquals(1, bitwise_ops(10));
assertOptimized(bitwise_ops);

// Floating-point accumulator. Exercises the cloner over Float64 nodes
// (Float64Add, ChangeInt32ToFloat64, etc.).
function float_accumulator(n) {
  let s = 0.5;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s = s + 1.5;
  }
  return s;
}
%PrepareFunctionForOptimization(float_accumulator);
assertEquals(15.5, float_accumulator(10));
assertEquals(15.5, float_accumulator(10));
%OptimizeFunctionOnNextCall(float_accumulator);
assertEquals(0.5, float_accumulator(0));
assertEquals(2, float_accumulator(1));
assertEquals(15.5, float_accumulator(10));
assertOptimized(float_accumulator);

// Two loops back-to-back. Each is independently peelable; the peeler
// must process them in sequence without state leak between candidates.
function back_to_back_loops(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    // TODO(victorgomes): not peeled yet (body value used downstream).
    %AssertNotPeeled();
    s += i;
  }
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    s += i * 2;
  }
  return s;
}
%PrepareFunctionForOptimization(back_to_back_loops);
assertEquals(135, back_to_back_loops(10)); // 45 + 90 = 135.
assertEquals(135, back_to_back_loops(10));
%OptimizeFunctionOnNextCall(back_to_back_loops);
assertEquals(0, back_to_back_loops(0));
assertEquals(135, back_to_back_loops(10));
assertOptimized(back_to_back_loops);

// Loop whose bound is the result of an inlined helper, computed once before
// the loop. small_helper is inlined a single time; peeling duplicates only the
// loop body (one iteration). Both the peeled copy and the loop refer to the
// single loop-invariant `pre` value — the helper's body is not duplicated.
function small_helper(x) { return x * 3; }
function caller_calls_twice(n) {
  let pre = small_helper(n);
  let s = 0;
  for (let i = 0; i < pre; i++) {
    %AssertPeeled();
    s += i;
  }
  return s;
}
%PrepareFunctionForOptimization(small_helper);
%PrepareFunctionForOptimization(caller_calls_twice);
assertEquals(435, caller_calls_twice(10)); // pre=30, sum 0..29 = 435.
assertEquals(435, caller_calls_twice(10));
%OptimizeFunctionOnNextCall(caller_calls_twice);
assertEquals(0, caller_calls_twice(0));
assertEquals(3, caller_calls_twice(1));    // pre=3, sum 0+1+2 = 3.
assertEquals(435, caller_calls_twice(10));
assertOptimized(caller_calls_twice);

// Real do-while loop: the body runs once before the condition is tested, so
// the exit edge leaves a body block's Branch (not the header) and the escaping
// value is body-defined. The peeler currently bails on it ("body value used
// downstream"); this checks the bail path stays correct on a bottom-tested
// loop. (A future relaxation of that restriction should peel it.)
function do_while_sum(n) {
  let s = 0;
  let i = 0;
  do {
    s += i;
    i++;
  } while (i < n);
  return s;
}
%PrepareFunctionForOptimization(do_while_sum);
assertEquals(45, do_while_sum(10));
assertEquals(45, do_while_sum(10));
%OptimizeFunctionOnNextCall(do_while_sum);
assertEquals(0, do_while_sum(0));   // Body runs once (s += 0), then exits.
assertEquals(0, do_while_sum(1));
assertEquals(10, do_while_sum(5));  // 0+1+2+3+4 = 10.
assertEquals(45, do_while_sum(10));
assertOptimized(do_while_sum);

// Two interdependent header phis (Fibonacci-style): each iteration `a` takes
// the previous `b`, and `b` takes `a + b`. One header phi's back-edge input is
// another header phi, so the new loop phis must reference each other after
// peeling — exercises the loop-phi cross-reference fixup in
// RewireDownstreamPhiRefs (without it, `a`'s loop phi would read the stale
// header phi for `b`).
function recursive_phis(n) {
  let a = 1;
  let b = 1;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    let next = a + b;
    a = b;
    b = next;
  }
  return a * 1000 + b;
}
%PrepareFunctionForOptimization(recursive_phis);
assertEquals(89144, recursive_phis(10));  // a=89, b=144 after 10 iters.
assertEquals(89144, recursive_phis(10));
%OptimizeFunctionOnNextCall(recursive_phis);
assertEquals(1001, recursive_phis(0));    // a=1, b=1.
assertEquals(1002, recursive_phis(1));    // a=1, b=2.
assertEquals(2003, recursive_phis(2));    // a=2, b=3.
assertEquals(89144, recursive_phis(10));
assertOptimized(recursive_phis);

// Loop whose forward entry into the header is a critical edge: `i` is set
// before the branch, so the if-true arm jumps straight into the loop header
// (a 2-pred merge), making the pre-header an edge-split block. Peeling would
// retarget that edge-split to the stateless cloned header and break the
// post-peel KNA recompute, so the peeler bails. This checks the bail path is
// correct and crash-free.
function edge_split_preheader(n, c) {
  let s = 0;
  let i = 0;
  if (c) {
    for (; i < n; i++) s += i;
    return s;
  }
  return -1;
}
%PrepareFunctionForOptimization(edge_split_preheader);
assertEquals(45, edge_split_preheader(10, true));
assertEquals(45, edge_split_preheader(10, true));
%OptimizeFunctionOnNextCall(edge_split_preheader);
assertEquals(-1, edge_split_preheader(10, false));
assertEquals(0, edge_split_preheader(0, true));
assertEquals(45, edge_split_preheader(10, true));
assertOptimized(edge_split_preheader);
