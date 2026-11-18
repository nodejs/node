// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-growable-stacks
// Flags: --expose-gc --stack-size=400

d8.file.execute('test/mjsunit/mjsunit.js');
d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestStackGrowSIMDS128() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true).exportAs('depth');

  // 10 s128 params and 4 s128 returns exceed GP/FP register limits on all
  // architectures (max 8 FP param regs, max 2 FP return regs), ensuring both
  // register and stack parameter/return slots are used across stack growth and
  // return trampoline shrinking.
  const kNumParams = 10;
  const kNumReturns = 4;
  const paramTypes = Array(kNumParams).fill(kWasmS128);
  const returnTypes = Array(kNumReturns).fill(kWasmS128);
  const sig = makeSig(paramTypes, returnTypes);
  const import_index = builder.addImport('m', 'import', kSig_v_v);
  const sig_if = builder.addType(makeSig([], returnTypes));
  const deep_calc = builder.addFunction('deep_calc', sig);
  const simd_const = [kExprI32Const, 1, ...SimdInstr(kExprI32x4Splat)];

  const recursive_args = [];
  for (let i = 0; i < kNumParams; ++i) {
    recursive_args.push(
        kExprLocalGet, i, ...simd_const, ...SimdInstr(kExprI32x4Add));
  }

  // Return 2 register params (0, 1) and 2 stack params (8, 9) in the 4 return
  // slots (2 register returns + 2 stack returns).
  const return_indices = [0, 1, 8, 9];
  const base_returns = [];
  for (const idx of return_indices) {
    base_returns.push(kExprLocalGet, idx);
  }

  deep_calc.addBody([
    kExprGlobalGet, 0,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprGlobalSet, 0,

    kExprGlobalGet, 0,
    kExprIf, sig_if,
    ...recursive_args,
    kExprCallFunction, deep_calc.index,
    kExprElse,
    kExprCallFunction, import_index,
    ...base_returns,
    kExprEnd,
  ]);

  const check_simd =
      builder
          .addFunction(
              'check_simd',
              makeSig(
                  [kWasmS128, kWasmI32, kWasmI32, kWasmI32, kWasmI32],
                  [kWasmI32]))
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

  const makeSimdConst = (base) => wasmS128Const([
    base + 1, 0, 0, 0,
    base + 2, 0, 0, 0,
    base + 3, 0, 0, 0,
    base + 4, 0, 0, 0,
  ]);

  const test_body = [];
  for (let i = 0; i < kNumParams; ++i) {
    test_body.push(...makeSimdConst(i * 10));
  }
  test_body.push(kExprCallFunction, deep_calc.index);
  // Store the 4 s128 returns into locals 1..4 (in reverse pop order: 4..1).
  for (let r = kNumReturns; r >= 1; --r) {
    test_body.push(kExprLocalSet, r);
  }

  // Verify each of the 4 returned s128 values.
  // Each recursive step with depth > 0 increments every lane by 1, so for
  // initial depth D, each lane is incremented (D - 1) times.
  // combined with base = idx * 10 + lane (1..4), expected value for lane L (0..3)
  // is: (idx * 10 + L) + D.
  for (let r = 0; r < kNumReturns; ++r) {
    const paramIdx = return_indices[r];
    const baseOffset = paramIdx * 10;
    test_body.push(
        kExprLocalGet, r + 1,
        kExprLocalGet, 0,
        kExprI32Const, ...wasmSignedLeb(baseOffset),
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32Const, ...wasmSignedLeb(baseOffset + 1),
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32Const, ...wasmSignedLeb(baseOffset + 2),
        kExprI32Add,
        kExprLocalGet, 0,
        kExprI32Const, ...wasmSignedLeb(baseOffset + 3),
        kExprI32Add,
        kExprCallFunction, check_simd.index,
    );
    if (r > 0) {
      test_body.push(kExprI32And);
    }
  }

  builder.addFunction('test', kSig_i_i)
      .addLocals(kWasmS128, kNumReturns)
      .addBody(test_body)
      .exportFunc();

  const js_import = new WebAssembly.Suspending(() => {
    gc();
    return Promise.resolve();
  });

  const instance = builder.instantiate({m: {import: js_import}});
  const wrapper = WebAssembly.promising(instance.exports.test);

  const depth = 430;
  instance.exports.depth.value = depth;

  assertPromiseResult(wrapper(depth), res => assertEquals(res, 1));
})();
