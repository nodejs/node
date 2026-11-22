// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-struct --allow-natives-syntax

function Foo() {}

function TestInstanceOfSharedArray() {
  return Foo instanceof SharedArray;
}
function TestInstanceOfSharedStruct() {
  return Foo instanceof (new SharedStructType(["foo"]));
}

%PrepareFunctionForOptimization(TestInstanceOfSharedArray);
%PrepareFunctionForOptimization(TestInstanceOfSharedStruct);

for (let i = 0; i < 10; i++) {
  assertFalse(TestInstanceOfSharedArray());
  assertFalse(TestInstanceOfSharedStruct());
}

%OptimizeFunctionOnNextCall(TestInstanceOfSharedArray);
%OptimizeFunctionOnNextCall(TestInstanceOfSharedStruct);

assertFalse(TestInstanceOfSharedArray());
assertFalse(TestInstanceOfSharedStruct());
