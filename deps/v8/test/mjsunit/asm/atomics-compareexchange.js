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
  var compareExchange = stdlib.Atomics.compareExchange;
  var fround = stdlib.Math.fround;

  function compareExchangei8(i, o, n) {
    i = i | 0;
    o = o | 0;
    n = n | 0;
    return compareExchange(MEM8, i, o, n)|0;
  }

  function compareExchangei16(i, o, n) {
    i = i | 0;
    o = o | 0;
    n = n | 0;
    return compareExchange(MEM16, i, o, n)|0;
  }

  function compareExchangei32(i, o, n) {
    i = i | 0;
    o = o | 0;
    n = n | 0;
    return compareExchange(MEM32, i, o, n)|0;
  }

  function compareExchangeu8(i, o, n) {
    i = i | 0;
    o = o >>> 0;
    n = n >>> 0;
    return compareExchange(MEMU8, i, o, n)>>>0;
  }

  function compareExchangeu16(i, o, n) {
    i = i | 0;
    o = o >>> 0;
    n = n >>> 0;
    return compareExchange(MEMU16, i, o, n)>>>0;
  }

  function compareExchangeu32(i, o, n) {
    i = i | 0;
    o = o >>> 0;
    n = n >>> 0;
    return compareExchange(MEMU32, i, o, n)>>>0;
  }

  function compareExchangef32(i, o, n) {
    i = i | 0;
    o = fround(o);
    n = fround(n);
    return fround(compareExchange(MEMF32, i, o, n));
  }

  function compareExchangef64(i, o, n) {
    i = i | 0;
    o = +o;
    n = +n;
    return +compareExchange(MEMF64, i, o, n);
  }

  return {
    compareExchangei8: compareExchangei8,
    compareExchangei16: compareExchangei16,
    compareExchangei32: compareExchangei32,
    compareExchangeu8: compareExchangeu8,
    compareExchangeu16: compareExchangeu16,
    compareExchangeu32: compareExchangeu32,
    compareExchangef32: compareExchangef32,
    compareExchangef64: compareExchangef64
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
  assertEquals(0, ta[0]);
  assertEquals(0, f(0, 0, 50), name);
  assertEquals(50, ta[0]);
  // Value is not equal to 0, so compareExchange won't store 100
  assertEquals(50, f(0, 0, 100), name);
  assertEquals(50, ta[0]);
  // out of bounds
  assertEquals(oobValue, f(-1, 0, 0), name);
  assertEquals(oobValue, f(ta.length, 0, 0), name);
}

testElementType(Int8Array, m.compareExchangei8, 0);
testElementType(Int16Array, m.compareExchangei16, 0);
testElementType(Int32Array, m.compareExchangei32, 0);
testElementType(Uint8Array, m.compareExchangeu8, 0);
testElementType(Uint16Array, m.compareExchangeu16, 0);
testElementType(Uint32Array, m.compareExchangeu32, 0);
testElementType(Float32Array, m.compareExchangef32, NaN);
testElementType(Float64Array, m.compareExchangef64, NaN);
