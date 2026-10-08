// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-tier-mask-for-testing=1

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let helperBuilder = new WasmModuleBuilder();
let sig64_h =
    helperBuilder.addType(makeSig([kWasmI64], [kWasmI64, kWasmI64, kWasmI64]));
let sig32_h =
    helperBuilder.addType(makeSig([kWasmI32], [kWasmI32, kWasmI32, kWasmI32]));

helperBuilder.addFunction('prime', sig64_h)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 0, kExprLocalGet, 0])
    .exportFunc();

helperBuilder.addFunction('get', sig32_h)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 0, kExprLocalGet, 0])
    .exportFunc();

let helper = helperBuilder.instantiate({});

let builder = new WasmModuleBuilder();
let sig64 = builder.addType(makeSig([kWasmI64], [kWasmI64, kWasmI64, kWasmI64]));
let sig32 = builder.addType(makeSig([kWasmI32], [kWasmI32, kWasmI32, kWasmI32]));
let primeImport = builder.addImport('m', 'prime', sig64);
let getImport = builder.addImport('m', 'get', sig32);

builder.addFunction('test', makeSig([kWasmI64, kWasmI32, kWasmI32], [kWasmI64]))
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, primeImport,
      kExprDrop, kExprDrop, kExprDrop,
      kExprLocalGet, 2,
      kExprIf, kWasmVoid,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 1,
          kExprCallFunction, getImport,
          kExprLocalGet, 3,
          kExprI32Sub,
          kExprI64UConvertI32,
          kExprLocalGet, 2,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprLocalGet, 1,
            kExprLocalSet, 3,
            kExprBr, 1,
          kExprEnd,
          kExprReturn,
        kExprEnd,
      kExprEnd,
      kExprI64Const, 42,
    ])
    .exportFunc();

let instance = builder.instantiate({m: helper.exports});

assertEquals(0x4d2n, instance.exports.test(0x133700000000n, 1234, 1));
