// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = []
for (var i = 0; i < 9; i++) a[i] = i + 1;

function test(f, arg1, arg2, expected) {
  assertEquals(expected, f(arg1));
  f(arg2);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f(arg1));
}

test(function f0() {
  return a[7] * a[6] * a[5] * a[4] * a[3] * a[2] * a[1] * a[0];
}, 0, 1, 40320);

test(function f1() {
  return a[7] * a[6] * a[5] * a[4] * a[10] * a[2] * a[1] * a[0];
}, 0, 1, NaN);

test(function f2() {
  return a[0] * a[1] * a[2] * a[3] * a[4] * a[5] * a[6] * a[7];
}, 0, 1, 40320);

test(function f3() {
  return a[3] * a[0] * a[6] * a[7] * a[5] * a[1] * a[4] * a[2];
}, 0, 1, 40320);

test(function f4(b) {
  return a[b+3] * a[0] * a[b+6] * a[7] * a[b+5] * a[1] * a[b+4] * a[2];
}, 0, 1, 40320);

test(function f5(b) {
  return a[b+1] * a[0] * a[b+4] * a[7] * a[b+3] * a[1] * a[b+2] * a[2];
}, 2, 3, 40320);

test(function f6(b) {
  var c;
  if (b) c = a[3] * a[0] * a[6] * a[7];
  return c * a[5] * a[1] * a[4] * a[2];
}, true, false, 40320);

test(function f7(b) {
  var c = a[7];
  if (b) c *= a[3] * a[0] * a[6];
  return c * a[5] * a[1] * a[4] * a[2];
}, true, false, 40320);

test(function f8(b) {
  var c = a[7];
  if (b) c *= a[3] * a[0] * a[6];
  return c * a[5] * a[10] * a[4] * a[2];
}, true, false, NaN);

test(function f9(b) {
  var c = a[1];
  if (b) {
    c *= a[3] * a[0] * a[6];
  } else {
    c = a[6] * a[5] * a[4];
  }
  return c * a[5] * a[7] * a[4] * a[2];
}, true, false, 40320);

test(function fa(b) {
  var c = a[1];
  if (b) {
    c = a[6] * a[b+5] * a[4];
  } else {
    c *= a[b+3] * a[0] * a[b+6];
  }
  return c * a[5] * a[b+7] * a[4] * a[2];
}, 0, 1, 40320);

test(function fb(b) {
  var c = a[b-3];
  if (b != 4) {
    c = a[6] * a[b+1] * a[4];
  } else {
    c *= a[b-1] * a[0] * a[b+2];
  }
  return c * a[5] * a[b+3] * a[4] * a[b-2];
}, 4, 3, 40320);

test(function fc(b) {
  var c = a[b-3];
  if (b != 4) {
    c = a[6] * a[b+1] * a[4];
  } else {
    c *= a[b-1] * a[0] * a[b+2];
  }
  return c * a[5] * a[b+3] * a[4] * a[b-2];
}, 6, 3, NaN);

test(function fd(b) {
  var c = a[b-3];
  if (b != 4) {
    c = a[6] * a[b+1] * a[4];
  } else {
    c *= a[b-1] * a[0] * a[b+2];
  }
  return c * a[5] * a[b+3] * a[4] * a[b-2];
}, 1, 4, NaN);

test(function fe(b) {
  var c = 1;
  for (var i = 1; i < b-1; i++) {
    c *= a[i-1] * a[i] * a[i+1];
  }
  return c;
}, 8, 4, (40320 / 8 / 7) * (40320 / 8) * (40320 / 2));

test(function ff(b) {
  var c = 0;
  for (var i = 0; i < b; i++) {
    c += a[3] * a[0] * a[6] * a[7] * a[5] * a[1] * a[4] * a[2];
  }
  return c;
}, 100, 4, 40320 * 100);
