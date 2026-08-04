// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module(stdlib) {
  "use asm";
  var fround = stdlib.Math.fround;
  // The below constant is outside the range of representable {float} values.
  const infinity = fround(1.7976931348623157e+308);
  function f() {
    return infinity;
  }
  return { f: f };
}

var m = Module(this);
assertEquals(Infinity, m.f());
assertTrue(%IsAsmWasmCode(Module));
