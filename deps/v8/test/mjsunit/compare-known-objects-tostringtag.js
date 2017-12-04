// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function le(a, b) {
  return a <= b;
}

function lt(a, b) {
  return a < b;
}

function ge(a, b) {
  return a >= b;
}

function gt(a, b) {
  return a > b;
}

function test(a, b) {
  // Check CompareIC for less than or equal of known objects.
  assertThrows(() => {le(a, a)});
  assertThrows(() => {le(a, b)});
  assertThrows(() => {le(b, a)});
  // Check CompareIC for less than of known objects.
  assertThrows(() => {lt(a, a)});
  assertThrows(() => {lt(a, b)});
  assertThrows(() => {lt(b, a)});
  // Check CompareIC for greater than or equal of known objects.
  assertThrows(() => {ge(a, a)});
  assertThrows(() => {ge(a, b)});
  assertThrows(() => {ge(b, a)});
  // Check CompareIC for greater than of known objects.
  assertThrows(() => {gt(a, a)});
  assertThrows(() => {gt(a, b)});
  assertThrows(() => {gt(b, a)});
}

function O() { }
Object.defineProperty(O.prototype, Symbol.toStringTag, {
  get() { throw "@@toStringTag called!" }
});

var obj1 = new O;
var obj2 = new O;

assertTrue(%HaveSameMap(obj1, obj2));
test(obj1, obj2);
test(obj1, obj2);
%OptimizeFunctionOnNextCall(le);
%OptimizeFunctionOnNextCall(lt);
%OptimizeFunctionOnNextCall(ge);
%OptimizeFunctionOnNextCall(gt);
test(obj1, obj2);
