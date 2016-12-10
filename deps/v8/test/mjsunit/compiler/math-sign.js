// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function signInt32(i) {
  i = i|0;
  return Math.sign(i);
}

signInt32(0);
signInt32(2);
%OptimizeFunctionOnNextCall(signInt32);
assertEquals(1, signInt32(1));
assertEquals(0, signInt32(0));
assertEquals(-1, signInt32(-1));
assertEquals(-1, signInt32(-1));
assertEquals(1, signInt32(2147483647));
assertEquals(-1, signInt32(2147483648));
assertEquals(-1, signInt32(-2147483648));
assertEquals(0, signInt32(NaN));
assertEquals(0, signInt32(undefined));
assertEquals(0, signInt32(-0));

function signFloat64(i) {
  return Math.sign(+i);
}

signFloat64(0.1);
signFloat64(-0.1);
%OptimizeFunctionOnNextCall(signFloat64);
assertEquals(1, signFloat64(1));
assertEquals(1, signFloat64(0.001));
assertEquals(-1, signFloat64(-0.002));
assertEquals(1, signFloat64(1e100));
assertEquals(-1, signFloat64(-2e100));
assertEquals(0, signFloat64(0));
assertEquals(Infinity, 1/signFloat64(0));
assertEquals(-1, signFloat64(-1));
assertEquals(-1, signFloat64(-1));
assertEquals(1, signFloat64(2147483647));
assertEquals(1, signFloat64(2147483648));
assertEquals(-1, signFloat64(-2147483647));
assertEquals(-1, signFloat64(-2147483648));
assertEquals(-1, signFloat64(-2147483649));
assertEquals(-0, signFloat64(-0));
assertEquals(NaN, signFloat64(NaN));
assertEquals(NaN, signFloat64(undefined));
assertEquals(1, signFloat64(Infinity));
assertEquals(-1, signFloat64(-Infinity));
