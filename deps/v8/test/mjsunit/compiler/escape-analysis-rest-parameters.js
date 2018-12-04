// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test variable index access to rest parameters
// with up to 2 elements.
(function testRestParametersVariableIndex() {
  function g(...args) {
    let s = 0;
    for (let i = 0; i < args.length; ++i) s += args[i];
    return s;
  }

  function f(x, y) {
    // (a) args[i] is dead code since args.length is 0.
    const a = g();
    // (b) args[i] always yields the first element.
    const b = g(x);
    // (c) args[i] can yield either x or y.
    const c = g(x, y);
    return a + b + c;
  }

  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
})();
