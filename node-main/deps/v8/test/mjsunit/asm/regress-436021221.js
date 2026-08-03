// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  'use asm';
  function f() {}
  return {
    __proto__: f,
  };
}
let m = f()
assertFalse(%IsAsmWasmCode(f));
assertThrows(() => print(m));
