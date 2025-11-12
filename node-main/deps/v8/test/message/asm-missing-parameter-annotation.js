// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-validate-asm --no-suppress-asm-messages

function Module() {
  "use asm"
  function f(a) {
    var b = 0;
    b = (a + 23) | 0;
    return b | 0;
  }
  return { f:f };
}
Module().f();
