// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function __f_100() {
  "use asm";
  function __f_76() {
    var __v_39 = 0;
    outer: while (1) {
      while (__v_39 == 4294967295) {
      }
    }
  }
  return {__f_76: __f_76};
}
assertTrue(%IsNotAsmWasmCode(__f_100));
