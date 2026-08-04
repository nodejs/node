// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test Function.prototype.apply with null/undefined argumentsList
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return bar.apply(this, null); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo.call(42));
  assertEquals(42, foo.call(42));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo.call(42));
})();
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return bar.apply(this, undefined); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo.call(42));
  assertEquals(42, foo.call(42));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo.call(42));
})();

// Test Function.prototype.apply within try/catch.
(function() {
  "use strict";
  function foo(bar) {
    try {
      return Function.prototype.apply.call(bar, bar, arguments);
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
      return Function.prototype.apply.call(bar, bar, bar);
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

// Test Function.prototype.apply with wrong number of arguments.
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return bar.apply(); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(undefined, foo());
  assertEquals(undefined, foo());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(undefined, foo());
})();
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return bar.apply(this); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo.call(42));
  assertEquals(42, foo.call(42));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo.call(42));
})();
(function() {
  "use strict";
  function bar() { return this; }
  function foo() { return bar.apply(this, arguments, this); }

  %PrepareFunctionForOptimization(foo);
  assertEquals(42, foo.call(42));
  assertEquals(42, foo.call(42));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(42, foo.call(42));
})();

// Test proper order of callable check and array-like iteration
// in Function.prototype.apply.
(function() {
  var dummy_length_counter = 0;
  var dummy = { get length() { ++dummy_length_counter; return 0; } };

  function foo() {
    return Function.prototype.apply.call(undefined, this, dummy);
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
    return Function.prototype.apply.call(null, this, dummy);
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
    return Function.prototype.apply.call(null, this, dummy);
  }

  %PrepareFunctionForOptimization(foo);
  assertThrows(foo, TypeError);
  assertThrows(foo, TypeError);
  %OptimizeFunctionOnNextCall(foo);
  assertThrows(foo, TypeError);
  assertEquals(0, dummy_length_counter);
})();
