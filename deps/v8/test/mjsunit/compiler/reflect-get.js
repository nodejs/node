// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Reflect.get with wrong (number of) arguments.
(function() {
  "use strict";
  function foo() { return Reflect.get(); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
})();
(function() {
  "use strict";
  function foo(o) { return Reflect.get(o); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(undefined, foo({}));
  assertEquals(undefined, foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo({}));
})();
(function() {
  "use strict";
  function foo(o) { return Reflect.get(o); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo.bind(undefined, 1));
  assertThrows(foo.bind(undefined, undefined));
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo.bind(undefined, 'o'));
})();

// Test Reflect.get within try/catch.
(function() {
  const o = {x: 10};
  "use strict";
  function foo() {
    try {
      return Reflect.get(o, "x");
    } catch (e) {
      return 1;
    }
  }

  %PrepareFunctionForOptimization(foo);
  assertEquals(10, foo());
  assertEquals(10, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(10, foo());
})();
(function() {
  "use strict";
  const o = {};
  function foo(n) {
    try {
      return Reflect.get(o, n);
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
