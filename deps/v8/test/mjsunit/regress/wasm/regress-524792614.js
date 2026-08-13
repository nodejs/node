// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const g = builder.addGlobal(kWasmI32, true);

builder.addFunction('main', makeSig([kWasmI32], [kWasmI32, kWasmI32]))
    .exportAs('main')
    .addLocals(kWasmI32, 2)
    .addBody([
      // v2 = param 0 + 1
      kExprLocalGet,
      0,
      kExprI32Const,
      1,
      kExprI32Add,
      kExprLocalSet,
      1,

      // v27 = 0 - v2
      kExprI32Const,
      0,
      kExprLocalGet,
      1,
      kExprI32Sub,
      kExprLocalSet,
      2,

      // select(v27, 0, v27)
      kExprLocalGet,
      2,
      kExprI32Const,
      0,
      kExprLocalGet,
      2,
      kExprSelect,

      // If block to split the block (pushes Return use to next block)
      kExprLocalGet,
      0,
      kExprIf,
      kWasmVoid,
      kExprI32Const,
      0,
      kExprGlobalSet,
      g.index,
      kExprEnd,

      // Return v6, v2
      kExprLocalGet,
      1,
    ]);

const instance = builder.instantiate()
instance.exports.main();
%WasmTierUpFunction(instance.exports.main);
