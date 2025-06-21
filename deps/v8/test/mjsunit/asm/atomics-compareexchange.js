// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Module(stdlib, foreign, heap, offset) {
  "use asm";
  var MEM8 = new stdlib.Int8Array(heap, offset);
  var MEM16 = new stdlib.Int16Array(heap, offset);
  var MEM32 = new stdlib.Int32Array(heap, offset);
  var MEMU8 = new stdlib.Uint8Array(heap, offset);
  var MEMU16 = new stdlib.Uint16Array(heap, offset);
  var MEMU32 = new stdlib.Uint32Array(heap, offset);
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

  return {
    compareExchangei8: compareExchangei8,
    compareExchangei16: compareExchangei16,
    compareExchangei32: compareExchangei32,
    compareExchangeu8: compareExchangeu8,
    compareExchangeu16: compareExchangeu16,
    compareExchangeu32: compareExchangeu32,
  };
}

function clearArray() {
  var ui8 = new Uint8Array(sab);
  for (var i = 0; i < sab.byteLength; ++i) {
    ui8[i] = 0;
  }
}

function testElementType(taConstr, f, oobValue, offset) {
  clearArray();

  var ta = new taConstr(sab, offset);
  var name = Object.prototype.toString.call(ta);
  assertEquals(0, ta[0]);
  assertEquals(0, f(0, 0, 50), name);
  assertEquals(50, ta[0]);
  // Value is not equal to 0, so compareExchange won't store 100
  assertEquals(50, f(0, 0, 100), name);
  assertEquals(50, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0, 0); });
  assertThrows(function() { f(ta.length, 0, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.compareExchangei8, 0, offset);
  testElementType(Int16Array, m.compareExchangei16, 0, offset);
  testElementType(Int32Array, m.compareExchangei32, 0, offset);
  testElementType(Uint8Array, m.compareExchangeu8, 0, offset);
  testElementType(Uint16Array, m.compareExchangeu16, 0, offset);
  testElementType(Uint32Array, m.compareExchangeu32, 0, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
