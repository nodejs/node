// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var stdlib = this;
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);


var m = (function(stdlib, foreign, heap) {
  "use asm";
  function cd1(i, j) {
    i = i|0;
    j = j|0;
    return +%_ConstructDouble(i, j);
  }
  function cd2(i) {
    i = i|0;
    return +%_ConstructDouble(0, i);
  }
  return { cd1: cd1, cd2: cd2 };
})(stdlib, foreign, heap);

assertEquals(0.0, m.cd1(0, 0));
assertEquals(%ConstructDouble(0, 1), m.cd2(1));
for (var i = -2147483648; i < 2147483648; i += 3999773) {
  assertEquals(%ConstructDouble(0, i), m.cd2(i));
  for (var j = -2147483648; j < 2147483648; j += 3999773) {
    assertEquals(%ConstructDouble(i, j), m.cd1(i, j));
  }
}
