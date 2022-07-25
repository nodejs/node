// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestMathMaxOnLargeInt() {
  function Module(stdlib) {
    "use asm";
    var max = stdlib.Math.max;
    function f() {
      return max(42,0xffffffff);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(0xffffffff, f());
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMathMinOnLargeInt() {
  function Module(stdlib) {
    "use asm";
    var min = stdlib.Math.min;
    function f() {
      return min(42,0xffffffff);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(42, f());
  assertFalse(%IsAsmWasmCode(Module));
})();
