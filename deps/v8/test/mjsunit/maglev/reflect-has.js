// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testReflectHasExisting() {
  let obj = { a: 1 };
  return Reflect.has(obj, "a");
}

%PrepareFunctionForOptimization(testReflectHasExisting);
assertTrue(testReflectHasExisting());
%OptimizeFunctionOnNextCall(testReflectHasExisting);
assertTrue(testReflectHasExisting());


function testReflectHasMissing() {
  let obj = { a: 1 };
  return Reflect.has(obj, "b");
}

%PrepareFunctionForOptimization(testReflectHasMissing);
assertFalse(testReflectHasMissing());
%OptimizeFunctionOnNextCall(testReflectHasMissing);
assertFalse(testReflectHasMissing());


function testReflectHasNonObject() {
  return Reflect.has(42, "a");
}

%PrepareFunctionForOptimization(testReflectHasNonObject);
assertThrows(() => testReflectHasNonObject(), TypeError);
%OptimizeFunctionOnNextCall(testReflectHasNonObject);
assertThrows(() => testReflectHasNonObject(), TypeError);


function testReflectHasPolymorphic(x) {
  return Reflect.has(x, "a");
}

let obj1 = { a: 1 };
let obj2 = { b: 2 }; // different map

%PrepareFunctionForOptimization(testReflectHasPolymorphic);
assertTrue(testReflectHasPolymorphic(obj1));
assertFalse(testReflectHasPolymorphic(obj2));

%OptimizeFunctionOnNextCall(testReflectHasPolymorphic);
assertTrue(testReflectHasPolymorphic(obj1));
assertFalse(testReflectHasPolymorphic(obj2));

assertThrows(() => testReflectHasPolymorphic(42), TypeError);
