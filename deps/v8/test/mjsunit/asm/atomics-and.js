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
  ta[0] = 0x7f;
  assertEquals(0x7f, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  assertEquals(0xf, f(0, 0x19), name);
  assertEquals(0x9, ta[0]);
  // out of bounds
  assertEquals(0, f(-1, 0), name);
  assertEquals(0, f(ta.length, 0), name);
}

testElementType(Int8Array, m.andi8);
testElementType(Int16Array, m.andi16);
testElementType(Int32Array, m.andi32);
testElementType(Uint8Array, m.andu8);
testElementType(Uint16Array, m.andu16);
testElementType(Uint32Array, m.andu32);
