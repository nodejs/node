// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.find().
(function() {
  function foo(a, o) {
    return a.find(x => x === o.x);
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo([1, 2, 3], {x:3}));
  assertEquals(undefined, foo([0, 1, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2, 3], {x:3}));
  assertEquals(undefined, foo([0, 1, 2], {x:3}));

  // Packed
  // Non-extensible
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.preventExtensions(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.preventExtensions(['0', 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.preventExtensions(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.preventExtensions(['0', 1, 2]), {x:3}));

  // Sealed
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.seal(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.seal(['0', 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.seal(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.seal(['0', 1, 2]), {x:3}));

  // Frozen
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.freeze(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.freeze(['0', 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.freeze(['1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.freeze(['0', 1, 2]), {x:3}));

  // Holey
  // Non-extensible
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.preventExtensions([, '1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.preventExtensions([, '0', 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.preventExtensions([, '1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.preventExtensions([, '0', 1, 2]), {x:3}));

  // Sealed
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.seal([, '1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.seal([, '0', 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.seal([, '1', 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.seal([, '0', 1, 2]), {x:3}));

  // Frozen
  %PrepareFunctionForOptimization(foo);
  assertEquals(3, foo(Object.freeze([, 1, 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.freeze([, 0, 1, 2]), {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo(Object.freeze([, 1, 2, 3]), {x:3}));
  assertEquals(undefined, foo(Object.freeze([, 0, 1, 2]), {x:3}));
})();
