// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function __f_14() {
  "use asm";
  function __f_15() { return 0; }
  function __f_15() { return 137; }  // redeclared function
  return {};
}
__f_14();
assertFalse(%IsAsmWasmCode(__f_14));
