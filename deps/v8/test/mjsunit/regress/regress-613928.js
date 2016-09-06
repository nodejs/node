// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

(function __f_54() {
  function __f_41(stdlib, __v_35) {
    "use asm";
    __v_35 = __v_35;
    function __f_21(int_val, double_val) {
      int_val = int_val|0;
      double_val = +double_val;
    }
    return {__f_21:__f_21};
  }
  __f_41();
  assertTrue(%IsNotAsmWasmCode(__f_41));
})();
