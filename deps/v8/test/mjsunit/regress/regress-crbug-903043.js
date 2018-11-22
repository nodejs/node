// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function foo() {
    const x = 1e-1;
    return Object.is(-0, x * (-1e-308));
  }

  assertFalse(foo());
  assertFalse(foo());
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo());
})();

(function() {
  function foo(x) {
    return Object.is(-0, x * (-1e-308));
  }

  assertFalse(foo(1e-1));
  assertFalse(foo(1e-1));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(1e-1));
})();

(function() {
  function foo(x) {
    return Object.is(-0, x);
  }

  assertFalse(foo(1e-1 * (-1e-308)));
  assertFalse(foo(1e-1 * (-1e-308)));
  %OptimizeFunctionOnNextCall(foo);
  assertFalse(foo(1e-1 * (-1e-308)));
})();
