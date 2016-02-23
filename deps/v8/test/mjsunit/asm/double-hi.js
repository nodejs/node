// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var stdlib = this;
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);


var m = (function(stdlib, foreign, heap) {
  "use asm";
  function hi1(i) {
    i = +i;
    return %_DoubleHi(i)|0;
  }
  function hi2(i, j) {
    i = +i;
    j = +j;
    return %_DoubleHi(i)+%_DoubleHi(j)|0;
  }
  return { hi1: hi1, hi2: hi2 };
})(stdlib, foreign, heap);

assertEquals(0, m.hi1(0.0));
assertEquals(-2147483648, m.hi1(-0.0));
assertEquals(2146435072, m.hi1(Infinity));
assertEquals(-1048576, m.hi1(-Infinity));
assertEquals(0, m.hi2(0.0, 0.0));
assertEquals(-2147483648, m.hi2(0.0, -0.0));
assertEquals(-2147483648, m.hi2(-0.0, 0.0));
assertEquals(0, m.hi2(-0.0, -0.0));
for (var i = -2147483648; i < 2147483648; i += 3999773) {
  assertEquals(%_DoubleHi(i), m.hi1(i));
  assertEquals(i, m.hi1(%ConstructDouble(i, 0)));
  assertEquals(i, m.hi1(%ConstructDouble(i, i)));
  assertEquals(i+i|0, m.hi2(%ConstructDouble(i, 0), %ConstructDouble(i, 0)));
  assertEquals(i+i|0, m.hi2(%ConstructDouble(i, i), %ConstructDouble(i, i)));
}
