// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-growable-stacks
// Flags: --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addGlobal(kWasmI32, true).exportAs('depth');

const sig = makeSig([kWasmS128], [kWasmS128]);

const import_index = builder.addImport("m", "import", kSig_v_v);

const sig_if = builder.addType(makeSig([], [kWasmS128]));

const deep_calc = builder.addFunction("deep_calc", sig);
const simd_const = [kExprI32Const, 1, ...SimdInstr(kExprI32x4Splat)];

deep_calc.addBody([
  kExprGlobalGet, 0,
  kExprI32Const, 1,
  kExprI32Sub,
  kExprGlobalSet, 0,

  kExprGlobalGet, 0,
  kExprIf, sig_if,
    kExprLocalGet, 0,
    ...simd_const,
    ...SimdInstr(kExprI32x4Add),
    kExprCallFunction, deep_calc.index,
  kExprElse,
    kExprCallFunction, import_index, // Call suspending import to trigger growth/switch
    kExprLocalGet, 0,
  kExprEnd
]);

// Helper to check if a SIMD value matches expected i32x4
// Returns 1 if match, 0 if mismatch
const check_simd = builder.addFunction("check_simd", makeSig([kWasmS128, kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    ...SimdInstr(kExprI32x4ExtractLane), 0,
    kExprLocalGet, 1,
    kExprI32Eq,

    kExprLocalGet, 0,
    ...SimdInstr(kExprI32x4ExtractLane), 1,
    kExprLocalGet, 2,
    kExprI32Eq,
    kExprI32And,

    kExprLocalGet, 0,
    ...SimdInstr(kExprI32x4ExtractLane), 2,
    kExprLocalGet, 3,
    kExprI32Eq,
    kExprI32And,

    kExprLocalGet, 0,
    ...SimdInstr(kExprI32x4ExtractLane), 3,
    kExprLocalGet, 4,
    kExprI32Eq,
    kExprI32And,
  ]);

const simd_1_2_3_4 = [1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0];

builder.addFunction("test", kSig_i_i)
  .addLocals(kWasmS128, 1)
  .addBody([
    // Initialize SIMD values: i32x4(1, 2, 3, 4)
    ...wasmS128Const(simd_1_2_3_4),

    kExprCallFunction, deep_calc.index,

    // Store results to locals
    kExprLocalSet, 1, // local 1 is arg0

    // Verify arg0 (local 1): expected (depth, 1 + depth, 2 + depth, 3 + depth)
    kExprLocalGet, 1,
    kExprLocalGet, 0,                                // depth
    kExprI32Const, 1, kExprLocalGet, 0, kExprI32Add, // 1 + depth
    kExprI32Const, 2, kExprLocalGet, 0, kExprI32Add, // 2 + depth
    kExprI32Const, 3, kExprLocalGet, 0, kExprI32Add, // 3 + depth
    kExprCallFunction, check_simd.index,
  ]).exportFunc();

const js_import = new WebAssembly.Suspending(() => {
  gc();
  return Promise.resolve();
});

const instance = builder.instantiate({ m: {
  import: js_import
} });
const wrapper = WebAssembly.promising(instance.exports.test);

const depth = 1000;
instance.exports.depth.value = depth;

assertPromiseResult(
  wrapper(depth),
  res => assertEquals(res, 1)
);
