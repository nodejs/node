// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var DOUBLE_ZERO = %AllocateHeapNumber();
var SMI_ZERO = 0;
var MINUS_ZERO = -0.0;

function min1(a, b) {
  a = +a;
  b = +b;
  return +(a < b ? a : b);
}

function min2(a, b) {
  a = +a;
  b = +b;
  return a < b ? a : b;
}

for (f of [min1, min2]) {
  for (var i = 0; i < 5; i++) {
    assertEquals(3, f(3, 4));
    assertEquals(3, f(4, 3));
    assertEquals(3.3, f(3.3, 4));
    assertEquals(3.4, f(4, 3.4));

    assertEquals(-Infinity, 1 / f(SMI_ZERO, MINUS_ZERO));
    assertEquals(-Infinity, 1 / f(DOUBLE_ZERO, MINUS_ZERO));
    assertEquals(Infinity, 1 / f(MINUS_ZERO, SMI_ZERO));
    assertEquals(Infinity, 1 / f(MINUS_ZERO, DOUBLE_ZERO));

    assertEquals(NaN, f(NaN, NaN));
    assertEquals(NaN, f(3, NaN));
    assertEquals(3, f(NaN, 3));
  }
}

function min3(a, b) {
  a = +a;
  b = +b;
  return +(a > b ? b : a);
}

function min4(a, b) {
  a = +a;
  b = +b;
  return a > b ? b : a;
}

for (f of [min3, min4]) {
  for (var i = 0; i < 5; i++) {
    assertEquals(3, f(3, 4));
    assertEquals(3, f(4, 3));
    assertEquals(3.3, f(3.3, 4));
    assertEquals(3.4, f(4, 3.4));

    assertEquals(Infinity, 1 / f(SMI_ZERO, MINUS_ZERO));
    assertEquals(Infinity, 1 / f(DOUBLE_ZERO, MINUS_ZERO));
    assertEquals(-Infinity, 1 / f(MINUS_ZERO, SMI_ZERO));
    assertEquals(-Infinity, 1 / f(MINUS_ZERO, DOUBLE_ZERO));

    assertEquals(NaN, f(NaN, NaN));
    assertEquals(3, f(3, NaN));
    assertEquals(NaN, f(NaN, 3));
  }
}
