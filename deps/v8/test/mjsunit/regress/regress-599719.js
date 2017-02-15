// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --validate-asm

function __f_7() {
    %DeoptimizeFunction(__f_5);
}
function __f_8(global, env) {
  "use asm";
  var __f_7 = env.__f_7;
  function __f_9(i4, i5) {
    i4 = i4 | 0;
    i5 = i5 | 0;
    __f_7();
  }
  return {'__f_9': __f_9}
}
function __f_5() {
  var __v_5 = __f_8({}, {'__f_7': __f_7});
  assertTrue(%IsAsmWasmCode(__f_8));
  __v_5.__f_9(0, 0, 0);
}
__f_5();
