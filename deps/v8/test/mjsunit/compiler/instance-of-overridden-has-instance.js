// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function NonConstHasInstance() {
  var C = {
    [Symbol.hasInstance] : () => true
  };

  function f() {
    return {} instanceof C;
  }

  %PrepareFunctionForOptimization(f);
  assertTrue(f());
  assertTrue(f());
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
  C[Symbol.hasInstance] = () => false;
  assertFalse(f());
})();

(function NumberHasInstance() {
  var C = {
    [Symbol.hasInstance] : 0.1
  };

  function f(b, C) {
    if (b) return {} instanceof C;
    return false;
  }

  function g(b) {
    return f(b, C);
  }

  %PrepareFunctionForOptimization(g);
  assertFalse(f(true, Number));
  assertFalse(f(true, Number));
  assertFalse(g(false));
  assertFalse(g(false));
  %OptimizeFunctionOnNextCall(g);
  assertThrows(() => g(true));
})();

(function NonFunctionHasInstance() {
  var C = {
    [Symbol.hasInstance] : {}
  };

  function f(b, C) {
    if (b) return {} instanceof C;
    return false;
  }

  function g(b) {
    return f(b, C);
  }

  %PrepareFunctionForOptimization(g);
  assertFalse(f(true, Number));
  assertFalse(f(true, Number));
  assertFalse(g(false));
  assertFalse(g(false));
  %OptimizeFunctionOnNextCall(g);
  assertThrows(() => g(true));
})();

(function NonConstHasInstanceProto() {
  var B = {
    [Symbol.hasInstance]() { return true; }
  };

  var C = { __proto__ : B };

  function f() {
    return {} instanceof C;
  }

  %PrepareFunctionForOptimization(f);
  assertTrue(f());
  assertTrue(f());
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
  B[Symbol.hasInstance] = () => { return false; };
  assertFalse(f());
})();

(function HasInstanceOverwriteOnProto() {
  var A = {
    [Symbol.hasInstance] : () => false
  }

  var B = { __proto__ : A };

  var C = { __proto__ : B };

  function f() {
    return {} instanceof C;
  }

  %PrepareFunctionForOptimization(f);
  assertFalse(f());
  assertFalse(f());
  %OptimizeFunctionOnNextCall(f);
  assertFalse(f());
  B[Symbol.hasInstance] = () => { return true; };
  assertTrue(f());
})();
