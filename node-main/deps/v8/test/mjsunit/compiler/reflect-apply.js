// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Reflect.apply with wrong number of arguments.
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return Reflect.apply(bar); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
})();
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return Reflect.apply(bar, this); }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo);
  assertThrows(foo);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo);
})();
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return Reflect.apply(bar, this, arguments, this); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo.call(42));
  assertEquals(42, foo.call(42));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo.call(42));
})();

// Test Reflect.apply within try/catch.
(function() {
  "use strict";
  function foo(bar) {
    try {
      return Reflect.apply(bar, bar, arguments);
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
  function foo(bar) {
    try {
      return Reflect.apply(bar, bar, bar);
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

// Test proper order of callable check and array-like iteration
// in Reflect.apply.
(function() {
  var dummy_length_counter = 0;
  var dummy = { get length() { ++dummy_length_counter; return 0; } };

  function foo() {
    return Reflect.apply(undefined, this, dummy);
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, TypeError);
  assertThrows(foo, TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo, TypeError);
  assertEquals(0, dummy_length_counter);
})();
(function() {
  var dummy_length_counter = 0;
  var dummy = { get length() { ++dummy_length_counter; return 0; } };

  function foo() {
    return Reflect.apply(null, this, dummy);
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, TypeError);
  assertThrows(foo, TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo, TypeError);
  assertEquals(0, dummy_length_counter);
})();
(function() {
  var dummy_length_counter = 0;
  var dummy = { get length() { ++dummy_length_counter; return 0; } };

  function foo() {
    return Reflect.apply(null, this, dummy);
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, TypeError);
  assertThrows(foo, TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo, TypeError);
  assertEquals(0, dummy_length_counter);
})();
