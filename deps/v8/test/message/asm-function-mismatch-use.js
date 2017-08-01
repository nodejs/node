// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-opt --no-stress-validate-asm --no-suppress-asm-messages

// Violates asm.js because {g} has return type different from use of {g} in {f}.

function Module() {
  "use asm"
  function f() {
    return g() | 0;
  }
  function g() {
    return 2.3;
  }
  return { f:f };
}
Module().f();
