// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

"use strict";
const mathAbs = Math.abs;
const mathImul = Math.imul;


// Testing: FunctionPrototypeApply
function TestFunctionPrototypeApplyHelper() {
  return mathAbs.apply(undefined, arguments);
}

function TestFunctionPrototypeApply(x) {
  return TestFunctionPrototypeApplyHelper(x);
}

%PrepareFunctionForOptimization(TestFunctionPrototypeApplyHelper);
%PrepareFunctionForOptimization(TestFunctionPrototypeApply);
assertEquals(TestFunctionPrototypeApply(-13), 13);
assertEquals(TestFunctionPrototypeApply(42), 42);
%OptimizeFunctionOnNextCall(TestFunctionPrototypeApply);
assertEquals(TestFunctionPrototypeApply(-13), 13);
assertOptimized(TestFunctionPrototypeApply);
TestFunctionPrototypeApply("abc");
assertUnoptimized(TestFunctionPrototypeApply);


// Testing: FunctionPrototypeCall
function TestFunctionPrototypeCall(x) {
  return mathAbs.call(undefined, x);
}

%PrepareFunctionForOptimization(TestFunctionPrototypeCall);
TestFunctionPrototypeCall(42);
TestFunctionPrototypeCall(52);
%OptimizeFunctionOnNextCall(TestFunctionPrototypeCall);
TestFunctionPrototypeCall(12);
assertOptimized(TestFunctionPrototypeCall);
TestFunctionPrototypeCall("abc");
assertUnoptimized(TestFunctionPrototypeCall);


// Testing: ArrayForEach
function TestArrayForEach(x) {
  x.forEach(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayForEach);
TestArrayForEach([1, 3, -4]);
TestArrayForEach([-9, 9, 0]);
%OptimizeFunctionOnNextCall(TestArrayForEach);
TestArrayForEach([1, 3, -4]);
assertOptimized(TestArrayForEach);
TestArrayForEach(["abc", "xy"]);
assertUnoptimized(TestArrayForEach);


// Testing: ArrayReduce
function TestArrayReduce(x) {
  return x.reduce(mathImul);
}

%PrepareFunctionForOptimization(TestArrayReduce);
assertEquals(TestArrayReduce([1, 2, -3, 4]), -24);
assertEquals(TestArrayReduce([3, 5, 7]), 105);
%OptimizeFunctionOnNextCall(TestArrayReduce);
assertEquals(TestArrayReduce([1, 2, -3, 4]), -24);
assertOptimized(TestArrayReduce);
TestArrayReduce(["abc", "xy"]);
assertUnoptimized(TestArrayReduce);


// Testing: ArrayReduceRight
function TestArrayReduceRight(x) {
  return x.reduceRight(mathImul);
}

%PrepareFunctionForOptimization(TestArrayReduceRight);
assertEquals(TestArrayReduceRight([1, 2, -3, 4]), -24);
assertEquals(TestArrayReduceRight([3, 5, 7]), 105);
%OptimizeFunctionOnNextCall(TestArrayReduceRight);
assertEquals(TestArrayReduceRight([1, 2, -3, 4]), -24);
assertOptimized(TestArrayReduceRight);
TestArrayReduceRight(["abc", "xy"]);
assertUnoptimized(TestArrayReduceRight);


// Testing: ArrayMap
function TestArrayMap(x) {
  return x.map(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayMap);
assertEquals(TestArrayMap([1, -2, -3, 4]), [1, 2, 3, 4]);
assertEquals(TestArrayMap([5, -5, 5, -5]), [5, 5, 5, 5]);
%OptimizeFunctionOnNextCall(TestArrayMap);
assertEquals(TestArrayMap([1, -2, 3, -4]), [1, 2, 3, 4]);
assertOptimized(TestArrayMap);
TestArrayMap(["abc", "xy"]);
assertUnoptimized(TestArrayMap);


// Testing: ArrayFilter
function TestArrayFilter(x) {
  return x.filter(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayFilter);
assertEquals(TestArrayFilter([-2, 0, 3, -4]), [-2, 3, -4]);
assertEquals(TestArrayFilter([0, 1, 1, 0]), [1, 1]);
%OptimizeFunctionOnNextCall(TestArrayFilter);
assertEquals(TestArrayFilter([-2, 0, 3, -4]), [-2, 3, -4]);
assertOptimized(TestArrayFilter);
TestArrayFilter(["abc", "xy"]);
assertUnoptimized(TestArrayFilter);


// Testing: ArrayFind
function TestArrayFind(x) {
  return x.find(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayFind);
assertEquals(TestArrayFind([0, 0, -3, 12]), -3);
assertEquals(TestArrayFind([0, -18]), -18);
%OptimizeFunctionOnNextCall(TestArrayFind);
assertEquals(TestArrayFind([0, 0, -3, 12]), -3);
assertOptimized(TestArrayFind);
TestArrayFind(["", "abc", "xy"]);
assertUnoptimized(TestArrayFind);


// Testing: ArrayFindIndex
function TestArrayFindIndex(x) {
  return x.findIndex(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayFindIndex);
assertEquals(TestArrayFindIndex([0, 0, -3, 12]), 2);
assertEquals(TestArrayFindIndex([0, -18]), 1);
%OptimizeFunctionOnNextCall(TestArrayFindIndex);
assertEquals(TestArrayFindIndex([0, 0, -3, 12]), 2);
assertOptimized(TestArrayFindIndex);
TestArrayFindIndex(["", "abc", "xy"]);
assertUnoptimized(TestArrayFindIndex);


// Testing: ArrayEvery
function TestArrayEvery(x) {
  return x.every(mathAbs);
}

%PrepareFunctionForOptimization(TestArrayEvery);
assertEquals(TestArrayEvery([3, 0, -9]), false);
assertEquals(TestArrayEvery([2, 12, -1]), true);
%OptimizeFunctionOnNextCall(TestArrayEvery);
assertEquals(TestArrayEvery([3, 0, -9]), false);
assertOptimized(TestArrayEvery);
TestArrayEvery(["abc", "xy"]);
assertUnoptimized(TestArrayEvery);


// Testing: ArraySome
function TestArraySome(x) {
  return x.some(mathAbs);
}

%PrepareFunctionForOptimization(TestArraySome);
assertEquals(TestArraySome([3, 0, -9]), true);
assertEquals(TestArraySome([0, 0]), false);
%OptimizeFunctionOnNextCall(TestArraySome);
assertEquals(TestArraySome([3, 0, -9]), true);
assertOptimized(TestArraySome);
TestArraySome(["abc", "xy"]);
assertUnoptimized(TestArraySome);


// Testing: JSCall (JSFunction)
const boundMathImul = mathImul.bind(undefined, -3);
function TestJSCallWithJSFunction(x) {
  return boundMathImul(x);
}

%PrepareFunctionForOptimization(TestJSCallWithJSFunction);
assertEquals(TestJSCallWithJSFunction(-14), 42);
assertEquals(TestJSCallWithJSFunction(14), -42);
%OptimizeFunctionOnNextCall(TestJSCallWithJSFunction);
assertEquals(TestJSCallWithJSFunction(-14), 42);
assertOptimized(TestJSCallWithJSFunction);
TestJSCallWithJSFunction("abc");
assertUnoptimized(TestJSCallWithJSFunction);


// Testing: JSCall (JSBoundFunction)
function TestJSCallWithJSBoundFunction(x) {
  return mathImul.bind(undefined, -3)(x);
}

%PrepareFunctionForOptimization(TestJSCallWithJSBoundFunction);
assertEquals(TestJSCallWithJSBoundFunction(-14), 42);
assertEquals(TestJSCallWithJSBoundFunction(14), -42);
%OptimizeFunctionOnNextCall(TestJSCallWithJSBoundFunction);
assertEquals(TestJSCallWithJSBoundFunction(-14), 42);
assertOptimized(TestJSCallWithJSBoundFunction);
TestJSCallWithJSBoundFunction("abc");
assertUnoptimized(TestJSCallWithJSBoundFunction);


// Testing: ReflectApply
function TestReflectApplyHelper() {
  return Reflect.apply(mathAbs, undefined, arguments);
}

function TestReflectApply(x) {
  return TestReflectApplyHelper(x);
}

%PrepareFunctionForOptimization(TestReflectApplyHelper);
%PrepareFunctionForOptimization(TestReflectApply);
assertEquals(TestReflectApply(-9), 9);
assertEquals(TestReflectApply(7), 7);
%OptimizeFunctionOnNextCall(TestReflectApply);
assertEquals(TestReflectApply(-9), 9);
assertOptimized(TestReflectApply);
TestReflectApply("abc");
assertUnoptimized(TestReflectApply);


// Testing: CallWithSpread
function TestCallWithSpreadHelper() {
  return mathImul(...arguments);
}

function TestCallWithSpread(x) {
  return TestCallWithSpreadHelper(x, x);
}

%PrepareFunctionForOptimization(TestCallWithSpreadHelper);
%PrepareFunctionForOptimization(TestCallWithSpread);
assertEquals(TestCallWithSpread(-13), 169);
assertEquals(TestCallWithSpread(7), 49);
%OptimizeFunctionOnNextCall(TestCallWithSpread);
assertEquals(TestCallWithSpread(-13), 169);
assertOptimized(TestCallWithSpread);
TestCallWithSpread("abc");
assertUnoptimized(TestCallWithSpread);
