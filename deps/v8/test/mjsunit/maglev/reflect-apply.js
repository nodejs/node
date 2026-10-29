// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testReflectApplyBasic() {
  function f(x, y) { return x + y; }
  return Reflect.apply(f, null, [1, 2]);
}

%PrepareFunctionForOptimization(testReflectApplyBasic);
assertEquals(3, testReflectApplyBasic());
%OptimizeFunctionOnNextCall(testReflectApplyBasic);
assertEquals(3, testReflectApplyBasic());


function testReflectApplyArguments() {
  function f(x, y) { return x + y; }
  function g() {
    return Reflect.apply(f, null, arguments);
  }
  return g(10, 20);
}

%PrepareFunctionForOptimization(testReflectApplyArguments);
assertEquals(30, testReflectApplyArguments());
%OptimizeFunctionOnNextCall(testReflectApplyArguments);
assertEquals(30, testReflectApplyArguments());


function testReflectApplyExtraArgs() {
  function f(x, y) { return x + y; }
  return Reflect.apply(f, null, [1, 2], "extra");
}

%PrepareFunctionForOptimization(testReflectApplyExtraArgs);
assertEquals(3, testReflectApplyExtraArgs());
%OptimizeFunctionOnNextCall(testReflectApplyExtraArgs);
assertEquals(3, testReflectApplyExtraArgs());


function testReflectApplyMissingArgs() {
  function f() { return arguments.length; }
  return Reflect.apply(f); // should pad with undefined, so target=undefined, this=undefined, list=undefined
}

%PrepareFunctionForOptimization(testReflectApplyMissingArgs);
assertThrows(() => testReflectApplyMissingArgs(), TypeError); // undefined is not a function
%OptimizeFunctionOnNextCall(testReflectApplyMissingArgs);
assertThrows(() => testReflectApplyMissingArgs(), TypeError);


function testReflectApplyNonCallable() {
  return Reflect.apply(42, null, []);
}

%PrepareFunctionForOptimization(testReflectApplyNonCallable);
assertThrows(() => testReflectApplyNonCallable(), TypeError);
%OptimizeFunctionOnNextCall(testReflectApplyNonCallable);
assertThrows(() => testReflectApplyNonCallable(), TypeError);


function testReflectApplyNonArrayLike() {
  function f() {}
  return Reflect.apply(f, null, 42);
}

%PrepareFunctionForOptimization(testReflectApplyNonArrayLike);
assertThrows(() => testReflectApplyNonArrayLike(), TypeError);
%OptimizeFunctionOnNextCall(testReflectApplyNonArrayLike);
assertThrows(() => testReflectApplyNonArrayLike(), TypeError);
