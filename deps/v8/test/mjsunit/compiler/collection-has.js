// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  const map = new Map();
  map.set(true, true);

  for (let i = 0; i < 10000; i += 1) {
    map.set(i, i);
    map.set(`${i} number`, i);
  }

  function foo(x) {
    return map.has(x);
  }

  %PrepareFunctionForOptimization(foo);

  assertFalse(foo(1.5));
  assertTrue(foo(1));

  assertFalse(foo('1.5 number'));
  assertTrue(foo('1 number'));

  assertFalse(foo(false));
  assertTrue(foo(true));

  %OptimizeFunctionOnNextCall(foo);

  assertFalse(foo(1.5));
  assertTrue(foo(1));

  assertFalse(foo('1.5 number'));
  assertTrue(foo('1 number'));

  assertFalse(foo(false));
  assertTrue(foo(true));
})();

(function() {
  const set = new Set();
  set.add(true);

  for (let i = 0; i < 10000; i += 1) {
    set.add(i);
    set.add(`${i} number`);
  }

  function foo(x) {
    return set.has(x);
  }

  %PrepareFunctionForOptimization(foo);

  assertFalse(foo(1.5));
  assertTrue(foo(1));

  assertFalse(foo('1.5 number'));
  assertTrue(foo('1 number'));

  assertFalse(foo(false));
  assertTrue(foo(true));

  %OptimizeFunctionOnNextCall(foo);

  assertFalse(foo(1.5));
  assertTrue(foo(1));

  assertFalse(foo('1.5 number'));
  assertTrue(foo('1 number'));

  assertFalse(foo(false));
  assertTrue(foo(true));
})();
