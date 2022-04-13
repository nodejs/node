// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

// Changing the code a little to avoid infinite loop

function __f_109() {
  "use asm";
  function __f_18() {
    var a = 0;
    while(2147483648) {
      a = 1;
      break;
    }
    return a|0;
  }
  return {__f_18: __f_18};
}

var wasm = __f_109();
assertTrue(%IsAsmWasmCode(__f_109));
assertEquals(1, wasm.__f_18());
