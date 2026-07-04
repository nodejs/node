// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan
// Flags: --no-concurrent-osr

// The OSR loop header is the peel candidate. The MaglevLoopPeeler must strip the
// %AssertPeeled() marker; if it survives to the Turbolev backend the compile
// fails, so each case both checks results and asserts that peeling fired.

// Int + float accumulators; the post-loop `s + f` has no feedback at OSR
// compile time, so the loop's escaping values are live only in an unconditional
// deopt frame, and the float OSR value exercises the untagging hoist.
function osr_peel(n) {
  let s = 0;
  let f = 0.5;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    s += i;
    f += 1.5;
  }
  return s + f;
}
%PrepareFunctionForOptimization(osr_peel);
assertEquals(220.5, osr_peel(20));

// Int-only escaping value into a downstream unconditional deopt (`s + 1`).
function osr_int_deopt(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    s += i;
  }
  return s + 1;
}
%PrepareFunctionForOptimization(osr_int_deopt);
assertEquals(191, osr_int_deopt(20));

// Multi-exit OSR loop: the break adds a second exit edge, exercising the
// per-target peel-exit-merge with OSR-value phis.
function osr_break(n, limit) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    if (i >= limit) break;
    s += i;
  }
  return s + 1;
}
%PrepareFunctionForOptimization(osr_break);
assertEquals(1226, osr_break(50, 1000));

// Internal if/else where OSR triggers on the first back-edge, leaving the odd
// branch unwarmed; it lowers to an unconditional deopt in the loop body.
// TODO(victorgomes): peel loops with a terminal control node in the body.
function osr_if_else(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    if ((i & 1) === 0) s += i;
    else s -= i;
  }
  return s;
}
%PrepareFunctionForOptimization(osr_if_else);
assertEquals(-25, osr_if_else(50));

// Throwable JS call (no live catch block) in the OSR loop body; the peeler
// clones it and resets its exception-handler info.
function helper(x) { return x + 1; }
%NeverOptimizeFunction(helper);
function osr_call(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    %AssertPeeled();
    s += helper(i);
  }
  return s;
}
%PrepareFunctionForOptimization(osr_call);
assertEquals(210, osr_call(20));

// Nested loops; the inner loop is the OSR entry and the peel candidate.
function osr_nested(n, m) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    for (let j = 0; j < m; j++) {
      %OptimizeOsr();
      %AssertPeeled();
      s += i + j;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(osr_nested);
assertEquals(2800, osr_nested(20, 10));

// try/catch in the OSR loop body: a catch block in the body makes the loop
// non-peelable; the peeler must bail and compile/run correctly (no marker).
function osr_try_catch(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %OptimizeOsr();
    try {
      s += i;
    } catch (e) {
      s -= 1;
    }
  }
  return s;
}
%PrepareFunctionForOptimization(osr_try_catch);
assertEquals(190, osr_try_catch(20));
