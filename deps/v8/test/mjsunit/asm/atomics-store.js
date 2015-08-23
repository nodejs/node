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
  var store = stdlib.Atomics.store;
  var fround = stdlib.Math.fround;

  function storei8(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM8, i, x)|0;
  }

  function storei16(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM16, i, x)|0;
  }

  function storei32(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM32, i, x)|0;
  }

  function storeu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU8, i, x)>>>0;
  }

  function storeu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU16, i, x)>>>0;
  }

  function storeu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU32, i, x)>>>0;
  }

  function storef32(i, x) {
    i = i | 0;
    x = fround(x);
    return fround(store(MEMF32, i, x));
  }

  function storef64(i, x) {
    i = i | 0;
    x = +x;
    return +store(MEMF64, i, x);
  }

  return {
    storei8: storei8,
    storei16: storei16,
    storei32: storei32,
    storeu8: storeu8,
    storeu16: storeu16,
    storeu32: storeu32,
    storef32: storef32,
    storef64: storef64
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
  assertEquals(10, f(0, 10), name);
  assertEquals(10, ta[0]);
  // out of bounds
  assertEquals(oobValue, f(-1, 0), name);
  assertEquals(oobValue, f(ta.length, 0), name);
}

testElementType(Int8Array, m.storei8, 0);
testElementType(Int16Array, m.storei16, 0);
testElementType(Int32Array, m.storei32, 0);
testElementType(Uint8Array, m.storeu8, 0);
testElementType(Uint16Array, m.storeu16, 0);
testElementType(Uint32Array, m.storeu32, 0);
testElementType(Float32Array, m.storef32, NaN);
testElementType(Float64Array, m.storef64, NaN);
