// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function test() {
  assertEquals(6.1, Math.min(6.1, 6.2));
  assertEquals(6.1, Math.min(6.2, 6.1));

  assertEquals(-6.2, Math.min(-6.1, -6.2));
  assertEquals(-6.2, Math.min(-6.2, -6.1));

  assertTrue(Object.is(-0, Math.min(-0.0, 0.0)));
  assertTrue(Object.is(-0, Math.min(0.0, -0.0)));
  assertFalse(Object.is(-0, Math.min(0.0, 0.0)));
  assertTrue(Object.is(-0, Math.min(-0.0, -0.0)));

  assertTrue(isNaN(Math.min(5.2, NaN)));
  assertTrue(isNaN(Math.min(NaN, 5.3)));

  assertSame(Infinity, Math.min(Infinity, Infinity));
  assertSame(-Infinity, Math.min(Infinity, -Infinity));
  assertSame(-Infinity, Math.min(-Infinity, Infinity));
  assertSame(-Infinity, Math.min(-Infinity, -Infinity));

  assertTrue(isNaN(Math.min(6.1, undefined)));
  assertTrue(isNaN(Math.min(undefined, 6.2)));
  assertEquals(-2, Math.min(-2, false));
  assertEquals(0, Math.min(2, false));
  assertEquals(-2, Math.min(false, -2));
  assertEquals(0, Math.min(false, 2));
  assertEquals(-2, Math.min(-2, true));
  assertEquals(1, Math.min(2, true));
  assertEquals(-2, Math.min(true, -2));
  assertEquals(1, Math.min(true, 2));
  assertEquals(-2, Math.min(-2, null));
  assertEquals(0, Math.min(2, null));
  assertEquals(-2, Math.min(null, -2));
  assertEquals(0, Math.min(null, 2));
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeMaglevOnNextCall(test);
test();

// No deopts.
assertTrue(isMaglevved(test));
