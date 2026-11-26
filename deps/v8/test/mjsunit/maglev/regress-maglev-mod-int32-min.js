// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

d8.file.execute('test/mjsunit/mjsunit.js');

function modWithShift(x, y) {
  let a = x << 31;
  return a % y;
}

%PrepareFunctionForOptimization(modWithShift);
for (let i = 0; i < 2000; i++) {
  modWithShift(0, 3);
}
%OptimizeMaglevOnNextCall(modWithShift);
modWithShift(0, 3);
assertOptimized(modWithShift);

// INT32_MIN = -2147483648
// Negative dividend with non-power-of-two divisor
assertEquals(-2, modWithShift(1, 3));
assertEquals(-2, modWithShift(1, -3));
assertEquals(-3, modWithShift(1, 5));
assertEquals(-3, modWithShift(1, -5));
assertEquals(-2, modWithShift(1, 7));
assertEquals(-2, modWithShift(1, -7));

// Power-of-two divisor with zero remainder should deopt to produce -0.0
assertEquals(-0, modWithShift(1, 2));
assertEquals(-0, modWithShift(1, 1));

function modGeneral(x, y) {
  return x % y;
}

%PrepareFunctionForOptimization(modGeneral);
for (let i = 0; i < 2000; i++) {
  modGeneral(-7, 3);
}
%OptimizeMaglevOnNextCall(modGeneral);
modGeneral(-7, 3);
assertOptimized(modGeneral);

assertEquals(-1, modGeneral(-7, 3));
assertEquals(-1, modGeneral(-7, -3));
assertEquals(-0, modGeneral(-6, 3));
assertEquals(1, modGeneral(7, 3));
assertEquals(1, modGeneral(7, -3));
