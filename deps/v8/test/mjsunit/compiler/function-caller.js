// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestInlineAllocatedCaller() {
  function g() {
    var caller = g.caller;
    caller.foo = 23;
    assertEquals(23, caller.foo);
    assertEquals(23, g.caller.foo);
    assertSame(caller, g.caller);
  }
  %NeverOptimizeFunction(g);

  function f() {
    (function caller() { g() })();
  }

  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();
