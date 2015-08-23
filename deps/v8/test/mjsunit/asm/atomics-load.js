// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-atomics --harmony-sharedarraybuffer

function Module(stdlib, foreign, heap) {
  "use asm";
  var MEM8 = new stdlib.Int8Array(heap);
  var MEM16 = new stdlib.Int16Array(heap);
  var MEM32 = new stdlib.Int32Array(heap);
  var MEMU8 = new stdlib.Uint8Array(heap);
  var MEMU16 = new stdlib.Uint16Array(heap);
  var MEMU32 = new stdlib.Uint32Array(heap);
  var MEMF32 = new stdlib.Float32Array(heap);
  var MEMF64 = new stdlib.Float64Array(heap);
  var load = stdlib.Atomics.load;
  var fround = stdlib.Math.fround;

  function loadi8(i) {
    i = i | 0;
    return load(MEM8, i)|0;
  }

  function loadi16(i) {
    i = i | 0;
    return load(MEM16, i)|0;
  }

  function loadi32(i) {
    i = i | 0;
    return load(MEM32, i)|0;
  }

  function loadu8(i) {
    i = i | 0;
    return load(MEMU8, i)>>>0;
  }

  function loadu16(i) {
    i = i | 0;
    return load(MEMU16, i)>>>0;
  }

  function loadu32(i) {
    i = i | 0;
    return load(MEMU32, i)>>>0;
  }

  function loadf32(i) {
    i = i | 0;
    return fround(load(MEMF32, i));
  }

  function loadf64(i) {
    i = i | 0;
    return +load(MEMF64, i);
  }

  return {
    loadi8: loadi8,
    loadi16: loadi16,
    loadi32: loadi32,
    loadu8: loadu8,
    loadu16: loadu16,
    loadu32: loadu32,
    loadf32: loadf32,
    loadf64: loadf64
  };
}

var sab = new SharedArrayBuffer(16);
var m = Module(this, {}, sab);

function clearArray() {
  var ui8 = new Uint8Array(sab);
  for (var i = 0; i < sab.byteLength; ++i) {
    ui8[i] = 0;
  }
}

function testElementType(taConstr, f, oobValue) {
  clearArray();

  var ta = new taConstr(sab);
  var name = Object.prototype.toString.call(ta);
  ta[0] = 10;
  assertEquals(10, f(0), name);
  assertEquals(0, f(1), name);
  // out of bounds
  assertEquals(oobValue, f(-1), name);
  assertEquals(oobValue, f(ta.length), name);
}

testElementType(Int8Array, m.loadi8, 0);
testElementType(Int16Array, m.loadi16, 0);
testElementType(Int32Array, m.loadi32, 0);
testElementType(Uint8Array, m.loadu8, 0);
testElementType(Uint16Array, m.loadu16, 0);
testElementType(Uint32Array, m.loadu32, 0);
testElementType(Float32Array, m.loadf32, NaN);
testElementType(Float64Array, m.loadf64, NaN);
