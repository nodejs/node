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
  var xor = stdlib.Atomics.xor;
  var fround = stdlib.Math.fround;

  function xori8(i, x) {
    i = i | 0;
    x = x | 0;
    return xor(MEM8, i, x)|0;
  }

  function xori16(i, x) {
    i = i | 0;
    x = x | 0;
    return xor(MEM16, i, x)|0;
  }

  function xori32(i, x) {
    i = i | 0;
    x = x | 0;
    return xor(MEM32, i, x)|0;
  }

  function xoru8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return xor(MEMU8, i, x)>>>0;
  }

  function xoru16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return xor(MEMU16, i, x)>>>0;
  }

  function xoru32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return xor(MEMU32, i, x)>>>0;
  }

  return {
    xori8: xori8,
    xori16: xori16,
    xori32: xori32,
    xoru8: xoru8,
    xoru16: xoru16,
    xoru32: xoru32,
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
  assertEquals(0, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  assertEquals(0xf, f(0, 0x11), name);
  assertEquals(0x1e, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.xori8, offset);
  testElementType(Int16Array, m.xori16, offset);
  testElementType(Int32Array, m.xori32, offset);
  testElementType(Uint8Array, m.xoru8, offset);
  testElementType(Uint16Array, m.xoru16, offset);
  testElementType(Uint32Array, m.xoru32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
