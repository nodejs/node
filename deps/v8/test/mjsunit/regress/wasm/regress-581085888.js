// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const struct = builder.addStruct([makeField(kWasmAnyRef, true)]);
const id_import = builder.addImport('h', 'id', kSig_i_i);
builder.addMemory64(1, 1);

const kCount = 6;
const kPayload = 0x20000000000n;
for (let i = 0; i <= kCount; ++i) {
  builder.addGlobal(kWasmI32, true, false);
}

const kCounter = kCount + 6;
const kPureCounter = kCounter + 1;
const kInnerCounter = kCounter + 2;
const kShifted = kCounter + 3;
const kRaw = kCounter + 4;
const kFirst = kCounter + 5;
const kReference = kFirst + kCount;

const setup = [];
const touch = [];
const sum = [
  kExprLocalGet, kShifted,
  kExprLocalGet, 4 + kCount, kExprI32ConvertI64, kExprI32Xor,
  kExprLocalGet, 5 + kCount, kExprI32ConvertI64, kExprI32Xor,
];
for (let k = 0; k < kCount; ++k) {
  setup.push(
      kExprLocalGet, 4 + k,
      kExprI32Const, 1,
      kExprI32Add,
      kExprLocalSet, kFirst + k);
  touch.push(
      kExprLocalGet, kFirst + k,
      kExprGlobalSet, k + 1);
  sum.push(
      kExprLocalGet, kFirst + k,
      kExprI32Xor);
}

const sig = makeSig(
    [
      kWasmI32, kWasmI32, kWasmI32, kWasmExternRef,
      ...Array(kCount).fill(kWasmI32), kWasmI64, kWasmI64,
    ],
    [kWasmI32]);

builder.addFunction('test', sig)
    .addLocals(kWasmI64, 1)
    .addLocals(kWasmI32, 3)
    .addLocals(kWasmI64, 1)
    .addLocals(kWasmI32, kCount)
    .addLocals(kWasmAnyRef, 1)
    .addBody([
      ...wasmI64Const(kPayload), kExprLocalSet, kCounter,
      ...setup,
      kExprLocalGet, 4 + kCount,
      kExprLocalGet, 5 + kCount,
      kExprI64Add,
      kExprLocalSet, kRaw,
      kExprLocalGet, kRaw,
      kExprI32LoadMem8U, 0, 0,
      kExprDrop,
      ...touch,
      kExprBlock, kWasmVoid,
        kExprLoop, kWasmVoid,
          kExprLocalGet, kRaw,
          kExprI32ConvertI64,
          kExprLocalGet, 2,
          kExprI32Shl,
          kExprLocalSet, kShifted,
          kExprLocalGet, kRaw,
          kExprI32Const, 0x41,
          kExprI32StoreMem8, 0, 0,
          kExprLocalGet, kCounter,
          ...wasmI64Const(kPayload + 4n),
          kExprI64GeU,
          kExprBrIf, 1,
          kExprLocalGet, 0,
          kExprIf, kWasmVoid,
            ...sum,
            kExprReturn,
          kExprEnd,
          kExprLocalGet, 1,
          kExprLocalSet, kPureCounter,
          kExprBlock, kWasmVoid,
            kExprLoop, kWasmVoid,
              kExprLocalGet, kPureCounter,
              kExprI32Eqz,
              kExprBrIf, 1,
              kExprLocalGet, kPureCounter,
              kExprI32Const, 1,
              kExprI32Sub,
              kExprLocalSet, kPureCounter,
              kExprBr, 0,
            kExprEnd,
          kExprEnd,
          kExprI32Const, 7,
          kGCPrefix, kExprRefI31,
          kExprLocalSet, kReference,
          kExprLoop, kWasmVoid,
            kExprLocalGet, 1,
            kExprLocalSet, kInnerCounter,
            kExprBlock, kWasmVoid,
              kExprLoop, kWasmVoid,
                kExprLocalGet, kInnerCounter,
                kExprI32Eqz,
                kExprBrIf, 1,
                kExprLocalGet, kInnerCounter,
                kExprCallFunction, id_import,
                kExprI32Const, 1,
                kExprI32Sub,
                kExprLocalSet, kInnerCounter,
                kExprBr, 0,
              kExprEnd,
            kExprEnd,
            kExprI32Const, 0,
            kExprGlobalSet, 0,
            kExprGlobalGet, 0,
            kExprIf, kWasmVoid,
              kExprRefNull, kAnyRefCode,
              kGCPrefix, kExprStructNew, struct,
              kExprLocalSet, kReference,
              kExprBr, 1,
            kExprEnd,
          kExprEnd,
          kExprLocalGet, kReference,
          kGCPrefix, kExprRefCast, struct,
          kExprLocalGet, 3,
          kGCPrefix, kExprAnyConvertExtern,
          kGCPrefix, kExprStructSet, struct, 0,
          kExprLocalGet, kCounter,
          kExprI64Const, 1,
          kExprI64Add,
          kExprLocalSet, kCounter,
          kExprBr, 0,
        kExprEnd,
      kExprEnd,
      kExprI32Const, 0,
    ])
    .exportFunc();

const instance = builder.instantiate({h: {id: v => v}});
const args = [...Array(kCount).fill(1), 1n, 1n];
assertEquals(2 << 21, instance.exports.test(1, 0, 21, {}, ...args));
