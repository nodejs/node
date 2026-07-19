// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function Module(stdlib, imports, buffer) {
  "use asm";
  function f() {
    var bar = 0;
    return 0x1e+bar|0;
  }
  return f;
}
var f = Module(this);
assertTrue(%IsWasmCode(f));
assertTrue(%IsAsmWasmCode(Module));
assertEquals(0x1e, f());
