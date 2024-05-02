// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm --allow-natives-syntax

(function TestDeoptimizeArgMismatch() {
  function deopt() {
    %DeoptimizeFunction(test);
  }
  function Module(global, env, buffer) {
    "use asm";
    var deopt = env.deopt;
    function _main(i4, i5) {
      i4 = i4 | 0;
      i5 = i5 | 0;
      deopt();
      return i5 | 0;
    }
    return {'_main': _main}
  }
  function test() {
    var wasm = Module(null, {'deopt': deopt});
    wasm._main(0, 0, 0);
  }
  test();
})();
