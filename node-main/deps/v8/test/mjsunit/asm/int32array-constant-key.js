// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM32 = new stdlib.Int32Array(heap);
  function loadm4194304() {
    var i = -4194304 << 2;
    return MEM32[i >> 2] | 0;
  }
  function loadm0() {
    return MEM32[-0] | 0;
  }
  function load0() {
    return MEM32[0] | 0;
  }
  function load4() {
    return MEM32[4] | 0;
  }
  function storem4194304(v) {
    v = v | 0;
    var i = -4194304 << 2;
    MEM32[i >> 2] = v;
  }
  function storem0(v) {
    v = v | 0;
    MEM32[-0] = v;
  }
  function store0(v) {
    v = v | 0;
    MEM32[0 >> 2] = v;
  }
  function store4(v) {
    v = v | 0;
    MEM32[(4 << 2) >> 2] = v;
  }
  return { loadm4194304: loadm4194304, storem4194304: storem4194304,
           loadm0: loadm0, storem0: storem0, load0: load0, store0: store0,
           load4: load4, store4: store4 };
}

var m = Module(this, {}, new ArrayBuffer(4));

assertEquals(0, m.loadm4194304());
assertEquals(0, m.loadm0());
assertEquals(0, m.load0());
assertEquals(0, m.load4());
m.storem4194304(123456789);
assertEquals(0, m.loadm4194304());
assertEquals(0, m.loadm0());
assertEquals(0, m.load0());
assertEquals(0, m.load4());
m.storem0(987654321);
assertEquals(0, m.loadm4194304());
assertEquals(987654321, m.loadm0());
assertEquals(987654321, m.load0());
assertEquals(0, m.load4());
m.store0(0x12345678);
assertEquals(0, m.loadm4194304());
assertEquals(0x12345678, m.loadm0());
assertEquals(0x12345678, m.load0());
assertEquals(0, m.load4());
m.store4(43);
assertEquals(0, m.loadm4194304());
assertEquals(0x12345678, m.loadm0());
assertEquals(0x12345678, m.load0());
assertEquals(0, m.load4());
