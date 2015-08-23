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

var sab = new SharedArrayBuffer(16);
var m = Module(this, {}, sab);

function clearArray() {
  var ui8 = new Uint8Array(sab);
  for (var i = 0; i < sab.byteLength; ++i) {
    ui8[i] = 0;
  }
}

function testElementType(taConstr, f) {
  clearArray();

  var ta = new taConstr(sab);
  var name = Object.prototype.toString.call(ta);
  assertEquals(0, f(0, 10), name);
  assertEquals(10, ta[0]);
  assertEquals(10, f(0, 10), name);
  assertEquals(20, ta[0]);
  // out of bounds
  assertEquals(0, f(-1, 0), name);
  assertEquals(0, f(ta.length, 0), name);
}

testElementType(Int8Array, m.addi8);
testElementType(Int16Array, m.addi16);
testElementType(Int32Array, m.addi32);
testElementType(Uint8Array, m.addu8);
testElementType(Uint16Array, m.addu16);
testElementType(Uint32Array, m.addu32);
