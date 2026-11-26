// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Regression tests for TurboFan WasmLoadElimination::ReduceWasmStructGet using
// the unrefined static field type from WasmFieldInfo instead of the refined
// field type computed from the input struct's subtype.

(function TestSiblingSubtypesAcrossBranch() {
  const builder = new WasmModuleBuilder();
  const $Super =
      builder.addStruct({fields: [makeField(kWasmI32, false)], final: false});
  const $s3 = builder.addStruct({
    fields: [makeField(kWasmI32, false)],
    supertype: $Super,
  });
  const $s4 = builder.addStruct({
    fields: [makeField(kWasmI32, false), makeField(kWasmI32, false)],
    supertype: $Super,
  });

  const $holder1 = builder.addStruct({
    fields: [makeField(wasmRefNullType($Super), false)],
    final: false,
  });
  const $holder_s3 = builder.addStruct({
    fields: [makeField(wasmRefNullType($s3), false)],
    supertype: $holder1,
  });
  const $holder_s4 = builder.addStruct({
    fields: [makeField(wasmRefNullType($s4), false)],
    supertype: $holder1,
  });

  builder.addFunction('make_h4', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprRefNull, $s4,
        kGCPrefix, kExprStructNew, $holder_s4,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('make_h3', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprI32Const, 42,
        kGCPrefix, kExprStructNew, $s3,
        kGCPrefix, kExprStructNew, $holder_s3,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('f1', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_s4,
        kGCPrefix, kExprStructGet, $holder_s4, 0,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('touch', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder1,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('f2', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_s3,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprRefCast, $s4,
        kGCPrefix, kExprStructGet, $s4, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {make_h4, make_h3, f1, touch, f2} = instance.exports;
  const h4 = make_h4();
  const h3 = make_h3();

  function inner(x, y, z, cond) {
    f1(x);
    if (cond) {
      touch(z);
    }
    return f2(y);
  }

  function outer(x, z, cond) {
    return inner(x, x, z, cond);
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(outer);
  assertTraps(kTrapIllegalCast, () => inner(h4, h3, h4, true));
  assertTraps(kTrapIllegalCast, () => inner(h4, h3, h4, false));
  assertTraps(kTrapIllegalCast, () => outer(h4, h4, false));
  %OptimizeFunctionOnNextCall(outer);
  assertTraps(kTrapIllegalCast, () => outer(h4, h4, false));
})();

(function TestCovariantFieldWithCastOnCachedLoad() {
  const builder = new WasmModuleBuilder();
  const $Super =
      builder.addStruct({fields: [makeField(kWasmI32, false)], final: false});
  const $s3 = builder.addStruct({
    fields: [makeField(kWasmI32, false)],
    supertype: $Super,
  });
  const $s4 = builder.addStruct({
    fields: [makeField(kWasmI32, false), makeField(kWasmI32, false)],
    supertype: $Super,
  });

  const $holder1 = builder.addStruct({
    fields: [makeField(wasmRefNullType($Super), false)],
    final: false,
  });
  const $holder_s3 = builder.addStruct({
    fields: [makeField(wasmRefNullType($s3), false)],
    supertype: $holder1,
  });

  builder.addFunction('make_h1_null', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprRefNull, $Super,
        kGCPrefix, kExprStructNew, $holder1,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('make_h3_null', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprRefNull, $s3,
        kGCPrefix, kExprStructNew, $holder_s3,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('f1', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder1,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprRefCastNull, $s4,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('f2', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_s3,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprRefCast, $s4,
        kGCPrefix, kExprStructGet, $s4, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {make_h1_null, make_h3_null, f1, f2} = instance.exports;
  const h1 = make_h1_null();
  const h3 = make_h3_null();

  function inner(x, y) {
    f1(x);
    return f2(y);
  }

  function outer(x) {
    return inner(x, x);
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(outer);
  assertTraps(kTrapIllegalCast, () => inner(h1, h3));
  assertTraps(kTrapIllegalCast, () => outer(h3));
  %OptimizeFunctionOnNextCall(outer);
  assertTraps(kTrapIllegalCast, () => outer(h3));
})();

(function TestCovariantNullRefField() {
  const builder = new WasmModuleBuilder();
  const $Super =
      builder.addStruct({fields: [makeField(kWasmI32, false)], final: false});
  const $holder1 = builder.addStruct({
    fields: [makeField(wasmRefNullType($Super), false)],
    final: false,
  });
  const $holder_none = builder.addStruct({
    fields: [makeField(kWasmNullRef, false)],
    supertype: $holder1,
  });

  builder.addFunction('make_h_none', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprRefNull, kNullRefCode,
        kGCPrefix, kExprStructNew, $holder_none,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('f1', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder1,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('f2', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_none,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprStructGet, $Super, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {make_h_none, f1, f2} = instance.exports;
  const h = make_h_none();

  function outer(x) {
    f1(x);
    return f2(x);
  }

  %PrepareFunctionForOptimization(outer);
  assertTraps(kTrapNullDereference, () => outer(h));
  assertTraps(kTrapNullDereference, () => outer(h));
  %OptimizeFunctionOnNextCall(outer);
  assertTraps(kTrapNullDereference, () => outer(h));
})();

(function TestCovariantUninhabitedField() {
  const builder = new WasmModuleBuilder();
  const $Super =
      builder.addStruct({fields: [makeField(kWasmI32, false)], final: false});
  const $holder1 = builder.addStruct({
    fields: [makeField(wasmRefNullType($Super), false)],
    final: false,
  });
  const $holder_ref_none = builder.addStruct({
    fields: [makeField(wasmRefType(kWasmNullRef), false)],
    supertype: $holder1,
  });

  builder.addFunction('make_h1', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprRefNull, $Super,
        kGCPrefix, kExprStructNew, $holder1,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('f1', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder1,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('f2', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_ref_none,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprStructGet, $Super, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {make_h1, f1, f2} = instance.exports;
  const h1 = make_h1();

  function outer(x) {
    f1(x);
    return f2(x);
  }

  %PrepareFunctionForOptimization(outer);
  assertTraps(kTrapIllegalCast, () => outer(h1));
  assertTraps(kTrapIllegalCast, () => outer(h1));
  %OptimizeFunctionOnNextCall(outer);
  assertTraps(kTrapIllegalCast, () => outer(h1));
})();

(function TestCovariantFieldValidExecution() {
  const builder = new WasmModuleBuilder();
  const $Super =
      builder.addStruct({fields: [makeField(kWasmI32, false)], final: false});
  const $s3 = builder.addStruct({
    fields: [makeField(kWasmI32, false), makeField(kWasmI32, false)],
    supertype: $Super,
  });
  const $holder1 = builder.addStruct({
    fields: [makeField(wasmRefNullType($Super), false)],
    final: false,
  });
  const $holder_s3 = builder.addStruct({
    fields: [makeField(wasmRefNullType($s3), false)],
    supertype: $holder1,
  });

  builder.addFunction('make_h3', makeSig([], [kWasmExternRef]))
      .addBody([
        kExprI32Const, 7,
        kExprI32Const, 42,
        kGCPrefix, kExprStructNew, $s3,
        kGCPrefix, kExprStructNew, $holder_s3,
        kGCPrefix, kExprExternConvertAny,
      ])
      .exportFunc();

  builder.addFunction('f1', makeSig([kWasmExternRef], []))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder1,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kExprDrop,
      ])
      .exportFunc();

  builder.addFunction('f2', makeSig([kWasmExternRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprAnyConvertExtern,
        kGCPrefix, kExprRefCastNull, $holder_s3,
        kGCPrefix, kExprStructGet, $holder1, 0,
        kGCPrefix, kExprRefCast, $s3,
        kGCPrefix, kExprStructGet, $s3, 1,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const {make_h3, f1, f2} = instance.exports;
  const h3 = make_h3();

  function outer(x) {
    f1(x);
    return f2(x);
  }

  %PrepareFunctionForOptimization(outer);
  assertEquals(42, outer(h3));
  assertEquals(42, outer(h3));
  %OptimizeFunctionOnNextCall(outer);
  assertEquals(42, outer(h3));
})();
