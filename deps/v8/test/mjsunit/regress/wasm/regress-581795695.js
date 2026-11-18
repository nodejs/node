// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const sigCallee = builder.addType(kSig_i_i);
const sigMain = builder.addType(makeSig(
    [
      kWasmI32, kWasmI32, kWasmI32, kWasmI32, kWasmI32,
      wasmRefNullType(sigCallee), wasmRefNullType(sigCallee),
    ],
    [kWasmI32]));

const g0 = builder.addGlobal(kWasmI32, true, false, wasmI32Const(0));

builder.addFunction('callee1', sigCallee)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
    .exportFunc();

builder.addFunction('callee2', sigCallee)
    .addBody([kExprLocalGet, 0, kExprI32Const, 10, kExprI32Add])
    .exportFunc();

// Exceed the number of allocatable general-purpose registers on all platforms.
const kNumLocals = 32;
const kFirstI32Local = 7;
const kFirstI64Local = kFirstI32Local + kNumLocals;

const body = [kExprI32Const, 1, kExprGlobalSet, g0.index];
let expectedSum = 0n;
for (let i = 0; i < kNumLocals; i++) {
  const val = 0x41414100 + i;
  expectedSum += BigInt(val);
  body.push(...wasmI32Const(val), kExprLocalSet, kFirstI32Local + i);
}

body.push(kExprBlock, kWasmVoid, kExprLoop, kWasmVoid);
for (let i = 0; i < kNumLocals; i++) {
  body.push(
      kExprLocalGet, kFirstI32Local + i,
      kExprI64UConvertI32,
      kExprLocalSet, kFirstI64Local + i);
}
body.push(
    kExprGlobalGet, g0.index,
    kExprBrIf, 1,
    kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
        kExprLocalGet, 0,
        kExprBrIf, 1,
        kExprBr, 0,
      kExprEnd,
    kExprEnd);
for (let i = 0; i < kNumLocals; i++) {
  body.push(
      kExprLocalGet, kFirstI32Local + i,
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalSet, kFirstI32Local + i);
}
body.push(kExprBr, 0, kExprEnd, kExprEnd);

body.push(
    kExprLocalGet, 0,
    kExprLocalGet, 6,
    kExprCallRef, sigCallee,
    kExprLocalGet, 6,
    kExprCallRef, sigCallee,
    kExprI64Const, 0);
for (let i = 0; i < kNumLocals; i++) {
  body.push(kExprLocalGet, kFirstI64Local + i, kExprI64Add);
}
body.push(kExprI32ConvertI64, kExprI32Add);

builder.addFunction('main', sigMain)
    .addLocals(kWasmI32, kNumLocals)
    .addLocals(kWasmI64, kNumLocals)
    .addBody(body)
    .exportFunc();

const {main, callee1, callee2} = builder.instantiate().exports;
const baseSumI32 = Number(BigInt.asIntN(32, expectedSum));

assertEquals((baseSumI32 + 2) | 0, main(0, 0, 0, 0, 0, callee1, callee1));
%WasmTierUpFunction(main);
assertEquals((baseSumI32 + 2) | 0, main(0, 0, 0, 0, 0, callee1, callee1));
// Trigger speculative call_ref deoptimization and check FrameState constants.
assertEquals((baseSumI32 + 20) | 0, main(0, 0, 0, 0, 0, callee1, callee2));
