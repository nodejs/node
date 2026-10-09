// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=100

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const array_type = builder.addArray(kWasmExternRef, {mutable: true});

const kNumParamsA = 50;
const sigA = makeSig(new Array(kNumParamsA).fill(kWasmI64), []);
const funcA = builder.addFunction('A', sigA).addBody([]);

const sigBC = makeSig([kWasmI32, wasmRefNullType(array_type)], []);

const bodyB = [];
for (let i = 0; i < kNumParamsA; i++) {
  bodyB.push(...wasmI64Const(0x414141414141n));
}
bodyB.push(kExprCallFunction, funcA.index);
builder.addFunction('B', sigBC).addBody(bodyB).exportFunc();

const kNumParamsBig = 600;
const sigBig = makeSig(new Array(kNumParamsBig).fill(kWasmI64), []);
const funcBig = builder.addFunction('Big', sigBig).addBody([]);

const kNumArrayFills = 15;
const bodyC = [
  kExprLocalGet, 0,
  kExprI32Eqz,
  kExprIf, kWasmVoid,
];
for (let i = 0; i < kNumArrayFills; i++) {
  bodyC.push(
    kExprLocalGet, 1,
    kExprI32Const, 0,
    kExprRefNull, kExternRefCode,
    kExprI32Const, 32,
    kGCPrefix, kExprArrayFill, array_type
  );
}
for (let i = 0; i < kNumParamsBig; i++) {
  bodyC.push(...wasmI64Const(0n));
}
bodyC.push(
  kExprCallFunction, funcBig.index,
  kExprEnd
);

builder.addFunction('C', sigBC)
  .addBody(bodyC)
  .exportFunc();

const instance = builder.instantiate();
const B = instance.exports.B;
const C = instance.exports.C;

%WasmTierUpFunction(B);
%WasmTierUpFunction(C);
Error.stackTraceLimit = 0;

function recurse() {
  try {
    B(1, null);
    recurse();
  } catch (e) {
    B(1, null);
    %SimulateNewspaceFull();
    C(1, null);
  }
}

recurse();
