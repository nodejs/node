// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = new Float64Array(4);
a[2] *= -1;
a[3] *= -1;
assertEquals(0, a[0]);
assertEquals(0, a[1]);
assertEquals(-0, a[2]);
assertEquals(-0, a[3]);

function f1() {
  var z = a[0];
  // Same register.
  assertEquals(0, Math.min(z, z));
};
%PrepareFunctionForOptimization(f1);
function f2() {
  // Different registers.
  assertEquals(0, Math.min(a[0], a[1]));
};
%PrepareFunctionForOptimization(f2);
function f3() {
  // Zero and minus zero.
  assertEquals(-0, Math.min(a[1], a[2]));
};
%PrepareFunctionForOptimization(f3);
function f4() {
  // Zero and minus zero, reversed order.
  assertEquals(-0, Math.min(a[2], a[1]));
};
%PrepareFunctionForOptimization(f4);
function f5() {
  // Minus zero, same register.
  var m_z = a[2];
  assertEquals(-0, Math.min(m_z, m_z));
};
%PrepareFunctionForOptimization(f5);
function f6() {
  // Minus zero, different registers.
  assertEquals(-0, Math.min(a[2], a[3]));
};
%PrepareFunctionForOptimization(f6);
for (var i = 0; i < 3; i++) {
  f1();
  f2();
  f3();
  f4();
  f5();
  f6();
}
%OptimizeFunctionOnNextCall(f1);
%OptimizeFunctionOnNextCall(f2);
%OptimizeFunctionOnNextCall(f3);
%OptimizeFunctionOnNextCall(f4);
%OptimizeFunctionOnNextCall(f5);
%OptimizeFunctionOnNextCall(f6);
f1();
f2();
f3();
f4();
f5();
f6();
