// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestDeadValueBehindCastChain() {
  const builder = new WasmModuleBuilder();
  const superStruct = builder.addStruct([]);
  const immutStruct = builder.addStruct(
      {fields: [makeField(kWasmI32, false)], supertype: superStruct});
  const mutStruct1 = builder.addStruct(
      {fields: [makeField(kWasmI32, true)], supertype: superStruct});
  const mutStruct2 = builder.addStruct(
      {fields: [makeField(kWasmI32, true)], supertype: superStruct});

  builder.addFunction('makeMut2', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprI32Const, 0,
        kGCPrefix, kExprStructNew, mutStruct2,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('makeImmut', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprI32Const, 1,
        kGCPrefix, kExprStructNew, immutStruct,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('makeMut1', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprI32Const, 2,
        kGCPrefix, kExprStructNew, mutStruct1,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('castSuper', makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, superStruct,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('getImmut', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, immutStruct,
        kGCPrefix, kExprStructGet, immutStruct, 0,
      ])
      .exportFunc();

  builder.addFunction('getMut1', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, mutStruct1,
        kGCPrefix, kExprStructGet, mutStruct1, 0,
      ])
      .exportFunc();

  builder.addFunction(
             'getThenSetMut2',
             makeSig([kWasmExternRef, kWasmExternRef, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, mutStruct2,
        kGCPrefix, kExprStructGet, mutStruct2, 0,
        kExprDrop,
        kExprLocalGet, 1,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, superStruct,
        kGCPrefix, kExprRefCast, mutStruct2,
        kExprLocalGet, 2,
        kGCPrefix, kExprStructSet, mutStruct2, 0,
      ])
      .exportFunc();

  builder.addFunction('getImmutThenMut1',
                      makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, immutStruct,
        kGCPrefix, kExprStructGet, immutStruct, 0,
        kExprDrop,
        kExprLocalGet, 1,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, mutStruct1,
        kGCPrefix, kExprStructGet, mutStruct1, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {
    makeMut2,
    makeImmut,
    makeMut1,
    castSuper,
    getImmut,
    getMut1,
    getThenSetMut2,
    getImmutThenMut1,
  } = instance.exports;

  const mut2 = makeMut2();
  const immut = makeImmut();
  const mut1 = makeMut1();

  function helper(p1, p2, c1, n, n2, c2, c3, c4) {
    let x = 0;
    let sup = castSuper(p1);
    let q = c1 ? sup : p2;
    for (let k = 0; k < n; k++) {
      // Inner loop prevents outer loop peeling so the outer loop EffectPhi
      // delays WasmLoadElimination until the second pass.
      for (let m = 0; m < n2; m++) {
        x += 1;
      }
      getImmut(p1);
      let a = getMut1(q);
      let v = a ^ 1;
      if (c2) { x += 1; }
      if (c3) { x += 2; }
      if (c4) { x += 3; }
      getThenSetMut2(mut2, c1 ? a : mut2, v);
      getImmutThenMut1(p1, c1 ? sup : p2);
      q = p2;
    }
    return x;
  }

  function jsFunc(p1, p2, n, n2, c2, c3, c4) {
    return helper(p1, p2, true, n, n2, c2, c3, c4);
  }

  %PrepareFunctionForOptimization(helper);
  %PrepareFunctionForOptimization(jsFunc);
  for (let i = 0; i < 8; i++) {
    const c2 = (i & 1) !== 0;
    const c3 = (i & 2) !== 0;
    const c4 = (i & 4) !== 0;
    const expected = 2 * (1 + (c2 ? 1 : 0) + (c3 ? 2 : 0) + (c4 ? 3 : 0));
    assertEquals(expected, helper(immut, mut1, false, 2, 1, c2, c3, c4));
    assertTraps(kTrapIllegalCast, () => jsFunc(immut, mut1, 2, 1, c2, c3, c4));
  }
  %OptimizeFunctionOnNextCall(jsFunc);
  assertTraps(kTrapIllegalCast,
              () => jsFunc(immut, mut1, 2, 1, true, true, true));
})();

(function TestNullableSubAliasingNonNullSuper() {
  const builder = new WasmModuleBuilder();
  const superStruct = builder.addStruct([makeField(kWasmI32, true)]);
  const subStruct = builder.addStruct(
      {fields: [makeField(kWasmI32, true)], supertype: superStruct});
  const holderNullSub = builder.addStruct(
      [makeField(wasmRefNullType(subStruct), false)]);
  const holderNonNullSuper = builder.addStruct(
      [makeField(wasmRefType(superStruct), false)]);

  builder.addFunction('makeSub', makeSig([kWasmI32], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, subStruct,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('makeNullSubHolder',
                      makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, subStruct,
        kGCPrefix, kExprStructNew, holderNullSub,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('makeNonNullSuperHolder',
                      makeSig([kWasmExternRef], [kWasmExternRef]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, superStruct,
        kGCPrefix, kExprStructNew, holderNonNullSuper,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('getSubField', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, holderNullSub,
        kGCPrefix, kExprStructGet, holderNullSub, 0,
        kGCPrefix, kExprStructGet, subStruct, 0,
      ])
      .exportFunc();

  builder.addFunction('setSubField', makeSig([kWasmExternRef, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, holderNullSub,
        kGCPrefix, kExprStructGet, holderNullSub, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructSet, subStruct, 0,
      ])
      .exportFunc();

  builder.addFunction('getSuperField', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, holderNonNullSuper,
        kGCPrefix, kExprStructGet, holderNonNullSuper, 0,
        kGCPrefix, kExprStructGet, superStruct, 0,
      ])
      .exportFunc();

  builder.addFunction('setSuperField', makeSig([kWasmExternRef, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCast, holderNonNullSuper,
        kGCPrefix, kExprStructGet, holderNonNullSuper, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructSet, superStruct, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {
    makeSub,
    makeNullSubHolder,
    makeNonNullSuperHolder,
    getSubField,
    setSubField,
    getSuperField,
    setSuperField,
  } = instance.exports;

  const obj = makeSub(10);
  const hNullSub = makeNullSubHolder(obj);
  const hNonNullSuper = makeNonNullSuperHolder(obj);

  function readSubWriteSuper(h1, h2, v) {
    const before = getSubField(h1);
    setSuperField(h2, v);
    const after = getSubField(h1);
    return before * 1000 + after;
  }

  function readSuperWriteSub(h1, h2, v) {
    const before = getSuperField(h1);
    setSubField(h2, v);
    const after = getSuperField(h1);
    return before * 1000 + after;
  }

  %PrepareFunctionForOptimization(readSubWriteSuper);
  %PrepareFunctionForOptimization(readSuperWriteSub);
  setSuperField(hNonNullSuper, 10);
  assertEquals(10099, readSubWriteSuper(hNullSub, hNonNullSuper, 99));
  setSuperField(hNonNullSuper, 10);
  assertEquals(10077, readSuperWriteSub(hNonNullSuper, hNullSub, 77));

  %OptimizeFunctionOnNextCall(readSubWriteSuper);
  %OptimizeFunctionOnNextCall(readSuperWriteSub);
  setSuperField(hNonNullSuper, 10);
  assertEquals(10099, readSubWriteSuper(hNullSub, hNonNullSuper, 99));
  setSuperField(hNonNullSuper, 10);
  assertEquals(10077, readSuperWriteSub(hNonNullSuper, hNullSub, 77));
})();
