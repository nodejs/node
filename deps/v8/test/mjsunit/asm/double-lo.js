// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var stdlib = this;
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);


var m = (function(stdlib, foreign, heap) {
  "use asm";
  function lo1(i) {
    i = +i;
    return %_DoubleLo(i)|0;
  }
  function lo2(i, j) {
    i = +i;
    j = +j;
    return %_DoubleLo(i)+%_DoubleLo(j)|0;
  }
  return { lo1: lo1, lo2: lo2 };
})(stdlib, foreign, heap);

assertEquals(0, m.lo1(0.0));
assertEquals(0, m.lo1(-0.0));
assertEquals(0, m.lo1(Infinity));
assertEquals(0, m.lo1(-Infinity));
assertEquals(0, m.lo2(0.0, 0.0));
assertEquals(0, m.lo2(0.0, -0.0));
assertEquals(0, m.lo2(-0.0, 0.0));
assertEquals(0, m.lo2(-0.0, -0.0));
for (var i = -2147483648; i < 2147483648; i += 3999773) {
  assertEquals(%_DoubleLo(i), m.lo1(i));
  assertEquals(i, m.lo1(%ConstructDouble(0, i)));
  assertEquals(i, m.lo1(%ConstructDouble(i, i)));
  assertEquals(i+i|0, m.lo2(%ConstructDouble(0, i), %ConstructDouble(0, i)));
  assertEquals(i+i|0, m.lo2(%ConstructDouble(i, i), %ConstructDouble(i, i)));
}
