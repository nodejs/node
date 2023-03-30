// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

function Foo() {}

function TestInstanceOfMutex() {
  Foo instanceof Atomics.Mutex;
}
function TestInstanceOfCondition() {
  Foo instanceof Atomics.Condition;
}
function TestInstanceOfSharedArray() {
  Foo instanceof SharedArray;
}
function TestInstanceOfSharedStruct() {
  Foo instanceof (new SharedStructType(["foo"]));
}

%PrepareFunctionForOptimization(TestInstanceOfMutex);
%PrepareFunctionForOptimization(TestInstanceOfCondition);
%PrepareFunctionForOptimization(TestInstanceOfSharedArray);
%PrepareFunctionForOptimization(TestInstanceOfSharedStruct);

for (let i = 0; i < 10; i++) {
  assertThrows(TestInstanceOfMutex, TypeError);
  assertThrows(TestInstanceOfCondition, TypeError);
  assertThrows(TestInstanceOfSharedArray, TypeError);
  assertThrows(TestInstanceOfSharedStruct, TypeError);
}

%OptimizeFunctionOnNextCall(TestInstanceOfMutex);
%OptimizeFunctionOnNextCall(TestInstanceOfCondition);
%OptimizeFunctionOnNextCall(TestInstanceOfSharedArray);
%OptimizeFunctionOnNextCall(TestInstanceOfSharedStruct);

assertThrows(TestInstanceOfMutex, TypeError);
assertThrows(TestInstanceOfCondition, TypeError);
assertThrows(TestInstanceOfSharedArray, TypeError);
assertThrows(TestInstanceOfSharedStruct, TypeError);
