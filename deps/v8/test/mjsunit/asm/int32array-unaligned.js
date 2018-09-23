// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM32 = new stdlib.Int32Array(heap);
  function load(i) {
    i = i|0;
    i = MEM32[i >> 2] | 0;
    return i;
  }
  function store(i, v) {
    i = i|0;
    v = v|0;
    MEM32[i >> 2] = v;
  }
  return { load: load, store: store };
}

var m = Module(this, {}, new ArrayBuffer(1024));

m.store(0, 0x12345678);
m.store(4, -1);
m.store(8, -1);
for (var i = 0; i < 4; ++i) {
  assertEquals(0x12345678, m.load(i));
}
for (var i = 4; i < 12; ++i) {
  assertEquals(-1, m.load(i));
}
for (var j = 4; j < 8; ++j) {
  m.store(j, 0x11223344);
  for (var i = 0; i < 4; ++i) {
    assertEquals(0x12345678, m.load(i));
  }
  for (var i = 4; i < 8; ++i) {
    assertEquals(0x11223344, m.load(i));
  }
  for (var i = 8; i < 12; ++i) {
    assertEquals(-1, m.load(i));
  }
}
