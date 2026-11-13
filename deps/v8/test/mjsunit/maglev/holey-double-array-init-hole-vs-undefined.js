// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Inlined FixedDoubleArray allocations initialize their elements with raw
// 64 bit stores. Holes must stay holes and undefined must stay undefined, which
// assertEquals tells apart.

function literal() {
  return [1.1, , 3.3];
}
%PrepareFunctionForOptimization(literal);
literal();
%OptimizeMaglevOnNextCall(literal);
assertEquals([1.1, , 3.3], literal());
%OptimizeFunctionOnNextCall(literal);
assertEquals([1.1, , 3.3], literal());

// The store gives the allocation site double feedback, except in single
// generation builds, which have no allocation site tracking.
function arrayN(store) {
  const a = new Array(3);
  if (store) a[0] = 1.1;
  return a;
}
%PrepareFunctionForOptimization(arrayN);
arrayN(true);
%OptimizeMaglevOnNextCall(arrayN);
assertEquals(new Array(3), arrayN(false));
%OptimizeFunctionOnNextCall(arrayN);
assertEquals(new Array(3), arrayN(false));

// Growing within capacity exposes the preallocated slack.
function slack(store) {
  const a = new Array();
  if (store) a[0] = 1.1;
  a.length = 3;
  return a;
}
%PrepareFunctionForOptimization(slack);
slack(true);
%OptimizeMaglevOnNextCall(slack);
assertEquals(new Array(3), slack(false));
%OptimizeFunctionOnNextCall(slack);
assertEquals(new Array(3), slack(false));

function withUndefined() {
  return new Array(1.1, undefined, 3.3);
}
%PrepareFunctionForOptimization(withUndefined);
withUndefined();
%OptimizeMaglevOnNextCall(withUndefined);
assertEquals([1.1, undefined, 3.3], withUndefined());
%OptimizeFunctionOnNextCall(withUndefined);
assertEquals([1.1, undefined, 3.3], withUndefined());

// A loaded hole reads as undefined and must be stored as undefined.
const holey = [1.1, , 3.3];
function withLoadedHole() {
  return new Array(1.1, holey[1], 3.3);
}
%PrepareFunctionForOptimization(withLoadedHole);
withLoadedHole();
%OptimizeMaglevOnNextCall(withLoadedHole);
assertEquals([1.1, undefined, 3.3], withLoadedHole());
%OptimizeFunctionOnNextCall(withLoadedHole);
assertEquals([1.1, undefined, 3.3], withLoadedHole());

// `a` and `b` only live in the deopt frame, so their allocations are elided
// and the deoptimizer rematerializes them when the unseen return deopts.
function rematerialize(deopt) {
  const a = [1.1, , 3.3];
  const b = new Array(1.1, undefined, 3.3);
  if (deopt) return [a, b];
  return 0;
}
%PrepareFunctionForOptimization(rematerialize);
rematerialize(false);
%OptimizeMaglevOnNextCall(rematerialize);
assertEquals([[1.1, , 3.3], [1.1, undefined, 3.3]], rematerialize(true));
// Use a copy with fresh feedback so that Turbolev compiles the return as
// unseen too.
const rematerialize2 = copyFunctionWithFreshFeedback(rematerialize);
%PrepareFunctionForOptimization(rematerialize2);
rematerialize2(false);
%OptimizeFunctionOnNextCall(rematerialize2);
assertEquals([[1.1, , 3.3], [1.1, undefined, 3.3]], rematerialize2(true));
