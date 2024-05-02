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
  var store = stdlib.Atomics.store;
  var fround = stdlib.Math.fround;

  function storei8(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM8, i, x)|0;
  }

  function storei16(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM16, i, x)|0;
  }

  function storei32(i, x) {
    i = i | 0;
    x = x | 0;
    return store(MEM32, i, x)|0;
  }

  function storeu8(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU8, i, x)>>>0;
  }

  function storeu16(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU16, i, x)>>>0;
  }

  function storeu32(i, x) {
    i = i | 0;
    x = x >>> 0;
    return store(MEMU32, i, x)>>>0;
  }

  return {
    storei8: storei8,
    storei16: storei16,
    storei32: storei32,
    storeu8: storeu8,
    storeu16: storeu16,
    storeu32: storeu32,
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
  assertEquals(10, f(0, 10), name);
  assertEquals(10, ta[0]);
  // out of bounds
  assertThrows(function() { f(-1, 0); });
  assertThrows(function() { f(ta.length, 0); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.storei8, offset);
  testElementType(Int16Array, m.storei16, offset);
  testElementType(Int32Array, m.storei32, offset);
  testElementType(Uint8Array, m.storeu8, offset);
  testElementType(Uint16Array, m.storeu16, offset);
  testElementType(Uint32Array, m.storeu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
