// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function testReflectGet2Args() {
  let obj = { a: 1 };
  let result = Reflect.get(obj, "a");
  return result;
}

%PrepareFunctionForOptimization(testReflectGet2Args);
assertEquals(1, testReflectGet2Args());
%OptimizeFunctionOnNextCall(testReflectGet2Args);
assertEquals(1, testReflectGet2Args());

let objWithGetter = {
  get a() { return this.x; },
  x: 1
};
let receiverWithX = { x: 2 };

function testReflectGet3ArgsWithGetter() {
  return Reflect.get(objWithGetter, "a", receiverWithX);
}

%PrepareFunctionForOptimization(testReflectGet3ArgsWithGetter);
assertEquals(2, testReflectGet3ArgsWithGetter());
%OptimizeFunctionOnNextCall(testReflectGet3ArgsWithGetter);
assertEquals(2, testReflectGet3ArgsWithGetter());

function testReflectGetPolymorphic(x) {
  return Reflect.get(x, "a");
}

let obj = { a: 1 };
%PrepareFunctionForOptimization(testReflectGetPolymorphic);
assertEquals(1, testReflectGetPolymorphic(obj));
%OptimizeFunctionOnNextCall(testReflectGetPolymorphic);
assertEquals(1, testReflectGetPolymorphic(obj));
assertThrows(() => testReflectGetPolymorphic(42), TypeError);
