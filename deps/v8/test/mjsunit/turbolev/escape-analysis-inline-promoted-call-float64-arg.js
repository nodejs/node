// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan
// Flags: --max-inlined-bytecode-size-small=10
// Flags: --max-inlined-bytecode-size-small-with-heapnum-in-out=75
// Flags: --max-inlined-bytecode-size-cumulative=0

// Bytecode length > 10 and <= 75: only eligible for small-function inlining
// when IsSmallFunction sees the Float64 argument (unwrapped from
// Float64ToTagged when VisitCall promotes the polymorphic call).
function makeFloatBox(x) {
  const a = x + 1.5;
  const b = a * 2.0;
  return {v: b};
}
%PrepareFunctionForOptimization(makeFloatBox);
makeFloatBox(1.5);
makeFloatBox(2.5);

function getTarget() {
  return makeFloatBox;
}
%PrepareFunctionForOptimization(getTarget);
%DisableEagerInlining(getTarget);
getTarget();

function invoke(fn, x) {
  return fn(x);
}
%PrepareFunctionForOptimization(invoke);
// Make the call IC in `invoke` polymorphic so `fn(x)` is emitted as a generic
// Call during graph building and promoted to CallKnownJSFunction in
// MaglevGraphOptimizer::VisitCall after `getTarget()` is inlined.
invoke(v => ({v}), 1.5);
invoke(v => ({v: v + 1}), 2.5);

function caller(a) {
  const dummy = {d: 1};
  const fn = getTarget();
  const box = invoke(fn, a + 0.25);
  %AssertEscapeAnalysisElided(box);
  return box.v + dummy.d;
}

%PrepareFunctionForOptimization(caller);
assertEquals(6.5, caller(1.0));
assertEquals(8.5, caller(2.0));
%OptimizeFunctionOnNextCall(caller);
assertEquals(10.5, caller(3.0));
assertOptimized(caller);

// 2. Queued call receiving an untagged Float64 ReturnedValue from an earlier
// non-eagerly-inlined helper, exercising MaglevCallerDetails::UnwrapArguments()
// before IsSmallFunction in MaglevInliner::InlineCallSites.
function makeFloat(a) {
  return a + 0.25;
}
%PrepareFunctionForOptimization(makeFloat);
%DisableEagerInlining(makeFloat);
makeFloat(1.0);

function makeFloatBoxQueued(x) {
  const a = x + 1.5;
  const b = a * 2.0;
  return {v: b};
}
%PrepareFunctionForOptimization(makeFloatBoxQueued);
%DisableEagerInlining(makeFloatBoxQueued);
makeFloatBoxQueued(1.5);
makeFloatBoxQueued(2.5);

function callerReturnedValue(a) {
  const dummy = {d: 1};
  const v = makeFloat(a);
  const box = makeFloatBoxQueued(v);
  %AssertEscapeAnalysisElided(box);
  return box.v + dummy.d;
}
%PrepareFunctionForOptimization(callerReturnedValue);
assertEquals(6.5, callerReturnedValue(1.0));
assertEquals(8.5, callerReturnedValue(2.0));
%OptimizeFunctionOnNextCall(callerReturnedValue);
assertEquals(10.5, callerReturnedValue(3.0));
assertOptimized(callerReturnedValue);
