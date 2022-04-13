// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sharedarraybuffer

function Module(stdlib, foreign, heap, offset) {
  "use asm";
  var MEM8 = new stdlib.Int8Array(heap, offset);
  var MEM16 = new stdlib.Int16Array(heap, offset);
  var MEM32 = new stdlib.Int32Array(heap, offset);
  var MEMU8 = new stdlib.Uint8Array(heap, offset);
  var MEMU16 = new stdlib.Uint16Array(heap, offset);
  var MEMU32 = new stdlib.Uint32Array(heap, offset);
  var add = stdlib.Atomics.add;
  var fround = stdlib.Math.fround;

  function addi8(i, x) {
    i = i | 0;
    x = x | 0;
    return add(MEM8, i, x)|0;
  }

  function addi16(i, x) {
    i = i | 0;
    x = x | 0;
    return add(MEM16, i, x)|0;
  }

  function addi32(i, x) {
    i = i | 0;
    x = x | 0;
    return add(MEM32, i, x)|0;
  }

  function addu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return add(MEMU8, i, x)>>>0;
  }

  function addu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return add(MEMU16, i, x)>>>0;
  }

  function addu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return add(MEMU32, i, x)>>>0;
  }

  return {
    addi8: addi8,
    addi16: addi16,
    addi32: addi32,
    addu8: addu8,
    addu16: addu16,
    addu32: addu32,
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
  assertEquals(0, f(0, 10), name);
  assertEquals(10, ta[0]);
  assertEquals(10, f(0, 10), name);
  assertEquals(20, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.addi8, offset);
  testElementType(Int16Array, m.addi16, offset);
  testElementType(Int32Array, m.addi32, offset);
  testElementType(Uint8Array, m.addu8, offset);
  testElementType(Uint16Array, m.addu16, offset);
  testElementType(Uint32Array, m.addu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
