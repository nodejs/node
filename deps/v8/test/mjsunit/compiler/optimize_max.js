// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var DOUBLE_ZERO = %AllocateHeapNumber();
var SMI_ZERO = 0;
var MINUS_ZERO = -0.0;

function max1(a, b) {
  a = +a;
  b = +b;
  return +(a < b ? b : a);
}

function max2(a, b) {
  a = +a;
  b = +b;
  return a < b ? b : a;
}

for (f of [max1, max2]) {
  for (var i = 0; i < 5; i++) {
    assertEquals(4, f(3, 4));
    assertEquals(4, f(4, 3));
    assertEquals(4.3, f(3.3, 4.3));
    assertEquals(4.4, f(4.4, 3.4));

    assertEquals(Infinity, 1 / f(SMI_ZERO, MINUS_ZERO));
    assertEquals(Infinity, 1 / f(DOUBLE_ZERO, MINUS_ZERO));
    assertEquals(-Infinity, 1 / f(MINUS_ZERO, SMI_ZERO));
    assertEquals(-Infinity, 1 / f(MINUS_ZERO, DOUBLE_ZERO));

    assertEquals(NaN, f(NaN, NaN));
    assertEquals(3, f(3, NaN));
    assertEquals(NaN, f(NaN, 3));
  }
}

function max3(a, b) {
  a = +a;
  b = +b;
  return +(a > b ? a : b);
}

function max4(a, b) {
  a = +a;
  b = +b;
  return a > b ? a : b;
}

for (f of [max3, max4]) {
  for (var i = 0; i < 5; i++) {
    assertEquals(4, f(3, 4));
    assertEquals(4, f(4, 3));
    assertEquals(4.3, f(3.3, 4.3));
    assertEquals(4.4, f(4.4, 3.4));

    assertEquals(-Infinity, 1 / f(SMI_ZERO, MINUS_ZERO));
    assertEquals(-Infinity, 1 / f(DOUBLE_ZERO, MINUS_ZERO));
    assertEquals(Infinity, 1 / f(MINUS_ZERO, SMI_ZERO));
    assertEquals(Infinity, 1 / f(MINUS_ZERO, DOUBLE_ZERO));

    assertEquals(NaN, f(NaN, NaN));
    assertEquals(NaN, f(3, NaN));
    assertEquals(3, f(NaN, 3));
  }
}
