// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo() {
    return Infinity / Math.max(-0, +0);
  }

  assertEquals(+Infinity, foo());
  assertEquals(+Infinity, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(+Infinity, foo());
})();

(function() {
  function foo() {
    return Infinity / Math.max(+0, -0);
  }

  assertEquals(+Infinity, foo());
  assertEquals(+Infinity, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(+Infinity, foo());
})();

(function() {
  function foo() {
    return Infinity / Math.min(-0, +0);
  }

  assertEquals(-Infinity, foo());
  assertEquals(-Infinity, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-Infinity, foo());
})();

(function() {
  function foo() {
    return Infinity / Math.min(+0, -0);
  }

  assertEquals(-Infinity, foo());
  assertEquals(-Infinity, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(-Infinity, foo());
})();
