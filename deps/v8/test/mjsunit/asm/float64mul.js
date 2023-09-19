// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  function f1(i) {
    i = +i;
    return +(i * -1);
  }
  function f2(i) {
    i = +i;
    return +(-1. * i);
  }
  function f3(i) {
    i = +i;
    return +(-i);
  }
  return { f1: f1, f2: f2, f3: f3 };
}

var m = Module(this, {}, new ArrayBuffer(64 * 1024));

assertEquals(NaN, m.f1(NaN));
assertEquals(NaN, m.f2(NaN));
assertEquals(NaN, m.f3(NaN));
assertEquals(Infinity, 1 / m.f1(-0));
assertEquals(Infinity, 1 / m.f2(-0));
assertEquals(Infinity, 1 / m.f3(-0));
assertEquals(Infinity, m.f1(-Infinity));
assertEquals(Infinity, m.f2(-Infinity));
assertEquals(Infinity, m.f3(-Infinity));
assertEquals(-Infinity, 1 / m.f1(0));
assertEquals(-Infinity, 1 / m.f2(0));
assertEquals(-Infinity, 1 / m.f3(0));
assertEquals(-Infinity, m.f1(Infinity));
assertEquals(-Infinity, m.f2(Infinity));
assertEquals(-Infinity, m.f3(Infinity));
for (var i = -2147483648; i < 2147483648; i += 3999777) {
  assertEquals(-i, m.f1(i));
  assertEquals(-i, m.f2(i));
  assertEquals(-i, m.f3(i));
}
