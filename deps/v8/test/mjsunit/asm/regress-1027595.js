// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestF32StoreConvertsF64ToF32() {
  function Module(stdlib, foreign, heap) {
    'use asm';
    var f32 = new stdlib.Float32Array(heap);
    function f(a) {
      a = +a;
      f32[0] = f32[1] = a;
    }
    return f;
  }
  var buffer = new ArrayBuffer(0x10000);
  var f = Module(this, {}, buffer);
  assertDoesNotThrow(() => f(23.42));
  var view = new Float32Array(buffer);
  assertEquals(Math.fround(23.42), view[0]);
  assertEquals(Math.fround(23.42), view[1]);
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestF64StoreConvertsF32ToF64() {
  function Module(stdlib, foreign, heap) {
    'use asm';
    var fround = stdlib.Math.fround;
    var f64 = new stdlib.Float64Array(heap);
    function f(a) {
      a = fround(a);
      f64[0] = f64[1] = a;
    }
    return f;
  }
  var buffer = new ArrayBuffer(0x10000);
  var f = Module(this, {}, buffer);
  assertDoesNotThrow(() => f(23.42));
  var view = new Float64Array(buffer);
  assertEquals(Math.fround(23.42), view[0]);
  assertEquals(Math.fround(23.42), view[1]);
  assertTrue(%IsAsmWasmCode(Module));
})();
