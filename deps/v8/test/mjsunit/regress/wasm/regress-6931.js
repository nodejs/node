// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');


// This test checks for accidental sign extension. The Wasm spec says we do
// arbitrary precision unsigned arithmetic to compute the memory address,
// meaning this test should do 0xfffffffc + 8, which is 0x100000004 and out of
// bounds. However, if we interpret 0xfffffffc as -4, then the result is 4 and
// succeeds erroneously.


(function() {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.addFunction('test', kSig_v_v)
      .addBody([
        kExprI32Const, 0x7c, // address = -4
        kExprI32Const, 0,
        kExprI32StoreMem, 0, 8, // align = 0, offset = 8
      ])
      .exportFunc();
  let module = builder.instantiate();

  assertTraps(kTrapMemOutOfBounds, module.exports.test);
})();
