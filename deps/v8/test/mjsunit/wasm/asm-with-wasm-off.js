// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

// NOTE: This is in it's own file because it calls %DisallowCodegenFromStrings,
// which messes with the isolate's state.
(function testAsmWithWasmOff() {
  %DisallowCodegenFromStrings();
  function Module() {
    'use asm';
    function foo() {
      return 0;
    }
    return {foo: foo};
  }
  Module();
  assertTrue(%IsAsmWasmCode(Module));
})();
