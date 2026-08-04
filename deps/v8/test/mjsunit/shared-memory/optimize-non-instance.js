// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

function Foo() {}

function TestInstanceOfMutex() {
  return Foo instanceof Atomics.Mutex;
}
function TestInstanceOfCondition() {
  return Foo instanceof Atomics.Condition;
}
function TestInstanceOfSharedArray() {
  return Foo instanceof SharedArray;
}
function TestInstanceOfSharedStruct() {
  return Foo instanceof (new SharedStructType(["foo"]));
}

%PrepareFunctionForOptimization(TestInstanceOfMutex);
%PrepareFunctionForOptimization(TestInstanceOfCondition);
%PrepareFunctionForOptimization(TestInstanceOfSharedArray);
%PrepareFunctionForOptimization(TestInstanceOfSharedStruct);

for (let i = 0; i < 10; i++) {
  assertFalse(TestInstanceOfMutex());
  assertFalse(TestInstanceOfCondition());
  assertFalse(TestInstanceOfSharedArray());
  assertFalse(TestInstanceOfSharedStruct());
}

%OptimizeFunctionOnNextCall(TestInstanceOfMutex);
%OptimizeFunctionOnNextCall(TestInstanceOfCondition);
%OptimizeFunctionOnNextCall(TestInstanceOfSharedArray);
%OptimizeFunctionOnNextCall(TestInstanceOfSharedStruct);

assertFalse(TestInstanceOfMutex());
assertFalse(TestInstanceOfCondition());
assertFalse(TestInstanceOfSharedArray());
assertFalse(TestInstanceOfSharedStruct());
