// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestLeftRight() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function f(i) {
      i = i | 0;
      return HEAP32[i << 2 >> 2] | 0;
    }
    return { f:f }
  }
  var buffer = new ArrayBuffer(1024);
  var module = new Module(this, {}, buffer);
  assertTrue(%IsAsmWasmCode(Module));
  new Int32Array(buffer)[42] = 23;
  assertEquals(23, module.f(42));
})();

(function TestRightRight() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function f(i) {
      i = i | 0;
      return HEAP32[i >> 2 >> 2] | 0;
    }
    return { f:f }
  }
  var buffer = new ArrayBuffer(1024);
  var module = new Module(this, {}, buffer)
  assertTrue(%IsAsmWasmCode(Module));
  new Int32Array(buffer)[42 >> 4] = 23;
  assertEquals(23, module.f(42));
})();

(function TestRightLeft() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function f(i) {
      i = i | 0;
      return HEAP32[i >> 2 << 2] | 0;
    }
    return { f:f }
  }
  var buffer = new ArrayBuffer(1024);
  var module = new Module(this, {}, buffer)
  assertFalse(%IsAsmWasmCode(Module));
  new Int32Array(buffer)[42 & 0xfc] = 23;
  assertEquals(23, module.f(42));
})();

(function TestRightButNotImmediate() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function f(i) {
      i = i | 0;
      return HEAP32[i >> 2 + 1] | 0;
    }
    return { f:f }
  }
  var buffer = new ArrayBuffer(1024);
  var module = new Module(this, {}, buffer)
  assertFalse(%IsAsmWasmCode(Module));
  new Int32Array(buffer)[42 >> 3] = 23;
  assertEquals(23, module.f(42));
})();

(function TestLeftOnly() {
  function Module(stdlib, foreign, heap) {
    "use asm";
    var HEAP32 = new stdlib.Int32Array(heap);
    function f(i) {
      i = i | 0;
      return HEAP32[i << 2] | 0;
    }
    return { f:f }
  }
  var buffer = new ArrayBuffer(1024);
  var module = new Module(this, {}, buffer)
  assertFalse(%IsAsmWasmCode(Module));
  new Int32Array(buffer)[42 << 2] = 23;
  assertEquals(23, module.f(42));
})();
