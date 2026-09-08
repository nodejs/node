// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Test 1: Spreading new Array() (empty capacity 4, length 0) and new Array(2)
// (holey).
(function() {
function target(...rest) {
  return rest.length;
}
function f_empty() {
  const a = new Array();
  return target(...a);
}
function f_holey() {
  const a = new Array(2);
  return target(...a);
}

%PrepareFunctionForOptimization(target);
for (let [f, expected] of [[f_empty, 0], [f_holey, 2]]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f());
  assertOptimized(f);
}
})();

// Test 2: Spreading holey array literals (double InlinedAllocation & tagged COW
// FixedArray).
(function() {
function target(a, b, c) {
  return '' + a + '|' + b + '|' + c;
}
function f_double() {
  return target(...[1.5, , 2.5]);
}
function f_tagged() {
  return target(...['x', , 'z']);
}

%PrepareFunctionForOptimization(target);
for (let [f, expected] of [
         [f_double, '1.5|undefined|2.5'], [f_tagged, 'x|undefined|z']]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f());
  assertOptimized(f);
}
})();

// Test 3: Spreading rest parameters and arguments objects (emits
// CallForwardVarargs).
(function() {
function outer(a, b, c) {
  return '' + a + '|' + b + '|' + c;
}
function mid_rest(...rest) {
  return outer(...rest);
}
function mid_args() {
  return outer(...arguments);
}

%PrepareFunctionForOptimization(outer);
for (let f of [mid_rest, mid_args]) {
  %PrepareFunctionForOptimization(f);
  assertEquals('1|2|3', f(1, 2, 3));
  assertEquals('1|2|3', f(1, 2, 3));
  %OptimizeMaglevOnNextCall(f);
  assertEquals('1|2|3', f(1, 2, 3));
  assertOptimized(f);
}
})();

// Test 4: Construct with spread over empty, holey, and tagged arrays.
(function() {
class Target {
  constructor(a, b, c) {
    this.res = '' + a + '|' + b + '|' + c;
  }
}
function f_empty() {
  return new Target(...new Array());
}
function f_double() {
  return new Target(...[1.5, , 2.5]);
}
function f_tagged() {
  return new Target(...['x', , 'z']);
}
function f_holey() {
  return new Target(...new Array(2));
}

%PrepareFunctionForOptimization(Target);
for (let [f, expected] of [
         [f_empty, 'undefined|undefined|undefined'],
         [f_double, '1.5|undefined|2.5'],
         [f_tagged, 'x|undefined|z'],
         [f_holey, 'undefined|undefined|undefined'],
]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f().res);
  assertEquals(expected, f().res);
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f().res);
  assertOptimized(f);
}
})();

// Test 5: Call with array-like (apply) over packed and holey arrays.
(function() {
function target(a, b, c) {
  return '' + a + '|' + b + '|' + c;
}
function f_packed() {
  return target.apply(null, [10, 20, 30]);
}
function f_double() {
  return target.apply(null, [1.5, , 2.5]);
}
function f_tagged() {
  return target.apply(null, ['x', , 'z']);
}
function f_holey() {
  return target.apply(null, new Array(2));
}

%PrepareFunctionForOptimization(target);
for (let [f, expected] of [
         [f_packed, '10|20|30'],
         [f_double, '1.5|undefined|2.5'],
         [f_tagged, 'x|undefined|z'],
         [f_holey, 'undefined|undefined|undefined'],
]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f());
  assertOptimized(f);
}
})();

// Test 6: Arity threshold (32 unpacks & inlines, 33 bails to generic call).
(function() {
function target(...rest) {
  return rest.length;
}

function spread32() {
  return target(
      ...[0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
          16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31]);
}
function spread33() {
  return target(
      ...[0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
          17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32]);
}
function apply32() {
  return target.apply(
      null, [
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
      ]);
}
function apply33() {
  return target.apply(
      null, [
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      ]);
}

%PrepareFunctionForOptimization(target);
for (let [f, expected] of [
         [spread32, 32],
         [spread33, 33],
         [apply32, 32],
         [apply33, 33],
]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f());
  assertOptimized(f);
}
})();

// Test 7: Spreading new Array(undefined, numbers...) (holey double elements).
(function() {
function target(a, b, c) {
  return '' + a + '|' + b + '|' + c;
}
class Target {
  constructor(a, b, c) {
    this.res = '' + a + '|' + b + '|' + c;
  }
}

function f_spread() {
  const a = new Array(undefined, 1.5, 2.5);
  return target(...a);
}
function f_apply() {
  const a = new Array(undefined, 1.5, 2.5);
  return target.apply(null, a);
}
function f_construct() {
  const a = new Array(undefined, 1.5, 2.5);
  return new Target(...a);
}
function f_reflect() {
  const a = new Array(undefined, 1.5, 2.5);
  return Reflect.apply(target, null, a);
}
function f_deopt() {
  const a = new Array(undefined, 1.5);
  return (function(x) { return [x].join(','); })(...a);
}

%PrepareFunctionForOptimization(target);
%PrepareFunctionForOptimization(Target);
for (let [f, expected] of [
         [f_spread, 'undefined|1.5|2.5'],
         [f_apply, 'undefined|1.5|2.5'],
         [f_reflect, 'undefined|1.5|2.5'],
]) {
  %PrepareFunctionForOptimization(f);
  assertEquals(expected, f());
  assertEquals(expected, f());
  %OptimizeMaglevOnNextCall(f);
  assertEquals(expected, f());
  assertOptimized(f);
}

%PrepareFunctionForOptimization(f_construct);
assertEquals('undefined|1.5|2.5', f_construct().res);
assertEquals('undefined|1.5|2.5', f_construct().res);
%OptimizeMaglevOnNextCall(f_construct);
assertEquals('undefined|1.5|2.5', f_construct().res);
assertOptimized(f_construct);

%PrepareFunctionForOptimization(f_deopt);
assertEquals('', f_deopt());
assertEquals('', f_deopt());
%OptimizeMaglevOnNextCall(f_deopt);
assertEquals('', f_deopt());
assertOptimized(f_deopt);
})();
