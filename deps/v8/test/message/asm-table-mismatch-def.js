// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-validate-asm --no-suppress-asm-messages

// Violates asm.js {funTable} definition doesn't match the use in {f}.

function Module() {
  "use asm"
  function f(a) {
    a = a | 0;
    a = funTable[a & 0](a | 0) | 0;
    return a | 0;
  }
  function g() {
    return 2.3;
  }
  var funTable = [ g ];
  return { f:f };
}
Module().f();
