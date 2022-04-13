// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestUnalignedMemory() {
  // Test that a buffer whose length is not a multiple of the element size of a
  // heap view throws the proper {RangeError} during instantiation.
  function Module(stdlib, foreign, heap) {
    "use asm";
    var a = new stdlib.Int32Array(heap);
    function f() {}
    return { f:f };
  }
  assertThrows(() => Module(this, {}, new ArrayBuffer(2)), RangeError);
  assertThrows(() => Module(this, {}, new ArrayBuffer(10)), RangeError);
  assertDoesNotThrow(() => Module(this, {}, new ArrayBuffer(4)));
  assertDoesNotThrow(() => Module(this, {}, new ArrayBuffer(16)));
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMissingMemory() {
  // Test that a buffer is required for instantiation of modules containing any
  // heap views. JavaScript needs to create individual buffers for each view.
  function Module(stdlib, foreign, heap) {
    "use asm";
    var a = new stdlib.Int16Array(heap);
    var b = new stdlib.Int32Array(heap);
    function f() {
      a[0] = 0x1234;
      return b[0] | 0;
    }
    return { f:f };
  }
  var m = Module(this, {}, undefined);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(0, m.f());
})();

(function TestNonBufferMemory() {
  // Test that a buffer has to be an instance of {ArrayBuffer} in order to be
  // valid. JavaScript will also accept any other array-like object.
  function Module(stdlib, foreign, heap) {
    "use asm";
    var a = new stdlib.Int32Array(heap);
    function f() {
      return a[0] | 0;
    }
    return { f:f };
  }
  var m = Module(this, {}, [ 23, 42 ]);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(23, m.f());
})();
