// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

(function() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(16, 32, false);
  builder.addFunction('test', kSig_i_i)
      .addBodyWithEnd([
  kExprI32Const, 0x41,
      kExprI32Const, 0x45,
      kExprI32Const, 0x41,
     kExprI32DivU,
    kExprI32LoadMem8S, 0x00, 0x3a,
      kExprI32Const, 0x75,
       kExprI32Const, 0x75,
         kExprI32Const, 0x6e,
        kExprI32Eqz,
       kExprI32LoadMem8S, 0x00, 0x3a,
      kExprI32Add,
     kExprI32DivU,
    kExprI32LoadMem8S, 0x00, 0x74,
   kExprI32And,
  kExprI32Eqz,
 kExprI32And,
kExprMemoryGrow, 0x00,
   kExprI32Const, 0x55,
  kExprI32LoadMem8S, 0x00, 0x3a,
   kExprI32LoadMem16U, 0x00, 0x71,
   kExprI32Const, 0x00,
  kExprI32RemU,
 kExprI32And,
kExprI32Eqz,
kExprEnd,   // @44
      ])
      .exportFunc();
  var module = builder.instantiate();
  assertThrows(() => {module.exports.test(1);});
})();
