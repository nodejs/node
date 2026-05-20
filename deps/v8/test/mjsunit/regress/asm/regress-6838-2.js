// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestMathCeilReturningFloatish() {
  function Module(stdlib) {
    "use asm";
    var ceil = stdlib.Math.ceil;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return ceil(a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(3, f(2.2));
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMathFloorReturningFloatish() {
  function Module(stdlib) {
    "use asm";
    var floor = stdlib.Math.floor;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return floor(a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(2, f(2.2));
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMathSqrtReturningFloatish() {
  function Module(stdlib) {
    "use asm";
    var sqrt = stdlib.Math.sqrt;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return sqrt(a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(Math.sqrt(Math.fround(2.2)), f(2.2));
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMathAbsReturningFloatish() {
  function Module(stdlib) {
    "use asm";
    var abs = stdlib.Math.abs;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return abs(a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(Math.fround(2.2), f(-2.2));
  assertFalse(%IsAsmWasmCode(Module));
})();

(function TestMathMinReturningFloat() {
  function Module(stdlib) {
    "use asm";
    var min = stdlib.Math.min;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return min(a, a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(Math.fround(2.2), f(2.2));
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestMathMaxReturningFloat() {
  function Module(stdlib) {
    "use asm";
    var max = stdlib.Math.max;
    var fround = stdlib.Math.fround;
    function f(a) {
      a = fround(a);
      return max(a, a);
    }
    return f;
  }
  var f = Module(this);
  assertEquals(Math.fround(2.2), f(2.2));
  assertTrue(%IsAsmWasmCode(Module));
})();
