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
  assertThrows(function() {le(a, a)});
  assertThrows(function() {le(a, b)});
  assertThrows(function() {le(b, a)});
  // Check CompareIC for less than of known objects.
  assertThrows(function() {lt(a, a)});
  assertThrows(function() {lt(a, b)});
  assertThrows(function() {lt(b, a)});
  // Check CompareIC for greater than or equal of known objects.
  assertThrows(function() {ge(a, a)});
  assertThrows(function() {ge(a, b)});
  assertThrows(function() {ge(b, a)});
  // Check CompareIC for greater than of known objects.
  assertThrows(function() {gt(a, a)});
  assertThrows(function() {gt(a, b)});
  assertThrows(function() {gt(b, a)});
}

function O() { }
Object.defineProperty(O.prototype, Symbol.toStringTag, {
  get: function() { throw "@@toStringTag called!" }
});

var obj1 = new O;
var obj2 = new O;

%PrepareFunctionForOptimization(le);
%PrepareFunctionForOptimization(lt);
%PrepareFunctionForOptimization(ge);
%PrepareFunctionForOptimization(gt);
assertTrue(%HaveSameMap(obj1, obj2));
test(obj1, obj2);
test(obj1, obj2);
%OptimizeFunctionOnNextCall(le);
%OptimizeFunctionOnNextCall(lt);
%OptimizeFunctionOnNextCall(ge);
%OptimizeFunctionOnNextCall(gt);
test(obj1, obj2);
