// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function umin(a, b) {
  a = a >>> 0;
  b = b >>> 0;
  return Math.min(a, b);
}

umin(1, 1);
umin(2, 2);
%OptimizeFunctionOnNextCall(umin);
assertEquals(1, umin(1, 2));
assertEquals(1, umin(2, 1));
assertEquals(0, umin(0, 4294967295));
assertEquals(0, umin(4294967295, 0));
assertEquals(4294967294, umin(-1, -2));
assertEquals(1234, umin(-2, 1234));

function umax(a, b) {
  a = a >>> 0;
  b = b >>> 0;
  return Math.max(a, b);
}

umax(1, 1);
umax(2, 2);
%OptimizeFunctionOnNextCall(umax);
assertEquals(2, umax(1, 2));
assertEquals(2, umax(2, 1));
assertEquals(4294967295, umax(0, 4294967295));
assertEquals(4294967295, umax(4294967295, 0));
assertEquals(4294967295, umax(-1, -2));
assertEquals(4294967294, umax(-2, 1234));
