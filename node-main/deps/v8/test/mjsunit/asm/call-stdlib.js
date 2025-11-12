// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --validate-asm

// This file contains test cases that are particularly interesting because they
// omit the usual call-site coercion of function calls that target well-known
// stdlib functions.

(function SuccessStdlibWithoutAnnotation() {
  function Module(stdlib, imports, heap) {
    "use asm";
    var imul = stdlib.Math.imul;
    function f(a, b) {
      a = a | 0;
      b = b | 0;
      var r = 0;
      r = imul(a, b);
      return r | 0;
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(966, m.f(23, 42));
  assertEquals(-0x0fffffff, m.f(0x7ffffff, 0x7ffffff));
})();

(function SuccessStdlibWithoutAnnotationThenRound() {
  function Module(stdlib, imports, heap) {
    "use asm";
    var fround = stdlib.Math.fround;
    var imul = stdlib.Math.imul;
    function f(a, b) {
      a = a | 0;
      b = b | 0;
      var r = fround(0);
      r = fround(imul(a, b));
      return fround(r);
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(966, m.f(23, 42));
  assertEquals(-0x0fffffff - 1, m.f(0x7ffffff, 0x7ffffff));
})();

(function FailureStdlibWithoutAnnotationMismatch() {
  function Module(stdlib, imports, heap) {
    "use asm";
    var fround = stdlib.Math.fround;
    var imul = stdlib.Math.imul;
    function f(a, b) {
      a = a | 0;
      b = b | 0;
      var r = fround(0);
      r = imul(a, b);
      return r | 0;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(966, m.f(23, 42));
  assertEquals(-0x0fffffff, m.f(0x7ffffff, 0x7ffffff));
})();

(function SuccessStdlibWithoutAnnotationUsedInReturn() {
  function Module(stdlib, imports, heap) {
    "use asm";
    var imul = stdlib.Math.imul;
    function f(a, b) {
      a = a | 0;
      b = b | 0;
      return imul(a, b);
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(966, m.f(23, 42));
  assertEquals(-0x0fffffff, m.f(0x7ffffff, 0x7ffffff));
})();
