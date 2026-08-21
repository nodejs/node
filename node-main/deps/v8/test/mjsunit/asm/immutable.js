// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function FailImmutableFunction() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if (a) {
        a = f((a - 1) | 0) | 0;
        f = 0;
        return (a + 1) | 0;
      }
      return 23;
    }
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(23, m.f(0));
  assertEquals(24, m.f(1));
  assertThrows(() => m.f(2));
})();

(function FailImmutableFunctionTable() {
  function Module(stdlib, imports, heap) {
    "use asm";
    function f(a) {
      a = a | 0;
      if (a) {
        a = funTable[a & 0]((a - 1) | 0) | 0;
        funTable = 0;
        return (a + 1) | 0;
      }
      return 23;
    }
    var funTable = [ f ];
    return { f:f };
  }
  var m = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
  assertEquals(23, m.f(0));
  assertEquals(24, m.f(1));
  assertThrows(() => m.f(2));
})();
