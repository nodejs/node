// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  const m = new Map();
  const k = Math.pow(2, 31) - 1;
  m.set(k, 1);

  function foo(m, k) {
    return m.get(k | 0);
  }

  assertEquals(1, foo(m, k));
  assertEquals(1, foo(m, k));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(1, foo(m, k));
})();
