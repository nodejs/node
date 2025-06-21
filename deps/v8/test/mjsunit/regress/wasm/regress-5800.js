// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function AddTest() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprBlock, kWasmVoid,
        kExprI64Const, 0,
        // 0x80 ... 0x10 is the LEB encoding of 0x100000000. This is chosen so
        // that the 64-bit constant has a non-zero top half. In this bug, the
        // top half was clobbering eax, leading to the function return 1 rather
        // than 0.
        kExprI64Const, 0x80, 0x80, 0x80, 0x80, 0x10,
        kExprI64Add,
        kExprI64Eqz,
        kExprBrIf, 0,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprI32Const, 0
    ])
    .exportFunc();
  let module = builder.instantiate();
  assertEquals(0, module.exports.main());
})();

(function SubTest() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprBlock, kWasmVoid,
        kExprI64Const, 0,
        // 0x80 ... 0x10 is the LEB encoding of 0x100000000. This is chosen so
        // that the 64-bit constant has a non-zero top half. In this bug, the
        // top half was clobbering eax, leading to the function return 1 rather
        // than 0.
        kExprI64Const, 0x80, 0x80, 0x80, 0x80, 0x10,
        kExprI64Sub,
        kExprI64Eqz,
        kExprBrIf, 0,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprI32Const, 0
    ])
    .exportFunc();
  let module = builder.instantiate();
  assertEquals(0, module.exports.main());
})();
