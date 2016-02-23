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

var m = Module(this, {}, new ArrayBuffer(4));

m.store(0, 0x12345678);
for (var i = 1; i < 64; ++i) {
  m.store(i * 4 * 32 * 1024, i);
}
assertEquals(0x12345678, m.load(0));
for (var i = 1; i < 64; ++i) {
  assertEquals(0, m.load(i * 4 * 32 * 1024));
}
