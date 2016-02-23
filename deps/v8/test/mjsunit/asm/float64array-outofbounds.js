// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM64 = new stdlib.Float64Array(heap);
  function load(i) {
    i = i|0;
    i = +MEM64[i >> 3];
    return i;
  }
  function store(i, v) {
    i = i|0;
    v = +v;
    MEM64[i >> 3] = v;
  }
  return { load: load, store: store };
}

var m = Module(this, {}, new ArrayBuffer(8));

m.store(0, 3.12);
for (var i = 1; i < 64; ++i) {
  m.store(i * 8 * 32 * 1024, i);
}
assertEquals(3.12, m.load(0));
for (var i = 1; i < 64; ++i) {
  assertEquals(NaN, m.load(i * 8 * 32 * 1024));
}
