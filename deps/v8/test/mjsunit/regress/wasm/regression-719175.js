// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --wasm-interpret-all

function asm() {
  'use asm';
  function f() {
    if (1.0 % 2.5 == -0.75) {
    }
    return 0;
  }
  return {f: f};
}
asm().f();
