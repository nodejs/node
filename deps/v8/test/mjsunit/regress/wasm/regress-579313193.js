// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestImmutableUnrelatedPackedStructGet() {
  const builder = new WasmModuleBuilder();
  const superStruct = builder.addStruct([]);
  const structF64 = builder.addStruct(
      {fields: [makeField(kWasmF64, false)], supertype: superStruct});
  const structI8 = builder.addStruct(
      {fields: [makeField(kWasmI8, false)], supertype: superStruct});
  const structI16 = builder.addStruct(
      {fields: [makeField(kWasmI16, false)], supertype: superStruct});

  builder.addFunction('makeStructF64', makeSig([kWasmF64], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, structF64,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  for (const [name, targetStruct, getOp] of [
           ['getI8S', structI8, kExprStructGetS],
           ['getI8U', structI8, kExprStructGetU],
           ['getI16S', structI16, kExprStructGetS],
           ['getI16U', structI16, kExprStructGetU],
       ]) {
    builder.addFunction(name, makeSig([kWasmExternRef], [kWasmI32]))
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprAnyConvertExtern,
          kGCPrefix, kExprRefCast, superStruct,
          kExprLocalGet, 0,
          kGCPrefix, kExprAnyConvertExtern,
          kGCPrefix, kExprRefCast, structF64,
          kGCPrefix, kExprStructGet, structF64, 0,
          kExprDrop,
          kGCPrefix, kExprRefCast, targetStruct,
          kGCPrefix, getOp, targetStruct, 0,
        ])
        .exportFunc();
  }

  const instance = builder.instantiate();
  const s = instance.exports.makeStructF64(1.5);

  for (const fn of [
           instance.exports.getI8S,
           instance.exports.getI8U,
           instance.exports.getI16S,
           instance.exports.getI16U,
       ]) {
    const caller = (x) => fn(x) + 1;
    %PrepareFunctionForOptimization(caller);
    for (let i = 0; i < 3; i++) {
      assertTraps(kTrapIllegalCast, () => caller(s));
    }
    %OptimizeFunctionOnNextCall(caller);
    assertTraps(kTrapIllegalCast, () => caller(s));
  }
})();

(function TestMutableUnrelatedStructSetToPackedGet() {
  const builder = new WasmModuleBuilder();
  const superStruct = builder.addStruct([]);
  const structF64 = builder.addStruct(
      {fields: [makeField(kWasmF64, true)], supertype: superStruct});
  const structF32 = builder.addStruct(
      {fields: [makeField(kWasmF32, true)], supertype: superStruct});
  const structI8 = builder.addStruct(
      {fields: [makeField(kWasmI8, true)], supertype: superStruct});
  const structI16 = builder.addStruct(
      {fields: [makeField(kWasmI16, true)], supertype: superStruct});

  builder.addFunction('makeStructF64', makeSig([kWasmF64], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, structF64,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('makeStructF32', makeSig([kWasmF32], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, structF32,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('setF64GetI8S',
                      makeSig([kWasmExternRef, kWasmF64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, superStruct,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, structF64,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructSet, structF64, 0,
        kGCPrefix, kExprRefCast, structI8,
        kGCPrefix, kExprStructGetS, structI8, 0,
      ])
      .exportFunc();

  builder.addFunction('setF32GetI16U',
                      makeSig([kWasmExternRef, kWasmF32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, superStruct,
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, structF32,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructSet, structF32, 0,
        kGCPrefix, kExprRefCast, structI16,
        kGCPrefix, kExprStructGetU, structI16, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const sF64 = instance.exports.makeStructF64(2.5);
  const sF32 = instance.exports.makeStructF32(1.25);

  const callerF64 = (x) => instance.exports.setF64GetI8S(x, 3.5) + 1;
  %PrepareFunctionForOptimization(callerF64);
  for (let i = 0; i < 3; i++) {
    assertTraps(kTrapIllegalCast, () => callerF64(sF64));
  }
  %OptimizeFunctionOnNextCall(callerF64);
  assertTraps(kTrapIllegalCast, () => callerF64(sF64));

  const callerF32 = (x) => instance.exports.setF32GetI16U(x, 2.25) + 1;
  %PrepareFunctionForOptimization(callerF32);
  for (let i = 0; i < 3; i++) {
    assertTraps(kTrapIllegalCast, () => callerF32(sF32));
  }
  %OptimizeFunctionOnNextCall(callerF32);
  assertTraps(kTrapIllegalCast, () => callerF32(sF32));
})();

(function TestReachablePackedLoadElimination() {
  const builder = new WasmModuleBuilder();
  const structPacked = builder.addStruct([
    makeField(kWasmI8, true),
    makeField(kWasmI16, true),
  ]);

  builder.addFunction('makePacked', makeSig([], [kWasmExternRef]))
      .addBody([
        kGCPrefix, kExprStructNewDefault, structPacked,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  for (const [name, fieldIdx, getOp] of [
           ['setGetI8S', 0, kExprStructGetS],
           ['setGetI8U', 0, kExprStructGetU],
           ['setGetI16S', 1, kExprStructGetS],
           ['setGetI16U', 1, kExprStructGetU],
       ]) {
    builder.addFunction(name, makeSig([kWasmExternRef, kWasmI32], [kWasmI32]))
        .addBody([
          kExprLocalGet, 0,
          kGCPrefix, kExprAnyConvertExtern,
          kGCPrefix, kExprRefCast, structPacked,
          kExprLocalGet, 1,
          kGCPrefix, kExprStructSet, structPacked, fieldIdx,
          kExprLocalGet, 0,
          kGCPrefix, kExprAnyConvertExtern,
          kGCPrefix, kExprRefCast, structPacked,
          kGCPrefix, getOp, structPacked, fieldIdx,
        ])
        .exportFunc();
  }

  const instance = builder.instantiate();
  const s = instance.exports.makePacked();
  const {setGetI8S, setGetI8U, setGetI16S, setGetI16U} = instance.exports;

  function roundtrip(ref, v) {
    return [
      setGetI8S(ref, v),
      setGetI8U(ref, v),
      setGetI16S(ref, v),
      setGetI16U(ref, v),
    ];
  }

  %PrepareFunctionForOptimization(roundtrip);
  for (let i = 0; i < 3; i++) {
    assertEquals([-1, 0xff, -1, 0xffff], roundtrip(s, -1));
  }
  %OptimizeFunctionOnNextCall(roundtrip);
  assertEquals([0, 0, 0, 0], roundtrip(s, 0));
  assertEquals([-1, 0xff, -1, 0xffff], roundtrip(s, -1));
  assertEquals([-0x80, 0x80, 0x1280, 0x1280], roundtrip(s, 0x11280));
  assertEquals([0x7f, 0x7f, -0x7f81, 0x807f], roundtrip(s, 0x1807f));
})();
