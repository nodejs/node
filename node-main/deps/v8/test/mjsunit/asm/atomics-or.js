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
  var or = stdlib.Atomics.or;
  var fround = stdlib.Math.fround;

  function ori8(i, x) {
    i = i | 0;
    x = x | 0;
    return or(MEM8, i, x)|0;
  }

  function ori16(i, x) {
    i = i | 0;
    x = x | 0;
    return or(MEM16, i, x)|0;
  }

  function ori32(i, x) {
    i = i | 0;
    x = x | 0;
    return or(MEM32, i, x)|0;
  }

  function oru8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return or(MEMU8, i, x)>>>0;
  }

  function oru16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return or(MEMU16, i, x)>>>0;
  }

  function oru32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return or(MEMU32, i, x)>>>0;
  }

  return {
    ori8: ori8,
    ori16: ori16,
    ori32: ori32,
    oru8: oru8,
    oru16: oru16,
    oru32: oru32,
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
  assertEquals(0, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  assertEquals(0xf, f(0, 0x11), name);
  assertEquals(0x1f, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.ori8, offset);
  testElementType(Int16Array, m.ori16, offset);
  testElementType(Int32Array, m.ori32, offset);
  testElementType(Uint8Array, m.oru8, offset);
  testElementType(Uint16Array, m.oru16, offset);
  testElementType(Uint32Array, m.oru32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
