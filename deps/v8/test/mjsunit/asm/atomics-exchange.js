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
  var exchange = stdlib.Atomics.exchange;
  var fround = stdlib.Math.fround;

  function exchangei8(i, x) {
    i = i | 0;
    x = x | 0;
    return exchange(MEM8, i, x)|0;
  }

  function exchangei16(i, x) {
    i = i | 0;
    x = x | 0;
    return exchange(MEM16, i, x)|0;
  }

  function exchangei32(i, x) {
    i = i | 0;
    x = x | 0;
    return exchange(MEM32, i, x)|0;
  }

  function exchangeu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return exchange(MEMU8, i, x)>>>0;
  }

  function exchangeu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return exchange(MEMU16, i, x)>>>0;
  }

  function exchangeu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return exchange(MEMU32, i, x)>>>0;
  }

  return {
    exchangei8: exchangei8,
    exchangei16: exchangei16,
    exchangei32: exchangei32,
    exchangeu8: exchangeu8,
    exchangeu16: exchangeu16,
    exchangeu32: exchangeu32,
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

function testElementType(taConstr, f, offset) {
  clearArray();

  var ta = new taConstr(sab, offset);
  var name = Object.prototype.toString.call(ta);
  ta[0] = 0x7f;
  assertEquals(0x7f, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.exchangei8, offset);
  testElementType(Int16Array, m.exchangei16, offset);
  testElementType(Int32Array, m.exchangei32, offset);
  testElementType(Uint8Array, m.exchangeu8, offset);
  testElementType(Uint16Array, m.exchangeu16, offset);
  testElementType(Uint32Array, m.exchangeu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
