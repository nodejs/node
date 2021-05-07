// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function Module(stdlib, imports, buffer) {
  "use asm";
  function f(a,b,c) {
    a = a | 0;
    b = b | 0;
    c = c | 0;
    var x = 0;
    x = funTable[a & 1](funTable[b & 1](c) | 0) | 0;
    return x | 0;
  }
  function g(a) {
    a = a | 0;
    return (a + 23) | 0;
  }
  function h(a) {
    a = a | 0;
    return (a + 42) | 0;
  }
  var funTable = [ g, h ];
  return f;
}
var f = Module(this);
assertTrue(%IsWasmCode(f));
assertTrue(%IsAsmWasmCode(Module));
assertEquals(165, f(0, 1, 100));
