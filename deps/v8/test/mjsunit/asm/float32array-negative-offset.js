// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = this;
var buffer = new ArrayBuffer(64 * 1024);
var foreign = {}


var m = (function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM32 = new stdlib.Float32Array(heap);
  function load(i) {
    i = i|0;
    return +MEM32[i >> 2];
  }
  function store(i, v) {
    i = i|0;
    v = +v;
    MEM32[i >> 2] = v;
  }
  function load8(i) {
    i = i|0;
    return +MEM32[i + 8 >> 2];
  }
  function store8(i, v) {
    i = i|0;
    v = +v;
    MEM32[i + 8 >> 2] = v;
  }
  return { load: load, store: store, load8: load8, store8: store8 };
})(stdlib, foreign, buffer);

assertEquals(NaN, m.load(-8));
assertEquals(NaN, m.load8(-16));
m.store(0, 42.0);
assertEquals(42.0, m.load8(-8));
m.store8(-8, 99.0);
assertEquals(99.0, m.load(0));
assertEquals(99.0, m.load8(-8));
