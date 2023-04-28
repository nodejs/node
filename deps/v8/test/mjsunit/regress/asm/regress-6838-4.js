// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestMinusZeroIsDouble() {
  function Module(stdlib) {
    'use asm';
    function f() {
      var x = 0.;
      x = 1. / +-0;
      return +x;
    }
    return f;
  }
  var f = Module(this);
  assertEquals(-Infinity, f());
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestMinusZeroIsDoubleBracketed() {
  function Module(stdlib) {
    'use asm';
    function f() {
      var x = 0.;
      x = 1. / (-0);
      return +x;
    }
    return f;
  }
  var f = Module(this);
  assertEquals(-Infinity, f());
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestMinusZeroIsDoubleMultDouble1() {
  function Module(stdlib) {
    'use asm';
    function f() {
      var x = 0.;
      x = 1. / (-0 * 1.0);
      return +x;
    }
    return f;
  }
  var f = Module(this);
  assertEquals(-Infinity, f());
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestMinusZeroIsDoubleMultDouble2() {
  function Module(stdlib) {
    'use asm';
    function f() {
      var x = 0.;
      x = 1. / (1.0 * -0);
      return +x;
    }
    return f;
  }
  var f = Module(this);
  assertEquals(-Infinity, f());
  assertTrue(%IsAsmWasmCode(Module));
})();

(function TestMinusZeroIsDoubleMultInt() {
  function Module(stdlib) {
    'use asm';
    function f() {
      var x = 0.;
      x = 1. / (-0 * 1);
      return +x;
    }
    return f;
  }
  var f = Module(this);
  assertFalse(%IsAsmWasmCode(Module));
})();
