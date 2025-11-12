// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function test() {
  assertEquals(6.2, Math.max(6.1, 6.2));
  assertEquals(6.2, Math.max(6.2, 6.1));

  assertEquals(-6.1, Math.max(-6.1, -6.2));
  assertEquals(-6.1, Math.max(-6.2, -6.1));

  assertFalse(Object.is(-0, Math.max(-0.0, 0.0)));
  assertFalse(Object.is(-0, Math.max(0.0, -0.0)));
  assertFalse(Object.is(-0, Math.max(0.0, 0.0)));
  assertTrue(Object.is(-0, Math.max(-0.0, -0.0)));

  assertTrue(isNaN(Math.max(5.2, NaN)));
  assertTrue(isNaN(Math.max(NaN, 5.3)));

  assertSame(Infinity, Math.max(Infinity, Infinity));
  assertSame(Infinity, Math.max(Infinity, -Infinity));
  assertSame(Infinity, Math.max(-Infinity, Infinity));
  assertSame(-Infinity, Math.max(-Infinity, -Infinity));

  assertTrue(isNaN(Math.max(6.1, undefined)));
  assertTrue(isNaN(Math.max(undefined, 6.2)));
  assertEquals(0, Math.max(-2, false));
  assertEquals(2, Math.max(2, false));
  assertEquals(0, Math.max(false, -2));
  assertEquals(2, Math.max(false, 2));
  assertEquals(1, Math.max(-2, true));
  assertEquals(2, Math.max(2, true));
  assertEquals(1, Math.max(true, -2));
  assertEquals(2, Math.max(true, 2));
  assertEquals(0, Math.max(-2, null));
  assertEquals(2, Math.max(2, null));
  assertEquals(0, Math.max(null, -2));
  assertEquals(2, Math.max(null, 2));
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeMaglevOnNextCall(test);
test();

// No deopts.
assertTrue(isMaglevved(test));
