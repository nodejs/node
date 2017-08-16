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
  var load = stdlib.Atomics.load;
  var fround = stdlib.Math.fround;

  function loadi8(i) {
    i = i | 0;
    return load(MEM8, i)|0;
  }

  function loadi16(i) {
    i = i | 0;
    return load(MEM16, i)|0;
  }

  function loadi32(i) {
    i = i | 0;
    return load(MEM32, i)|0;
  }

  function loadu8(i) {
    i = i | 0;
    return load(MEMU8, i)>>>0;
  }

  function loadu16(i) {
    i = i | 0;
    return load(MEMU16, i)>>>0;
  }

  function loadu32(i) {
    i = i | 0;
    return load(MEMU32, i)>>>0;
  }

  return {
    loadi8: loadi8,
    loadi16: loadi16,
    loadi32: loadi32,
    loadu8: loadu8,
    loadu16: loadu16,
    loadu32: loadu32,
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
  ta[0] = 10;
  assertEquals(10, f(0), name);
  assertEquals(0, f(1), name);
  // out of bounds
  assertThrows(function() { f(-1); });
  assertThrows(function() { f(ta.length); });
}

function testElement(m, offset) {
  testElementType(Int8Array, m.loadi8, offset);
  testElementType(Int16Array, m.loadi16, offset);
  testElementType(Int32Array, m.loadi32, offset);
  testElementType(Uint8Array, m.loadu8, offset);
  testElementType(Uint16Array, m.loadu16, offset);
  testElementType(Uint32Array, m.loadu32, offset);
}

var offset = 0;
var sab = new SharedArrayBuffer(16);
var m1 = Module(this, {}, sab, offset);
testElement(m1, offset);

offset = 32;
sab = new SharedArrayBuffer(64);
var m2 = Module(this, {}, sab, offset);
testElement(m2, offset);
