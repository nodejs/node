// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --no-stress-validate-asm --no-suppress-asm-messages

// Violates asm.js because symbol {f} is defined as module function twice.

function Module() {
  "use asm"
  function f() {}
  function f() {}
  return { f:f };
}
Module().f();
