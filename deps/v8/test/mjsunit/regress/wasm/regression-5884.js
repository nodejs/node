// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(31, 31, false);
  builder.addFunction('test', kSig_i_iii)
      .addBodyWithEnd([
        // body:
        kExprI64Const, 0x41, kExprI64Const, 0x41, kExprI64LtS, kExprI32Const,
        0x01, kExprI32StoreMem, 0x00, 0x41, kExprUnreachable,
        kExprEnd,  // @60
      ])
      .exportFunc();
  var module = builder.instantiate();
})();
