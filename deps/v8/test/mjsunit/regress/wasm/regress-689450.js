// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(16, 32, false);
  builder.addFunction('test', kSig_i_i)
      .addBodyWithEnd([
              kExprGetLocal, 0x00,
              kExprI32Const, 0x29,
            kExprI32Shl,
            kExprI32Const, 0x18,
          kExprI32ShrS,
          kExprI32Const, 0x18,
        kExprI32Shl,
        kExprEnd,
      ])
      .exportFunc();
  var module = builder.instantiate();
  assertEquals(0, module.exports.test(16));
})();
