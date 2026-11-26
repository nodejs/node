// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-custom-descriptors

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Dominating inexact ref.get_desc must not be GVN'd with a dominated exact
// ref.get_desc after br_on_cast narrows the operand to an exact type.
const builder = new WasmModuleBuilder();

builder.startRecGroup();
const s0 = builder.nextTypeIndex();
const d0 = s0 + 1;
const s1 = s0 + 2;
const d1 = s0 + 3;
builder.addStruct({fields: [makeField(kWasmI32, true)], descriptor: d0});
builder.addStruct({fields: [makeField(kWasmI32, true)], describes: s0});
builder.addStruct({
  fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
  supertype: s0,
  descriptor: d1,
  final: true,
});
builder.addStruct({
  fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
  supertype: d0,
  describes: s1,
  final: true,
});
builder.endRecGroup();

const refS0 = wasmRefType(s0);
const refExactS0 = wasmRefType(s0).exact();
const refNullD0 = wasmRefNullType(d0);
const refNullExactD0 = wasmRefNullType(d0).exact();
const blockExactS0 = builder.addType(makeSig([], [refExactS0]));

builder.addFunction('makeS0', makeSig([kWasmI32], [refS0]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprI32Const, 10,
      kGCPrefix, kExprStructNew, d0,
      kGCPrefix, kExprStructNewDesc, s0,
    ]);

builder.addFunction('makeS1', makeSig([kWasmI32], [refS0]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprI32Const, 1,
      kExprI32Const, 20,
      kExprI32Const, 30,
      kGCPrefix, kExprStructNew, d1,
      kGCPrefix, kExprStructNewDesc, s1,
    ]);

builder.addFunction('castAfterNarrow', makeSig([refS0], [kWasmI32]))
    .addLocals(refNullD0, 1)
    .addLocals(refNullExactD0, 1)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefGetDesc, s0,
      kExprLocalSet, 1,
      kExprBlock, blockExactS0,
        kExprLocalGet, 0,
        ...wasmBrOnCast(0, refS0, refExactS0),
        kExprDrop,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, d0, 0,
        kExprReturn,
      kExprEnd,
      kGCPrefix, kExprRefGetDesc, s0,
      kExprLocalTee, 2,
      kGCPrefix, kExprRefCastNull, d1,
      kExprDrop,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructGet, d0, 0,
    ]);

builder.addFunction('testAfterNarrow', makeSig([refS0], [kWasmI32]))
    .addLocals(refNullD0, 1)
    .addLocals(refNullExactD0, 1)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprRefGetDesc, s0,
      kExprLocalSet, 1,
      kExprBlock, blockExactS0,
        kExprLocalGet, 0,
        ...wasmBrOnCast(0, refS0, refExactS0),
        kExprDrop,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructGet, d0, 0,
        kExprReturn,
      kExprEnd,
      kGCPrefix, kExprRefGetDesc, s0,
      kExprLocalTee, 2,
      kGCPrefix, kExprRefTestNull, d1,
      kExprLocalGet, 0,
      kExprLocalGet, 2,
      kExprRefAsNonNull,
      kGCPrefix, kExprRefCastDescEq, kWasmExact, s0,
      kGCPrefix, kExprStructGet, s0, 0,
      kExprI32Add,
    ]);

const wasm = builder.instantiate().exports;
const obj0 = wasm.makeS0(42);
const obj1 = wasm.makeS1(99);

assertEquals(20, wasm.castAfterNarrow(obj1));
assertTraps(kTrapIllegalCast, () => wasm.castAfterNarrow(obj0));

assertEquals(42, wasm.testAfterNarrow(obj0));
assertEquals(20, wasm.testAfterNarrow(obj1));
