// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const o1 = {x: 1};
const o2 = {x: 2};

function equals(a, b) {
  return a == b;
}

function eqNullish(a, which) {
  const rhs = which === 1 ? null : undefined;
  return a == rhs;
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

%PrepareFunctionForOptimization(equals);
%PrepareFunctionForOptimization(eqNullish);
%PrepareFunctionForOptimization(eqDetectableObj);
%PrepareFunctionForOptimization(eqSelf);
%PrepareFunctionForOptimization(eqKnownReceiver);

// Collect ReceiverOrNullOrUndefined feedback.
equals(o1, null);
eqNullish(o1, 1);
eqDetectableObj(null);
eqSelf(o1);
eqSelf(null);
eqKnownReceiver(o1, o2, null);

%OptimizeMaglevOnNextCall(equals);
%OptimizeMaglevOnNextCall(eqNullish);
%OptimizeMaglevOnNextCall(eqDetectableObj);
%OptimizeMaglevOnNextCall(eqSelf);
%OptimizeMaglevOnNextCall(eqKnownReceiver);

assertTrue(equals(null, null));
assertTrue(equals(null, undefined));
assertTrue(equals(undefined, null));
assertTrue(equals(undefined, undefined));
assertFalse(equals(null, o1));
assertFalse(equals(o1, null));
assertFalse(equals(undefined, o1));
assertFalse(equals(o1, undefined));
assertTrue(equals(o1, o1));
assertFalse(equals(o1, o2));
assertMaglevved(equals);

assertFalse(eqNullish(o1, 1));
assertTrue(eqNullish(null, 0));
assertMaglevved(eqNullish);

assertTrue(eqDetectableObj(o1));
assertFalse(eqDetectableObj(null));
assertMaglevved(eqDetectableObj);

assertTrue(eqSelf(o1));
assertTrue(eqSelf(null));
assertMaglevved(eqSelf);

assertFalse(eqKnownReceiver(o1, o2, null));
assertTrue(eqKnownReceiver(o1, o2, o1));
assertMaglevved(eqKnownReceiver);

// Invalidate NoUndetectableObjectsProtector.
const undetectable1 = %GetUndetectable();
const undetectable2 = %GetUndetectable();

assertNotMaglevved(equals);
assertNotMaglevved(eqKnownReceiver);
assertMaglevved(eqNullish);
assertMaglevved(eqDetectableObj);
assertMaglevved(eqSelf);

assertTrue(eqKnownReceiver(undetectable1, o2, null));
assertTrue(eqNullish(undetectable1, 1));
assertFalse(eqDetectableObj(undetectable1));
assertTrue(eqSelf(undetectable1));

// Re-optimize with NoUndetectableObjectsProtector invalidated.
function equalsPost(a, b) {
  return a == b;
}
function eqConstUndetectable(a) {
  return a == undetectable1;
}

%PrepareFunctionForOptimization(equalsPost);
%PrepareFunctionForOptimization(eqConstUndetectable);
equalsPost(o1, null);
eqConstUndetectable(null);

%OptimizeMaglevOnNextCall(equalsPost);
%OptimizeMaglevOnNextCall(eqConstUndetectable);

assertTrue(equalsPost(null, null));
assertTrue(equalsPost(null, undefined));
assertTrue(equalsPost(null, undetectable1));
assertTrue(equalsPost(undetectable1, null));
assertTrue(equalsPost(undefined, undetectable1));
assertTrue(equalsPost(undetectable1, undefined));
assertTrue(equalsPost(undetectable1, undetectable1));
assertFalse(equalsPost(undetectable1, undetectable2));
assertFalse(equalsPost(undetectable1, o1));
assertFalse(equalsPost(o1, undetectable1));
assertTrue(equalsPost(o1, o1));
assertFalse(equalsPost(o1, o2));
assertMaglevved(equalsPost);

assertTrue(eqConstUndetectable(null));
assertTrue(eqConstUndetectable(undefined));
assertTrue(eqConstUndetectable(undetectable1));
assertFalse(eqConstUndetectable(undetectable2));
assertFalse(eqConstUndetectable(o1));
assertMaglevved(eqConstUndetectable);

// Passing an unexpected primitive deopts.
assertFalse(equalsPost(o1, ""));
assertNotMaglevved(equalsPost);
