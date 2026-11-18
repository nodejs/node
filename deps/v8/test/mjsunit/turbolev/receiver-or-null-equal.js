// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --turbofan

const o1 = {x: 1};
const o2 = {x: 2};

function eq(a, b) {
  return a == b;
}

function neq(a, b) {
  return a != b;
}

function strictEq(a, b) {
  return a === b;
}

// Static fast paths that must NOT depend on NoUndetectableObjectsProtector.
function eqNullish(a, which) {
  const rhs = which === 1 ? null : undefined;
  return a == rhs;
}

function eqBothNullish(w1, w2) {
  const lhs = w1 === 1 ? null : undefined;
  const rhs = w2 === 1 ? null : undefined;
  return lhs == rhs;
}

function eqDetectableObj(a) {
  return a == o1;
}

function eqSelf(a) {
  return a == a;
}

function eqKnownReceiver(a, b, c) {
  if (a == b) return true;
  return a == c;
}

function eqDeopt(a, b) {
  return a == b;
}

%PrepareFunctionForOptimization(eq);
%PrepareFunctionForOptimization(neq);
%PrepareFunctionForOptimization(strictEq);
%PrepareFunctionForOptimization(eqNullish);
%PrepareFunctionForOptimization(eqBothNullish);
%PrepareFunctionForOptimization(eqDetectableObj);
%PrepareFunctionForOptimization(eqSelf);
%PrepareFunctionForOptimization(eqKnownReceiver);
%PrepareFunctionForOptimization(eqDeopt);

for (let i = 0; i < 10; i++) {
  assertTrue(eq(o1, o1));
  assertFalse(eq(o1, o2));
  assertFalse(eq(o1, null));
  assertFalse(eq(null, o1));
  assertFalse(eq(o1, undefined));
  assertFalse(eq(undefined, o1));
  assertTrue(eq(null, null));
  assertTrue(eq(undefined, undefined));
  assertTrue(eq(null, undefined));
  assertTrue(eq(undefined, null));

  assertFalse(neq(o1, o1));
  assertTrue(neq(o1, o2));
  assertTrue(neq(o1, null));
  assertTrue(neq(null, o1));
  assertFalse(neq(null, undefined));
  assertFalse(neq(undefined, null));

  assertTrue(strictEq(o1, o1));
  assertFalse(strictEq(o1, o2));
  assertFalse(strictEq(null, undefined));
  assertTrue(strictEq(null, null));
  assertTrue(strictEq(undefined, undefined));

  assertFalse(eqNullish(o1, 1));
  assertTrue(eqNullish(null, 0));
  assertTrue(eqNullish(undefined, 1));

  assertTrue(eqBothNullish(1, 0));
  assertTrue(eqBothNullish(0, 1));

  assertTrue(eqDetectableObj(o1));
  assertFalse(eqDetectableObj(o2));
  assertFalse(eqDetectableObj(null));
  assertFalse(eqDetectableObj(undefined));

  assertTrue(eqSelf(o1));
  assertTrue(eqSelf(null));
  assertTrue(eqSelf(undefined));

  assertFalse(eqKnownReceiver(o1, o2, null));
  assertTrue(eqKnownReceiver(o1, o2, o1));
  assertTrue(eqKnownReceiver(o1, o1, null));

  assertFalse(eqDeopt(o1, null));
  assertTrue(eqDeopt(null, undefined));
}

%OptimizeFunctionOnNextCall(eq);
%OptimizeFunctionOnNextCall(neq);
%OptimizeFunctionOnNextCall(strictEq);
%OptimizeFunctionOnNextCall(eqNullish);
%OptimizeFunctionOnNextCall(eqBothNullish);
%OptimizeFunctionOnNextCall(eqDetectableObj);
%OptimizeFunctionOnNextCall(eqSelf);
%OptimizeFunctionOnNextCall(eqKnownReceiver);
%OptimizeFunctionOnNextCall(eqDeopt);

// 1. Test optimized fast path with NoUndetectableObjectsProtector intact.
assertTrue(eq(o1, o1));
assertFalse(eq(o1, o2));
assertFalse(eq(o1, null));
assertFalse(eq(null, o1));
assertFalse(eq(o1, undefined));
assertFalse(eq(undefined, o1));
assertTrue(eq(null, null));
assertTrue(eq(undefined, undefined));
assertTrue(eq(null, undefined));
assertTrue(eq(undefined, null));
assertOptimized(eq);

assertFalse(neq(o1, o1));
assertTrue(neq(o1, o2));
assertTrue(neq(o1, null));
assertTrue(neq(null, o1));
assertFalse(neq(null, undefined));
assertFalse(neq(undefined, null));
assertOptimized(neq);

assertTrue(strictEq(o1, o1));
assertFalse(strictEq(o1, o2));
assertFalse(strictEq(null, undefined));
assertTrue(strictEq(null, null));
assertTrue(strictEq(undefined, undefined));
assertOptimized(strictEq);

assertFalse(eqNullish(o1, 1));
assertTrue(eqNullish(null, 0));
assertTrue(eqNullish(undefined, 1));
assertOptimized(eqNullish);

assertTrue(eqBothNullish(1, 0));
assertOptimized(eqBothNullish);

assertTrue(eqDetectableObj(o1));
assertFalse(eqDetectableObj(o2));
assertFalse(eqDetectableObj(null));
assertOptimized(eqDetectableObj);

assertTrue(eqSelf(o1));
assertTrue(eqSelf(null));
assertOptimized(eqSelf);

assertFalse(eqKnownReceiver(o1, o2, null));
assertTrue(eqKnownReceiver(o1, o2, o1));
assertTrue(eqKnownReceiver(o1, o1, null));
assertOptimized(eqKnownReceiver);

// 2. Primitive input triggers eager deopt without corrupting result.
assertTrue(eqDeopt(o1, o1));
assertOptimized(eqDeopt);
assertFalse(eqDeopt(o1, 42));
assertUnoptimized(eqDeopt);
assertFalse(eqDeopt(42, o1));
assertTrue(eqDeopt(0, false));

// 3. Invalidate NoUndetectableObjectsProtector via %GetUndetectable().
const htmldda1 = %GetUndetectable();
const htmldda2 = %GetUndetectable();
const proxyDda = new Proxy(htmldda1, {});

// Functions that relied on the protector deopt cleanly, while static fast-path
// functions that did not need the protector stay optimized and accurate.
assertUnoptimized(eq);
assertUnoptimized(eqKnownReceiver);
assertOptimized(strictEq);
assertOptimized(eqBothNullish);
assertOptimized(eqDetectableObj);
assertOptimized(eqSelf);

assertTrue(eqKnownReceiver(htmldda1, o2, null));
assertFalse(eqKnownReceiver(htmldda1, htmldda2, o1));

assertTrue(eqNullish(htmldda1, 1));
assertTrue(eqNullish(htmldda1, 0));
assertFalse(eqNullish(proxyDda, 1));

assertFalse(eqDetectableObj(htmldda1));
assertTrue(eqDetectableObj(o1));
assertOptimized(eqDetectableObj);

assertTrue(eqSelf(htmldda1));
assertOptimized(eqSelf);

assertFalse(strictEq(htmldda1, null));
assertFalse(strictEq(htmldda1, undefined));
assertTrue(strictEq(htmldda1, htmldda1));
assertFalse(strictEq(htmldda1, htmldda2));
assertOptimized(strictEq);

// 4. Test optimized code compiled AFTER protector invalidation
// (!no_undetectable).
function eqPost(a, b) {
  return a == b;
}

function neqPost(a, b) {
  return a != b;
}

function eqConstDda(a) {
  return a == htmldda1;
}

function eqNullishPost(a, useNull) {
  const rhs = useNull ? null : undefined;
  return a == rhs;
}

const holeyDoubles = [1.5, , 2.5];
function eqHoleyDouble(arr, i, b) {
  return arr[i] == b;
}

%PrepareFunctionForOptimization(eqPost);
%PrepareFunctionForOptimization(neqPost);
%PrepareFunctionForOptimization(eqConstDda);
%PrepareFunctionForOptimization(eqNullishPost);
%PrepareFunctionForOptimization(eqHoleyDouble);

for (let i = 0; i < 10; i++) {
  assertTrue(eqPost(o1, o1));
  assertFalse(eqPost(o1, o2));
  assertFalse(eqPost(o1, null));
  assertTrue(eqPost(null, undefined));
  assertTrue(eqPost(htmldda1, null));
  assertTrue(eqPost(null, htmldda1));
  assertTrue(eqPost(htmldda1, undefined));
  assertTrue(eqPost(undefined, htmldda1));
  assertTrue(eqPost(htmldda1, htmldda1));
  assertFalse(eqPost(htmldda1, htmldda2));
  assertFalse(eqPost(htmldda1, o1));
  assertFalse(eqPost(o1, htmldda1));
  assertFalse(eqPost(proxyDda, null));

  assertFalse(neqPost(htmldda1, null));
  assertTrue(neqPost(htmldda1, htmldda2));

  assertTrue(eqConstDda(null));
  assertTrue(eqConstDda(undefined));
  assertTrue(eqConstDda(htmldda1));
  assertFalse(eqConstDda(htmldda2));
  assertFalse(eqConstDda(o1));

  assertTrue(eqNullishPost(htmldda1, true));
  assertFalse(eqNullishPost(o1, false));

  assertTrue(eqHoleyDouble(holeyDoubles, 1, null));
  assertFalse(eqHoleyDouble(holeyDoubles, 1, o1));
}

%OptimizeFunctionOnNextCall(eqPost);
%OptimizeFunctionOnNextCall(neqPost);
%OptimizeFunctionOnNextCall(eqConstDda);
%OptimizeFunctionOnNextCall(eqNullishPost);
%OptimizeFunctionOnNextCall(eqHoleyDouble);

assertTrue(eqPost(o1, o1));
assertFalse(eqPost(o1, o2));
assertFalse(eqPost(o1, null));
assertFalse(eqPost(null, o1));
assertFalse(eqPost(o1, undefined));
assertFalse(eqPost(undefined, o1));
assertTrue(eqPost(null, null));
assertTrue(eqPost(undefined, undefined));
assertTrue(eqPost(null, undefined));
assertTrue(eqPost(undefined, null));
assertTrue(eqPost(htmldda1, null));
assertTrue(eqPost(null, htmldda1));
assertTrue(eqPost(htmldda1, undefined));
assertTrue(eqPost(undefined, htmldda1));
assertTrue(eqPost(htmldda1, htmldda1));
assertFalse(eqPost(htmldda1, htmldda2));
assertFalse(eqPost(htmldda1, o1));
assertFalse(eqPost(o1, htmldda1));
assertFalse(eqPost(proxyDda, null));
assertFalse(eqPost(null, proxyDda));
assertTrue(eqPost(proxyDda, proxyDda));
assertOptimized(eqPost);

assertFalse(neqPost(htmldda1, null));
assertFalse(neqPost(undefined, htmldda1));
assertTrue(neqPost(htmldda1, htmldda2));
assertTrue(neqPost(htmldda1, o1));
assertOptimized(neqPost);

assertTrue(eqConstDda(null));
assertTrue(eqConstDda(undefined));
assertTrue(eqConstDda(htmldda1));
assertFalse(eqConstDda(htmldda2));
assertFalse(eqConstDda(o1));
assertOptimized(eqConstDda);

assertTrue(eqNullishPost(htmldda1, true));
assertTrue(eqNullishPost(htmldda1, false));
assertTrue(eqNullishPost(null, false));
assertFalse(eqNullishPost(o1, true));
assertFalse(eqNullishPost(proxyDda, false));
assertOptimized(eqNullishPost);

assertTrue(eqHoleyDouble(holeyDoubles, 1, null));
assertTrue(eqHoleyDouble(holeyDoubles, 1, htmldda1));
assertFalse(eqHoleyDouble(holeyDoubles, 1, o1));
assertOptimized(eqHoleyDouble);
// Non-hole double triggers eager deopt and falls back accurately.
assertFalse(eqHoleyDouble(holeyDoubles, 0, o1));
assertUnoptimized(eqHoleyDouble);
