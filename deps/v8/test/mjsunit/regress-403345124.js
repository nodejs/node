// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var fail;
  let count = 0;
  function bar () {
    if (count > 50) return undefined;
  }

  function soft() {
    try { fail(); } catch (e) {}
  }

  function foo(x) {
    if (x < 3) {
      bar();
      soft();
      if (x == 42) {
      }
    }
  }

  %PrepareFunctionForOptimization(soft);
  %PrepareFunctionForOptimization(foo);
  foo(0);
  foo(0);
  // This will force {bar} to inline and
  // soft deopt due to lack of comparison
  // feedback.
  %PrepareFunctionForOptimization(bar);
  %OptimizeFunctionOnNextCall(foo);
  foo();
})();

(function() {
  function testclz(x) {
    for (var i = 0; i < 33; i++) {
      if (x & 0x80000000) return i;
      x <<= 1;
    }
    return 32;
  }

  function f() {
    assertEquals(testclz(0), 32);
  }

  %PrepareFunctionForOptimization(assertEquals);
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeMaglevOnNextCall(f);
  f();
})();
