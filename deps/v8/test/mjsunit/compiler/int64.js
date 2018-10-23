// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test NumberAdd with PositiveSafeInteger -> PositiveSafeInteger (as Tagged).
(function() {
  function foo(x) {
    const i = x ? 0xFFFFFFFF : 0;
    return i + 1;
  }

  assertEquals(0x000000001, foo(false));
  assertEquals(0x000000001, foo(false));
  assertEquals(0x100000000, foo(true));
  assertEquals(0x100000000, foo(true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0x000000001, foo(false));
  assertEquals(0x100000000, foo(true));
})();

// Test NumberAdd with SafeInteger -> SafeInteger (as Tagged).
(function() {
  function foo(x) {
    const i = x ? 0xFFFFFFFF : -1;
    return i + 1;
  }

  assertEquals(0x000000000, foo(false));
  assertEquals(0x000000000, foo(false));
  assertEquals(0x100000000, foo(true));
  assertEquals(0x100000000, foo(true));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0x000000000, foo(false));
  assertEquals(0x100000000, foo(true));
})();

// NumberAdd: Smi x Unsigned32 -> SafeInteger (as Float64).
(function() {
  const a = new Float64Array(1);

  function foo(o) {
    a[0] = o.x + 0xFFFFFFFF;
    return a[0];
  }

  assertEquals(0x0FFFFFFFF, foo({x:0}));
  assertEquals(0x100000000, foo({x:1}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0x100000000, foo({x:1}));
})();

// NumberAdd: Smi x Unsigned32 -> SafeInteger (as TaggedSigned).
(function() {
  function foo(o) {
    return {x: Math.floor((o.x + 11123456789) + -11123456788)}.x;
  }

  assertEquals(1, foo({x:0}));
  assertEquals(2, foo({x:1}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo({x:1}));
})();

// NumberSubtract: Unsigned32 x Unsigned32 -> SafeInteger (as Word32).
(function() {
  function foo(a, i) {
    i = ((i >>> 0)) - 0xFFFFFFFF;
    return a[i];
  }

  assertEquals(1, foo([1], 0xFFFFFFFF));
  assertEquals(2, foo([2], 0xFFFFFFFF));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([3], 0xFFFFFFFF));
})();

// Test that the Deoptimizer can handle Word64 properly.
(function() {
  function foo(b) {
    const i = ((b >>> 0)) - 0xFFFFFFFF;
    %DeoptimizeFunction(foo);
    return i;
  }

  assertEquals(0, foo(0xFFFFFFFF));
  assertEquals(0, foo(0xFFFFFFFF));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(0, foo(0xFFFFFFFF));
})();
