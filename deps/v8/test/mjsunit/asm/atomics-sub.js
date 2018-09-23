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
  ta[0] = 30;
  assertEquals(30, f(0, 10), name);
  assertEquals(20, ta[0]);
  assertEquals(20, f(0, 10), name);
  assertEquals(10, ta[0]);
  // out of bounds
  assertEquals(0, f(-1, 0), name);
  assertEquals(0, f(ta.length, 0), name);
}

testElementType(Int8Array, m.subi8);
testElementType(Int16Array, m.subi16);
testElementType(Int32Array, m.subi32);
testElementType(Uint8Array, m.subu8);
testElementType(Uint16Array, m.subu16);
testElementType(Uint32Array, m.subu32);
