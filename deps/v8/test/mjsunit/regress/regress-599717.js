// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function __f_61(stdlib, foreign, buffer) {
  "use asm";
  var __v_14 = new stdlib.Float64Array(buffer);
  function __f_74() {
    var __v_35 = 6.0;
    __v_14[2] = __v_35 + 1.0;
  }
  return {__f_74: __f_74};
}
var ok = false;
try {
  var __v_12 = new ArrayBuffer(2147483648);
  ok = true;
} catch (e) {
  // Can happen on 32 bit systems.
}
if (ok) {
  var module = __f_61(this, null, __v_12);
  assertTrue(%IsAsmWasmCode(__f_61));
}
