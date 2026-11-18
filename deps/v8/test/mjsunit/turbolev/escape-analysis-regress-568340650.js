// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

// 1. Direct constant-index store to rest array followed by eager deopt.
function storeConstantIndex(o, ...r) {
  r[0] = 42;
  o.x;
  return r[0];
}

%PrepareFunctionForOptimization(storeConstantIndex);
assertEquals(42, storeConstantIndex({x: 1}, 1, 2, 3));
assertEquals(42, storeConstantIndex({x: 1}, 1, 2, 3));
%OptimizeFunctionOnNextCall(storeConstantIndex);
assertEquals(42, storeConstantIndex({x: 1}, 1, 2, 3));
assertEquals(42, storeConstantIndex({y: 1, x: 2}, 1, 2, 3));

// 2. Dynamic-index store to rest array followed by eager deopt.
function storeDynamicIndex(o, i, ...r) {
  r[i] = 42;
  o.x;
  return r[i];
}

%PrepareFunctionForOptimization(storeDynamicIndex);
assertEquals(42, storeDynamicIndex({x: 1}, 1, 10, 20, 30));
assertEquals(42, storeDynamicIndex({x: 1}, 1, 10, 20, 30));
%OptimizeFunctionOnNextCall(storeDynamicIndex);
assertEquals(42, storeDynamicIndex({x: 1}, 1, 10, 20, 30));
assertEquals(42, storeDynamicIndex({y: 1, x: 2}, 1, 10, 20, 30));

// 3. Store via inlined callee followed by eager deopt.
function mutateFirst(a) {
  a[0] = 42;
}
function storeViaInlinedCallee(o, ...r) {
  mutateFirst(r);
  o.x;
  return r[0];
}

%PrepareFunctionForOptimization(mutateFirst);
%PrepareFunctionForOptimization(storeViaInlinedCallee);
assertEquals(42, storeViaInlinedCallee({x: 1}, 1, 2, 3));
assertEquals(42, storeViaInlinedCallee({x: 1}, 1, 2, 3));
%OptimizeFunctionOnNextCall(storeViaInlinedCallee);
assertEquals(42, storeViaInlinedCallee({x: 1}, 1, 2, 3));
assertEquals(42, storeViaInlinedCallee({y: 1, x: 2}, 1, 2, 3));

// 4. Store inside an unpeeled loop body after an earlier loop deopt point.
function storeAfterDeoptInLoop(n, o1, o2, ...r) {
  for (let i = 0; i < n; i++) {
    const o = i === 2 ? o2 : o1;
    o.x;
    if (i === 1) r[0] = 42;
  }
  return r[0];
}

%PrepareFunctionForOptimization(storeAfterDeoptInLoop);
assertEquals(42, storeAfterDeoptInLoop(3, {x: 1}, {x: 2}, 1, 2, 3));
assertEquals(42, storeAfterDeoptInLoop(3, {x: 1}, {x: 2}, 1, 2, 3));
%OptimizeFunctionOnNextCall(storeAfterDeoptInLoop);
assertEquals(42, storeAfterDeoptInLoop(3, {x: 1}, {x: 2}, 1, 2, 3));
assertEquals(42, storeAfterDeoptInLoop(3, {x: 1}, {y: 1, x: 2}, 1, 2, 3));

// 5. Store to rest array followed by lazy deopt.
function maybeLazyDeopt(deopt) {
  if (deopt) %DeoptimizeFunction(storeBeforeLazyDeopt);
}
%NeverOptimizeFunction(maybeLazyDeopt);

function storeBeforeLazyDeopt(deopt, ...r) {
  r[0] = 42;
  maybeLazyDeopt(deopt);
  return r[0];
}

%PrepareFunctionForOptimization(storeBeforeLazyDeopt);
assertEquals(42, storeBeforeLazyDeopt(false, 1, 2, 3));
assertEquals(42, storeBeforeLazyDeopt(false, 1, 2, 3));
%OptimizeFunctionOnNextCall(storeBeforeLazyDeopt);
assertEquals(42, storeBeforeLazyDeopt(false, 1, 2, 3));
assertEquals(42, storeBeforeLazyDeopt(true, 1, 2, 3));

// 6. Store of a fresh candidate object (write barrier) followed by eager deopt.
function storeObject(o, ...r) {
  r[0] = {v: 42};
  o.x;
  return r[0].v;
}

%PrepareFunctionForOptimization(storeObject);
assertEquals(42, storeObject({x: 1}, 1, 2));
assertEquals(42, storeObject({x: 1}, 1, 2));
%OptimizeFunctionOnNextCall(storeObject);
assertEquals(42, storeObject({x: 1}, 1, 2));
assertEquals(42, storeObject({y: 1, x: 2}, 1, 2));

// 7. Store to strict unmapped arguments object followed by eager deopt.
function storeStrictArguments(o) {
  "use strict";
  arguments[1] = 42;
  o.x;
  return arguments[1];
}

%PrepareFunctionForOptimization(storeStrictArguments);
assertEquals(42, storeStrictArguments({x: 1}, 10, 20));
assertEquals(42, storeStrictArguments({x: 1}, 10, 20));
%OptimizeFunctionOnNextCall(storeStrictArguments);
assertEquals(42, storeStrictArguments({x: 1}, 10, 20));
assertEquals(42, storeStrictArguments({y: 1, x: 2}, 10, 20));

// 8. Read-only rest array is still elided by escape analysis.
function readOnlyRest(i, ...r) {
  %AssertEscapeAnalysisElided(r);
  return r[0] + r[i] + r.length;
}

%PrepareFunctionForOptimization(readOnlyRest);
assertEquals(32, readOnlyRest(1, 10, 20));
assertEquals(32, readOnlyRest(1, 10, 20));
%OptimizeFunctionOnNextCall(readOnlyRest);
assertEquals(32, readOnlyRest(1, 10, 20));
