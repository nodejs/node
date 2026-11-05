// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-growable-stacks --no-wasm-inlining
// Flags: --wasm-wasmfx --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function addActiveLocals(func, numParams, numLocals = 600) {
  func.addLocals(kWasmI64, numLocals + 1);
  const firstLocal = numParams;
  const lastLocal = numParams + numLocals - 1;
  const sumIdx = numParams + numLocals;
  const body = [
    kExprLocalGet, 0,
    kExprI64UConvertI32,
    kExprI64Const, 1,
    kExprI64Add,
    kExprLocalSet, ...wasmUnsignedLeb(firstLocal),
  ];
  for (let i = firstLocal + 1; i <= lastLocal; ++i) {
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(i - 1),
        kExprI64Const, 1,
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(i),
    );
  }
  body.push(kExprI64Const, 0, kExprLocalSet, ...wasmUnsignedLeb(sumIdx));
  for (let i = firstLocal; i <= lastLocal; ++i) {
    body.push(
        kExprLocalGet, ...wasmUnsignedLeb(sumIdx),
        kExprLocalGet, ...wasmUnsignedLeb(i),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(sumIdx),
    );
  }
  body.push(
      kExprLocalGet, ...wasmUnsignedLeb(sumIdx),
      kExprI64Eqz,
      kExprIf, kWasmVoid,
      kExprUnreachable,
      kExprEnd,
  );
  return body;
}

// Test 1: Asymmetric tail calls: growing stack params (delta > 0) and
// shrinking stack params (delta < 0) across segment boundaries.
(function testAsymmetricStackParamsDelta() {
  const builder = new WasmModuleBuilder();

  // Sig small: (depth: i32, a: i32, b: i64, c: f64) -> (i64, f64)
  const sigSmall = builder.addType(
      makeSig([kWasmI32, kWasmI32, kWasmI64, kWasmF64], [kWasmI64, kWasmF64]));

  // Sig large: (depth: i32, a: i32, b: i64, c: f64, p4..p35: i32) -> (i64, f64)
  const largeParams = [kWasmI32, kWasmI32, kWasmI64, kWasmF64];
  for (let i = 4; i < 36; ++i) largeParams.push(kWasmI32);
  const sigLarge = builder.addType(makeSig(largeParams, [kWasmI64, kWasmF64]));

  // f_small (index 0): small frame, calls f_large_caller
  const f_small = builder.addFunction('f_small', sigSmall);
  f_small.addLocals(kWasmI32, 2);
  f_small.addBody([
    kExprLocalGet, 0,  // depth
    kExprI32Eqz,
    kExprIf, kWasmVoid,
    kExprLocalGet, 2,  // return b
    kExprLocalGet, 3,  // return c
    kExprReturn,
    kExprEnd,
    // Call f_large_grower with 36 params
    kExprLocalGet, 0,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kExprLocalGet, 3,
    ...(() => {
      let extra = [];
      for (let i = 4; i < 36; ++i) extra.push(kExprI32Const, i);
      return extra;
    })(),
    kExprCallFunction, 1,  // call f_large_grower (index 1)
  ]);

  // f_large_grower (index 1): large frame (600 locals) forces stack segment
  // allocation, then tail-calls f_small (delta < 0, shrinking stack params
  // from 36 to 4).
  const f_large_grower = builder.addFunction('f_large_grower', sigLarge);
  const growerLocals = addActiveLocals(f_large_grower, 36, 600);
  f_large_grower.addBody([
    ...growerLocals,
    // Verify extra params
    kExprLocalGet, 4,
    kExprI32Const, 4,
    kExprI32Ne,
    kExprIf, kWasmVoid,
    kExprUnreachable,
    kExprEnd,
    kExprLocalGet, 35,
    kExprI32Const, 35,
    kExprI32Ne,
    kExprIf,
    kWasmVoid,
    kExprUnreachable,
    kExprEnd,

    // Tail-call f_small (shrinking stack params)
    kExprLocalGet, 0,  // depth
    kExprLocalGet, 1,  // a
    kExprLocalGet, 2,  // b
    kExprLocalGet, 3,  // c
    kExprReturnCall, 0,  // return_call f_small
  ]);

  builder.addExport('main', 0);
  const instance = builder.instantiate();
  const promising_main = WebAssembly.promising(instance.exports.main);

  assertPromiseResult(promising_main(5, 42, 1000n, 3.14159), res => {
    assertEquals(1000n, res[0]);
    assertEquals(3.14159, res[1]);
  });
})();

// Test 2: Growing stack params (delta > 0) in tail call across segment
// boundaries.
(function testGrowingStackParamsDelta() {
  const builder = new WasmModuleBuilder();

  const sigSmall = builder.addType(makeSig([kWasmI32, kWasmI32], [kWasmI32]));
  const largeParams = [kWasmI32, kWasmI32];
  for (let i = 2; i < 30; ++i) largeParams.push(kWasmI32);
  const sigLarge = builder.addType(makeSig(largeParams, [kWasmI32]));

  // f_consumer (index 0): sums all 30 params
  const f_consumer = builder.addFunction('f_consumer', sigLarge);
  f_consumer.addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprI32Add,
    ...(() => {
      let ops = [];
      for (let i = 2; i < 30; ++i) ops.push(kExprLocalGet, i, kExprI32Add);
      return ops;
    })(),
  ]);

  // f_step (index 1): takes 2 params (sigSmall), large frame forces segment
  // allocation, then tail-calls f_consumer with 30 params (delta > 0).
  const f_step = builder.addFunction('f_step', sigSmall);
  const stepLocals = addActiveLocals(f_step, 2, 1000);
  f_step.addBody([
    ...stepLocals,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    ...(() => {
      let ops = [];
      for (let i = 2; i < 30; ++i) ops.push(kExprI32Const, i);
      return ops;
    })(),
    kExprReturnCall, f_consumer.index,
  ]);

  // f_entry (index 2): takes 2 params (sigSmall), calls f_step
  const f_entry = builder.addFunction('f_entry', sigSmall);
  f_entry.addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprCallFunction,
    f_step.index,
  ]);

  builder.addExport('main', f_entry.index);
  const instance = builder.instantiate();
  const promising_main = WebAssembly.promising(instance.exports.main);

  // Sum of 0..29 = 435. 10 + 20 + 2..29 = 464.
  assertPromiseResult(promising_main(10, 20), res => assertEquals(464, res));
})();

// Test 3: Indirect tail calls (return_call_indirect) across segment boundaries.
(function testIndirectTailCallsAcrossSegments() {
  const builder = new WasmModuleBuilder();

  const paramTypes = [kWasmI32, kWasmI64, kWasmF64];
  for (let i = 3; i < 20; ++i) paramTypes.push(kWasmI32);
  const sig = builder.addType(makeSig(paramTypes, [kWasmI64]));

  const table = builder.addTable(kWasmFuncRef, 2, 2);

  // f_target (index 0)
  const f_target = builder.addFunction('f_target', sig);
  f_target.addBody([
    kExprLocalGet, 0,  // depth
    kExprI32Eqz,
    kExprIf, kWasmVoid,
    kExprLocalGet, 1,  // return i64
    kExprReturn,
    kExprEnd,
    kExprLocalGet, 0,
    kExprI32Const, 1,
    kExprI32Sub,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    ...(() => {
      let ops = [];
      for (let i = 3; i < 20; ++i) ops.push(kExprLocalGet, i);
      return ops;
    })(),
    kExprCallFunction,
    1,  // call f_grow
  ]);

  // f_grow (index 1): large frame, indirect tail call back to f_target
  // (table index 0).
  const f_grow = builder.addFunction('f_grow', sig);
  const growLocals = addActiveLocals(f_grow, 20, 600);
  f_grow.addBody([
    ...growLocals,
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    ...(() => {
      let ops = [];
      for (let i = 3; i < 20; ++i) ops.push(kExprLocalGet, i);
      return ops;
    })(),
    kExprI32Const, 0,  // table index 0
    kExprReturnCallIndirect, sig, table.index,
  ]);

  builder.addActiveElementSegment(
      table.index, [kExprI32Const, 0], [f_target.index, f_grow.index]);
  builder.addExport('main', 0);

  const instance = builder.instantiate();
  const promising_main = WebAssembly.promising(instance.exports.main);
  const args = [5, 777777n, 2.718];
  for (let i = 3; i < 20; ++i) args.push(i * 10);
  assertPromiseResult(
      promising_main(...args), res => assertEquals(777777n, res));
})();

print('test-growable-tail-call-matrix passed!');
