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

function testElementType(taConstr, f) {
  clearArray();

  var ta = new taConstr(sab);
  var name = Object.prototype.toString.call(ta);
  assertEquals(0, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  assertEquals(0xf, f(0, 0x11), name);
  assertEquals(0x1f, ta[0]);
  // out of bounds
  assertEquals(0, f(-1, 0), name);
  assertEquals(0, f(ta.length, 0), name);
}

testElementType(Int8Array, m.ori8);
testElementType(Int16Array, m.ori16);
testElementType(Int32Array, m.ori32);
testElementType(Uint8Array, m.oru8);
testElementType(Uint16Array, m.oru16);
testElementType(Uint32Array, m.oru32);
