// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Module() {
  'use asm';
  function f() {}
  function g() {
    var x = 0.0;
    table[x & 3]();
  }
  var table = [f, f, f, f];
  return { g: g };
}
var m = Module();
assertDoesNotThrow(m.g);
assertFalse(%IsAsmWasmCode(Module));
