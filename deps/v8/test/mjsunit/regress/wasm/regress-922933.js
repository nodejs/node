// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI64], [kWasmI64]));
builder.addFunction(undefined, sig)
  .addLocals({i32_count: 14}).addLocals({i64_count: 17}).addLocals({f32_count: 14})
  .addBody([
    kExprBlock, kWasmStmt,
      kExprBr, 0x00,
      kExprEnd,
    kExprBlock, kWasmStmt,
      kExprI32Const, 0x00,
      kExprSetLocal, 0x09,
      kExprI32Const, 0x00,
      kExprIf, kWasmStmt,
        kExprBlock, kWasmStmt,
          kExprI32Const, 0x00,
          kExprSetLocal, 0x0a,
          kExprBr, 0x00,
          kExprEnd,
        kExprBlock, kWasmStmt,
          kExprBlock, kWasmStmt,
            kExprGetLocal, 0x00,
            kExprSetLocal, 0x12,
            kExprBr, 0x00,
            kExprEnd,
          kExprGetLocal, 0x16,
          kExprSetLocal, 0x0f,
          kExprGetLocal, 0x0f,
          kExprSetLocal, 0x17,
          kExprGetLocal, 0x0f,
          kExprSetLocal, 0x18,
          kExprGetLocal, 0x17,
          kExprGetLocal, 0x18,
          kExprI64ShrS,
          kExprSetLocal, 0x19,
          kExprUnreachable,
          kExprEnd,
        kExprUnreachable,
      kExprElse,
        kExprUnreachable,
        kExprEnd,
      kExprUnreachable,
      kExprEnd,
    kExprUnreachable
]);
builder.instantiate();
