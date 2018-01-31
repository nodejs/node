// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  const symbol = Symbol('symbol');
  const OBJS = [
    {[symbol]: 0, a: 1},
    {[symbol]: 1, b: 2},
    {[symbol]: 2, c: 3},
    {[symbol]: 3, d: 4}
  ];
  function foo(o) { return o[symbol]; }
  for (let i = 0; i < OBJS.length; ++i) {
    assertEquals(i, foo(OBJS[i]));
    assertEquals(i, foo(OBJS[i]));
  }
  %OptimizeFunctionOnNextCall(foo);
  for (let i = 0; i < OBJS.length; ++i) {
    assertEquals(i, foo(OBJS[i]));
    assertEquals(i, foo(OBJS[i]));
  }
})();

(function() {
  const symbol = Symbol('symbol');
  const OBJS = [
    {[symbol]: 0, a: 1},
    {[symbol]: 1, b: 2},
    {[symbol]: 2, c: 3},
    {[symbol]: 3, d: 4}
  ];
  function foo(o) { o[symbol] = o; }
  for (let i = 0; i < OBJS.length; ++i) {
    foo(OBJS[i]);
    foo(OBJS[i]);
  }
  %OptimizeFunctionOnNextCall(foo);
  for (let i = 0; i < OBJS.length; ++i) {
    foo(OBJS[i]);
    foo(OBJS[i]);
  }
  for (const o of OBJS) {
    assertEquals(o, o[symbol]);
  }
})();
