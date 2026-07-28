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
  var and = stdlib.Atomics.and;
  var fround = stdlib.Math.fround;

  function andi8(i, x) {
    i = i | 0;
    x = x | 0;
    return and(MEM8, i, x)|0;
  }

  function andi16(i, x) {
    i = i | 0;
    x = x | 0;
    return and(MEM16, i, x)|0;
  }

  function andi32(i, x) {
    i = i | 0;
    x = x | 0;
    return and(MEM32, i, x)|0;
  }

  function andu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return and(MEMU8, i, x)>>>0;
  }

  function andu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return and(MEMU16, i, x)>>>0;
  }

  function andu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return and(MEMU32, i, x)>>>0;
  }

  return {
    andi8: andi8,
    andi16: andi16,
    andi32: andi32,
    andu8: andu8,
    andu16: andu16,
    andu32: andu32,
  };
}

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
  assertEquals(0xf, f(0, 0x19), name);
  assertEquals(0x9, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.andi8, offset);
  testElementType(Int16Array, m.andi16, offset);
  testElementType(Int32Array, m.andi32, offset);
  testElementType(Uint8Array, m.andu8, offset);
  testElementType(Uint16Array, m.andu16, offset);
  testElementType(Uint32Array, m.andu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
