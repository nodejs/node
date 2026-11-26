// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test cases for the `in` operator, shared by the Maglev and Turbolev tests.
// The includer defines optimizeOnNextCall and assertTierOptimized.

function hasMono(o) {
  return 'a' in o;
}

function hasNotFound(o) {
  return 'missing' in o;
}

function hasPoly(o) {
  return 'b' in o;
}

function hasProto(o) {
  return 'p' in o;
}

const proto = { p: 1 };
const o1 = Object.create(proto);
o1.a = 1;
const o2 = Object.create(proto);
o2.b = 2;
const o3 = Object.create(proto);
o3.c = 3;

%PrepareFunctionForOptimization(hasMono);
%PrepareFunctionForOptimization(hasNotFound);
%PrepareFunctionForOptimization(hasPoly);
%PrepareFunctionForOptimization(hasProto);

for (let i = 0; i < 10; i++) {
  assertTrue(hasMono(o1));
  assertFalse(hasNotFound(o1));
  assertFalse(hasPoly(o1));
  assertTrue(hasPoly(o2));
  assertFalse(hasPoly(o3));
  assertTrue(hasProto(o1));
  assertTrue(hasProto(o2));
}

optimizeOnNextCall(hasMono);
optimizeOnNextCall(hasNotFound);
optimizeOnNextCall(hasPoly);
optimizeOnNextCall(hasProto);

assertTrue(hasMono(o1));
assertTierOptimized(hasMono);

assertFalse(hasNotFound(o1));
assertTierOptimized(hasNotFound);

assertFalse(hasPoly(o1));
assertTrue(hasPoly(o2));
assertFalse(hasPoly(o3));
assertTierOptimized(hasPoly);

assertTrue(hasProto(o1));
assertTrue(hasProto(o2));
assertTierOptimized(hasProto);

// Mutating prototype invalidates prototype chain dependency.
delete proto.p;
assertFalse(hasProto(o1));
assertFalse(hasProto(o2));
assertUnoptimized(hasProto);

// Primitive receiver throws TypeError.
assertThrows(() => hasMono(42), TypeError);

// Accessor property folds to true without invoking the getter.
const withGetter = Object.defineProperty({}, 'acc', {
  get() { assertUnreachable(); },
});
function hasAccessor(o) {
  return 'acc' in o;
}
%PrepareFunctionForOptimization(hasAccessor);
for (let i = 0; i < 10; i++) {
  assertTrue(hasAccessor(withGetter));
}
optimizeOnNextCall(hasAccessor);
assertTrue(hasAccessor(withGetter));
assertTierOptimized(hasAccessor);

// Dynamic key checks expected name and deopts when key changes.
function hasDynamic(k, o) {
  return k in o;
}
%PrepareFunctionForOptimization(hasDynamic);
for (let i = 0; i < 10; i++) {
  assertTrue(hasDynamic('a', o1));
}
optimizeOnNextCall(hasDynamic);
assertTrue(hasDynamic('a', o1));
assertTierOptimized(hasDynamic);
assertFalse(hasDynamic('b', o1));
assertUnoptimized(hasDynamic);

// The values undefined and 'undefined' are the same property key.
const withUndefinedKey = { undefined: 1 };
function hasUndefinedKey(k, o) {
  return k in o;
}
%PrepareFunctionForOptimization(hasUndefinedKey);
for (let i = 0; i < 10; i++) {
  assertTrue(hasUndefinedKey(undefined, withUndefinedKey));
}
optimizeOnNextCall(hasUndefinedKey);
assertTrue(hasUndefinedKey(undefined, withUndefinedKey));
assertTrue(hasUndefinedKey('undefined', withUndefinedKey));
assertTierOptimized(hasUndefinedKey);
assertFalse(hasUndefinedKey('other', withUndefinedKey));
assertUnoptimized(hasUndefinedKey);

// Insufficient feedback deopts.
function hasNoFeedback(o, doTest) {
  if (doTest) return 'a' in o;
  return false;
}
%PrepareFunctionForOptimization(hasNoFeedback);
for (let i = 0; i < 10; i++) {
  assertFalse(hasNoFeedback(o1, false));
}
optimizeOnNextCall(hasNoFeedback);
assertFalse(hasNoFeedback(o1, false));
assertTierOptimized(hasNoFeedback);
assertTrue(hasNoFeedback(o1, true));
assertUnoptimized(hasNoFeedback);

// Megamorphic element feedback refined with a constant Name on a fresh map.
function hasMegamorphic(k, o) {
  return k in o;
}
%PrepareFunctionForOptimization(hasMegamorphic);
const megamorphicTargets = [
  [1, 2],
  [1.5, 2.5],
  { 0: 'x', a: 1 },
  Object.assign([1], { extra: 1 }),
  new Int32Array(2),
  { b: 2 },
];
for (const target of megamorphicTargets) {
  hasMegamorphic(0, target);
  hasMegamorphic('x', target);
}
function hasRefinedConstantName() {
  const fresh = { present: 1 };
  return hasMegamorphic('present', fresh) && !hasMegamorphic('absent', fresh);
}
%PrepareFunctionForOptimization(hasRefinedConstantName);
for (let i = 0; i < 10; i++) {
  assertTrue(hasRefinedConstantName());
}
optimizeOnNextCall(hasRefinedConstantName);
assertTrue(hasRefinedConstantName());
assertTierOptimized(hasRefinedConstantName);

// The absent name depends on the prototype chain of the fresh map.
Object.prototype.absent = 1;
assertFalse(hasRefinedConstantName());
assertUnoptimized(hasRefinedConstantName);
delete Object.prototype.absent;

// Megamorphic feedback without known maps calls the generic builtin.
function hasMegamorphicNamed(o) {
  return 'x' in o;
}
%PrepareFunctionForOptimization(hasMegamorphicNamed);
const megamorphicNamedTargets = [
  { x: 1 },
  { a: 1, x: 1 },
  { b: 1, x: 1 },
  { c: 1, x: 1 },
  { d: 1, x: 1 },
  { e: 1 },
];
for (const target of megamorphicNamedTargets) {
  hasMegamorphicNamed(target);
}
optimizeOnNextCall(hasMegamorphicNamed);
assertTrue(hasMegamorphicNamed({ f: 1, x: 1 }));
assertFalse(hasMegamorphicNamed({ g: 1 }));
assertTrue(hasMegamorphicNamed(Object.create({ x: 1 })));
assertTrue(hasMegamorphicNamed(new Proxy({}, { has: () => true })));
assertThrows(() => hasMegamorphicNamed(42), TypeError);
assertTierOptimized(hasMegamorphicNamed);
