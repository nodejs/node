// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  "use strict";
  function bar() { return this; }

  function foo(x) {
    return bar.bind(x);
  }

  assertEquals(0, foo(0)());
  assertEquals(1, foo(1)());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("", foo("")());
})();

(function() {
  "use strict";
  function bar(x) { return x; }

  function foo(x) {
    return bar.bind(undefined, x);
  }

  assertEquals(0, foo(0)());
  assertEquals(1, foo(1)());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("", foo("")());
})();

(function() {
  function bar(x) { return x; }

  function foo(x) {
    return bar.bind(undefined, x);
  }

  assertEquals(0, foo(0)());
  assertEquals(1, foo(1)());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("", foo("")());
})();

(function() {
  "use strict";
  function bar(x, y) { return x + y; }

  function foo(x, y) {
    return bar.bind(undefined, x, y);
  }

  assertEquals(0, foo(0, 0)());
  assertEquals(2, foo(1, 1)());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("ab", foo("a", "b")());
  assertEquals(0, foo(0, 1).length);
  assertEquals("bound bar", foo(1, 2).name)
})();

(function() {
  function bar(x, y) { return x + y; }

  function foo(x, y) {
    return bar.bind(undefined, x, y);
  }

  assertEquals(0, foo(0, 0)());
  assertEquals(2, foo(1, 1)());
  %OptimizeFunctionOnNextCall(foo);
  assertEquals("ab", foo("a", "b")());
  assertEquals(0, foo(0, 1).length);
  assertEquals("bound bar", foo(1, 2).name)
})();
