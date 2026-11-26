// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function addTestFunc(builder, name, i64_val) {
  builder.addFunction(name, kSig_i_i)
      .addLocals(kWasmI64, 1)
      .addLocals(kWasmF32, 1)
      .addBody([
        ...wasmI64Const(i64_val),
        kExprLocalSet, 1,
        ...wasmF32Const(1.5),
        kExprLocalSet, 2,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            // Redundant condition eliminated by BranchEliminationReducer,
            // making the loop backedge dead so the Phis collapse to constants
            // in WasmDeadCodeEliminationPhase without MachineOptimization.
            kExprLocalGet, 0,
            kExprI32Eqz,
            kExprIf, kWasmVoid,
              kExprLocalGet, 1,
              kExprI32ConvertI64,
              kExprLocalGet, 2,
              kExprI32ReinterpretF32,
              kExprI32Add,
              kExprReturn,
            kExprEnd,
            ...wasmI64Const(999n),
            kExprLocalSet, 1,
            ...wasmF32Const(2.5),
            kExprLocalSet, 2,
            kExprBr, 1,
          kExprEnd,
        kExprEnd,
        kExprI32Const, 0,
      ])
      .exportFunc();
}

const cases = [
  ['large_const', 1234567890123n],
  ['small_const', 42n],
  ['zero_const', 0n],
  ['neg_one', -1n],
  ['int32_min', -2147483648n],
  ['int32_min_plus_one', -2147483647n],
  ['int32_max', 2147483647n],
  ['uint32_max', 0xffffffffn],
];

const builder = new WasmModuleBuilder();
for (const [name, val] of cases) {
  addTestFunc(builder, name, val);
}

const instance = builder.instantiate();
const f32_bits = 0x3fc00000;  // 1.5f

for (const [name, val] of cases) {
  const expected = (Number(BigInt.asIntN(32, val)) + f32_bits) | 0;
  const fn = instance.exports[name];
  assertEquals(expected, fn(0));
  assertEquals(0, fn(1));
  %WasmTierUpFunction(fn);
  assertEquals(expected, fn(0));
  assertEquals(0, fn(1));
}
