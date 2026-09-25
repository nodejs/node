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
  var sub = stdlib.Atomics.sub;
  var fround = stdlib.Math.fround;

  function subi8(i, x) {
    i = i | 0;
    x = x | 0;
    return sub(MEM8, i, x)|0;
  }

  function subi16(i, x) {
    i = i | 0;
    x = x | 0;
    return sub(MEM16, i, x)|0;
  }

  function subi32(i, x) {
    i = i | 0;
    x = x | 0;
    return sub(MEM32, i, x)|0;
  }

  function subu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return sub(MEMU8, i, x)>>>0;
  }

  function subu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return sub(MEMU16, i, x)>>>0;
  }

  function subu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return sub(MEMU32, i, x)>>>0;
  }

  return {
    subi8: subi8,
    subi16: subi16,
    subi32: subi32,
    subu8: subu8,
    subu16: subu16,
    subu32: subu32,
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
  ta[0] = 30;
  assertEquals(30, f(0, 10), name);
  assertEquals(20, ta[0]);
  assertEquals(20, f(0, 10), name);
  assertEquals(10, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.subi8, offset);
  testElementType(Int16Array, m.subi16, offset);
  testElementType(Int32Array, m.subi32, offset);
  testElementType(Uint8Array, m.subu8, offset);
  testElementType(Uint16Array, m.subu16, offset);
  testElementType(Uint32Array, m.subu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
