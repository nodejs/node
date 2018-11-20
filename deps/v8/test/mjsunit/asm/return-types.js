// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --validate-asm

(function SuccessReturnTypesMatch() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return 2.3;
      if ((a | 0) == 2) return 4.2;
      return 6.5;
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(2.3, m.f(1));
  assertEquals(4.2, m.f(2));
  assertEquals(6.5, m.f(3));
})();

(function FailReturnTypesMismatch() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return 2.3;
      if ((a | 0) == 2) return 123;
      return 4.2;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(2.3, m.f(1));
  assertEquals(123, m.f(2));
  assertEquals(4.2, m.f(3));
})();

(function FailFallOffNonVoidFunction() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return 2.3;
      if ((a | 0) == 2) return 4.2;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(2.3, m.f(1));
  assertEquals(4.2, m.f(2));
  assertEquals(undefined, m.f(3));
})();

(function FailNonVoidVoidMismatch() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return 2.3;
      if ((a | 0) == 2) return;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(2.3, m.f(1));
  assertEquals(undefined, m.f(2));
  assertEquals(undefined, m.f(3));
})();

(function FailVoidNonVoidMismatch() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return;
      if ((a | 0) == 2) return 2.3;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(undefined, m.f(1));
  assertEquals(2.3, m.f(2));
  assertEquals(undefined, m.f(3));
})();

(function SuccessVoidFunction() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return;
      return;
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(undefined, m.f(1));
  assertEquals(undefined, m.f(2));
})();

(function SuccessFallOffVoidFunction() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if ((a | 0) == 1) return;
    }
    return { f:f };
  }
  var m = Module(this);
  assertTrue(%IsAsmWasmCode(Module));
  assertEquals(undefined, m.f(1));
  assertEquals(undefined, m.f(2));
})();
