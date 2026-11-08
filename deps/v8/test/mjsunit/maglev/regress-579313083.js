// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const sinWithApply = Math.sin;
sinWithApply.call = Function.prototype.apply;

function inner(fn, cond, breakCond) {
  let x = true;
  while (cond) {
    // Inner loop resets `x`'s known type in KNA to the unfinalized outer loop
    // Phi type (`kUnknown`) before `fn.call(x)`, while `FinishLoop` later types
    // the outer Phi as `kBoolean` before `!x`.
    for (let k = 0; k < 0; k++) {}
    fn.call(x);
    if (breakCond) break;
    x = false;
  }
  return !x;
}

function outer(breakCond) {
  return inner(Function.prototype.apply, true, breakCond);
}

%PrepareFunctionForOptimization(inner);
%PrepareFunctionForOptimization(outer);

// 1. Calling `fn.call` when it is `Function.prototype.apply` on `Math.sin`
// records `Weak(Math.sin)` with `CallFeedbackContent::kReceiver`.
inner(sinWithApply, true, true);

// 2. Calling `fn.call` when `fn` is `Function.prototype.apply` transitions the
// slot to `many_closures_cell` while `CallFeedbackContent::kReceiver` remains
// set. When `outer` inlines `inner` with constant `Function.prototype.apply`,
// reducing `fn.call(x)` reduces `Function.prototype.apply` and must not
// speculate that `x` equals `many_closures_cell`.
assertThrows(() => outer(false), TypeError);

%OptimizeFunctionOnNextCall(outer);
assertThrows(() => outer(false), TypeError);

// Nested `Function.prototype.apply.apply(target)` records `kReceiver` with
// `Function.prototype.apply` for the outer `apply` call; the inner `apply`
// reduction must not reuse that feedback for `target` and deopt.
function nestedApply(target) {
  return Function.prototype.apply.apply(target);
}

%PrepareFunctionForOptimization(nestedApply);
assertEquals(42, nestedApply(() => 42));
%OptimizeFunctionOnNextCall(nestedApply);
assertEquals(42, nestedApply(() => 42));
assertOptimized(nestedApply);
