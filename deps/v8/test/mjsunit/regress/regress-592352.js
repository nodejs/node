// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

function __f_76() {
  "use asm";
  function __f_72() {
    %OptimizeFunctionOnNextCall();
  }
  return {__f_72:__f_72};
}

try {
  Wasm.instantiateModuleFromAsm( __f_76.toString());
  assertTrue(false);
} catch (e) {
  print("Caught: " + e);
}
