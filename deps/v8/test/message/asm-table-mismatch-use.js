// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-validate-asm --no-suppress-asm-messages

// Violates asm.js {funTable} use in {f} doesn't match its use in {g}.

function Module() {
  "use asm"
  function f(a) {
    a = a | 0;
    a = funTable[a & 0](a | 0) | 0;
    return a | 0;
  }
  function g(a) {
    a = a | 0;
    a = funTable[a & 0](2.3) | 0;
    return a | 0;
  }
  var funTable = [ f ];
  return { f:f, g:g };
}
Module();
