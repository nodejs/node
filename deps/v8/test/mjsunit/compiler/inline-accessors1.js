// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  class O {
    get x() {
      return 1;
    }
  }
  var o = new O;

  function foo(o) {
    try {
      return o.x;
    } catch (e) {
      return 0;
    }
  }

  assertEquals(1, foo(o));
  assertEquals(1, foo(o));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(o));
})();

(function() {
  class O {
    get x() {
      %DeoptimizeFunction(foo);
      return 1;
    }
  }
  var o = new O;

  function foo(o) {
    try {
      return o.x;
    } catch (e) {
      return 0;
    }
  }

  assertEquals(1, foo(o));
  assertEquals(1, foo(o));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(o));
})();

(function() {
  function bar(x) {
    throw x;
  }

  class O {
    get x() {
      %DeoptimizeFunction(foo);
      return bar("x");
    }
  }
  var o = new O;

  function foo(o) {
    try {
      return o.x;
    } catch (e) {
      return 0;
    }
  }

  assertEquals(0, foo(o));
  assertEquals(0, foo(o));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(o));
})();
