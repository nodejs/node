// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Reflect.has with wrong (number of) arguments.
(function() {
  "use strict";
  function foo() { return Reflect.has(); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
})();
(function() {
  "use strict";
  function foo(o) { return Reflect.has(o); }

  %PrepareFunctionForOptimization(foo);
  assertFalse(foo({}));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo({}));
})();
(function() {
  "use strict";
  function foo(o) { return Reflect.has(o); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo.bind(undefined, 1));
  assertThrows(foo.bind(undefined, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo.bind(undefined, 'o'));
})();

// Test Reflect.has within try/catch.
(function() {
  "use strict";
  function foo() {
    try {
      return Reflect.has();
    } catch (e) {
      return 1;
    }
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo());
  assertEquals(1, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo());
})();
(function() {
  "use strict";
  const o = {};
  function foo(n) {
    try {
      return Reflect.has(o, n);
    } catch (e) {
      return 1;
    }
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(1, foo({[Symbol.toPrimitive]() { throw new Error(); }}));
  assertEquals(1, foo({[Symbol.toPrimitive]() { throw new Error(); }}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo({[Symbol.toPrimitive]() { throw new Error(); }}));
})();
