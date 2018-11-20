// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM32 = new stdlib.Float32Array(heap);
  function load(i) {
    i = i|0;
    i = +MEM32[i >> 2];
    return i;
  }
  function store(i, v) {
    i = i|0;
    v = +v;
    MEM32[i >> 2] = v;
  }
  return { load: load, store: store };
}

var m = Module(this, {}, new ArrayBuffer(4));

m.store(0, 42.0);
for (var i = 1; i < 64; ++i) {
  m.store(i * 4 * 32 * 1024, i);
}
assertEquals(42.0, m.load(0));
for (var i = 1; i < 64; ++i) {
  assertEquals(NaN, m.load(i * 4 * 32 * 1024));
}
