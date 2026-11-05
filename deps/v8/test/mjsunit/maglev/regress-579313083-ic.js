// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Case 1: Calling `closure1.apply()` records `Weak(closure1)` with `kReceiver`.
// A subsequent direct call to `closure2` (sharing the same FeedbackCell)
// transitions the slot to `Weak(feedback_cell)` while leaving `kReceiver` set.
// The compiler must ignore `kReceiver` for non-callable feedback targets.
function makeClosure() {
  return function() {
    return 99;
  };
}
const closure1 = makeClosure();
closure1.m = 1;
closure1.m = Function.prototype.apply;
const closure2 = makeClosure();
%PrepareFunctionForOptimization(closure2);
closure2.m = 1;
closure2.m = closure2;

function callClosureMethod(obj) {
  return obj.m();
}

%PrepareFunctionForOptimization(callClosureMethod);
assertEquals(99, callClosureMethod(closure1));
assertEquals(99, callClosureMethod(closure2));
%OptimizeFunctionOnNextCall(callClosureMethod);
assertEquals(99, callClosureMethod(closure2));
assertOptimized(callClosureMethod);

// Case 2: Calling `targetFn.apply()` records `Weak(targetFn)` with `kReceiver`.
// A subsequent direct call to `targetFn` at the same site matches
// `Weak(targetFn)` in `IsMonomorphic` and leaves `kReceiver` set. When the old
// receiver map is deprecated so `obj.m` constant-folds to `targetFn`, the
// compiler must not speculate that the constant target is
// `Function.prototype.apply`.
function targetFn() {
  return 123;
}
function deprecateHelper() {}
targetFn.prop = 1;
targetFn.m = Function.prototype.apply;

function callKnownConstantTargetWithReceiverFeedback(obj) {
  return obj.m();
}

%PrepareFunctionForOptimization(callKnownConstantTargetWithReceiverFeedback);
assertEquals(123, callKnownConstantTargetWithReceiverFeedback(targetFn));

// Deprecate `targetFn`'s map so the compiler ignores it in the LoadIC for `.m`
// and constant-folds `obj.m` to `targetFn` from `plainObj`'s prototype.
deprecateHelper.prop = 1.5;
deprecateHelper.m = Function.prototype.apply;

const plainObj = Object.create({ m: targetFn });
assertEquals(123, callKnownConstantTargetWithReceiverFeedback(plainObj));
%OptimizeFunctionOnNextCall(callKnownConstantTargetWithReceiverFeedback);
assertEquals(123, callKnownConstantTargetWithReceiverFeedback(plainObj));
assertOptimized(callKnownConstantTargetWithReceiverFeedback);
