// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

function __f_97(stdlib, buffer) {
  "use asm";
  var __v_30 = new stdlib.Int32Array(buffer);
  function __f_74() {
    var __v_27 = 4;
    __v_30[__v_27 >> __v_2] = ((__v_30[-1073741825]|-10) + 2) | 0;
  }
}
var module = __f_97(this);
assertTrue(%IsNotAsmWasmCode(__f_97));
