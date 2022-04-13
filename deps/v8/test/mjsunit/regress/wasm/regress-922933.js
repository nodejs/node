// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sig = builder.addType(makeSig([kWasmI64], [kWasmI64]));
builder.addFunction(undefined, sig)
  .addLocals(kWasmI32, 14).addLocals(kWasmI64, 17).addLocals(kWasmF32, 14)
  .addBody([
    kExprBlock, kWasmVoid,
      kExprBr, 0x00,
      kExprEnd,
    kExprBlock, kWasmVoid,
      kExprI32Const, 0x00,
      kExprLocalSet, 0x09,
      kExprI32Const, 0x00,
      kExprIf, kWasmVoid,
        kExprBlock, kWasmVoid,
          kExprI32Const, 0x00,
          kExprLocalSet, 0x0a,
          kExprBr, 0x00,
          kExprEnd,
        kExprBlock, kWasmVoid,
          kExprBlock, kWasmVoid,
            kExprLocalGet, 0x00,
            kExprLocalSet, 0x12,
            kExprBr, 0x00,
            kExprEnd,
          kExprLocalGet, 0x16,
          kExprLocalSet, 0x0f,
          kExprLocalGet, 0x0f,
          kExprLocalSet, 0x17,
          kExprLocalGet, 0x0f,
          kExprLocalSet, 0x18,
          kExprLocalGet, 0x17,
          kExprLocalGet, 0x18,
          kExprI64ShrS,
          kExprLocalSet, 0x19,
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
