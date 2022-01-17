// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  function f(a, b) {
    a = +a;
    b = +b;
    return fround(a, b);
  }
  return { f: f };
}

var m = Module(this);
assertEquals(23, m.f(23));
assertEquals(42, m.f(42, 65));
assertFalse(%IsAsmWasmCode(Module));
