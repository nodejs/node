// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM8 = new stdlib.Uint8Array(heap);
  function load(i) {
    i = i|0;
    i = MEM8[i] | 0;
    return i;
  }
  function store(i, v) {
    i = i|0;
    v = v|0;
    MEM8[i] = v;
  }
  return { load: load, store: store };
}

var m = Module(this, {}, new ArrayBuffer(1));

m.store(0, 255);
for (var i = 1; i < 64; ++i) {
  m.store(i * 1 * 32 * 1024, i);
}
assertEquals(255, m.load(0));
for (var i = 1; i < 64; ++i) {
  assertEquals(0, m.load(i * 1 * 32 * 1024));
}
