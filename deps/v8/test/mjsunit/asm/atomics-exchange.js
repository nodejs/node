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

function testElementType(taConstr, f) {
  clearArray();

  var ta = new taConstr(sab);
  var name = Object.prototype.toString.call(ta);
  ta[0] = 0x7f;
  assertEquals(0x7f, f(0, 0xf), name);
  assertEquals(0xf, ta[0]);
  // out of bounds
  assertEquals(0, f(-1, 0), name);
  assertEquals(0, f(ta.length, 0), name);
}

testElementType(Int8Array, m.exchangei8);
testElementType(Int16Array, m.exchangei16);
testElementType(Int32Array, m.exchangei32);
testElementType(Uint8Array, m.exchangeu8);
testElementType(Uint16Array, m.exchangeu16);
testElementType(Uint32Array, m.exchangeu32);
