// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.findIndex().
(function() {
  function foo(a, o) {
    return a.findIndex(x => x === o.x);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo([1, 2, 3], {x:3}));
  assertEquals(-1, foo([0, 1, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo([1, 2, 3], {x:3}));
  assertEquals(-1, foo([0, 1, 2], {x:3}));

  // Non-extensible
  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(Object.preventExtensions([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.preventExtensions([0, 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(Object.preventExtensions([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.preventExtensions([0, 1, 2]), {x:3}));

  // Sealed
  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(Object.seal([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.seal([0, 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(Object.seal([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.seal([0, 1, 2]), {x:3}));

  // Frozen
  %PrepareFunctionForOptimization(foo);
  assertEquals(2, foo(Object.freeze([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.freeze([0, 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo(Object.freeze([1, 2, 3]), {x:3}));
  assertEquals(-1, foo(Object.freeze([0, 1, 2]), {x:3}));
})();
