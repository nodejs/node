// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

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
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();

// No deopts.
assertOptimized(test);
