// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  "use asm";
  const v = -0;
  function bar() {
    return v;
  }
  return { d: bar };
}

var m = foo();
assertEquals(-Infinity, 1 / m.d());
assertTrue(%IsAsmWasmCode(foo));
