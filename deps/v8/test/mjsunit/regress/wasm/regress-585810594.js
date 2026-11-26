// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const s0 =
    builder.addStruct({fields: [makeField(kWasmI32, true)], final: false});
const s1 = builder.addStruct({
  fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
  supertype: s0,
  final: true,
});

const refS0 = wasmRefType(s0);
const refNullS0 = wasmRefNullType(s0);
const refNullS1 = wasmRefNullType(s1);
const blkRefS0 = builder.addType(makeSig([], [refS0]));
const blkRefNullS0 = builder.addType(makeSig([], [refNullS0]));
const blkRefNullS1 = builder.addType(makeSig([], [refNullS1]));

builder.addFunction('makeS0', makeSig([kWasmI32], [kWasmEqRef]))
    .exportFunc()
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, s0]);

builder.addFunction('makeS1', makeSig([kWasmI32, kWasmI32], [kWasmEqRef]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructNew, s1,
    ]);

// 1. br_on_null fallthrough narrowing after loop unrolling turns the GVN'd
//    IsNull condition into a Phi.
builder.addFunction('brOnNull', makeSig([kWasmEqRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(refNullS0, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, kWasmVoid,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, s0,
          kExprLocalTee, 2,
          kExprBlock, kWasmVoid,
            kExprLocalGet, 2,
            kExprBrOnNull, 0,
            kExprDrop,
          kExprEnd,
          kExprDrop,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        kExprBrOnNull, 0,
        kGCPrefix, kExprRefCast, s1,
        kGCPrefix, kExprStructGet, s1, 1,
        kExprReturn,
      kExprEnd,
      ...wasmI32Const(-1),
    ]);

// 2. br_on_non_null taken-branch narrowing with a Phi IsNull condition.
builder.addFunction('brOnNonNull', makeSig([kWasmEqRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(refNullS0, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, blkRefS0,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, s0,
          kExprLocalTee, 2,
          kExprBlock, kWasmVoid,
            kExprLocalGet, 2,
            kExprBrOnNull, 0,
            kExprDrop,
          kExprEnd,
          kExprDrop,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        kExprBrOnNonNull, 0,
        ...wasmI32Const(-1),
        kExprReturn,
      kExprEnd,
      kGCPrefix, kExprRefCast, s1,
      kGCPrefix, kExprStructGet, s1, 1,
    ]);

// 3. br_on_cast taken-branch narrowing with a Phi WasmTypeCheck condition.
builder.addFunction('brOnCastTaken', makeSig([kWasmAnyRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(kWasmEqRef, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, blkRefS0,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, kEqRefCode,
          kExprLocalTee, 2,
          kGCPrefix, kExprRefTest, s0,
          kExprIf, kWasmVoid,
          kExprEnd,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        ...wasmBrOnCast(0, wasmRefNullType(kWasmEqRef), refS0),
        kExprDrop,
        ...wasmI32Const(-1),
        kExprReturn,
      kExprEnd,
      kGCPrefix, kExprRefCast, s1,
      kGCPrefix, kExprStructGet, s1, 1,
    ]);

// 4. br_on_cast fallthrough non-null narrowing with a Phi WasmTypeCheck.
builder.addFunction('brOnCastFallthrough', makeSig([kWasmEqRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(refNullS0, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, blkRefNullS1,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, s0,
          kExprLocalTee, 2,
          kGCPrefix, kExprRefTestNull, s1,
          kExprIf, kWasmVoid,
          kExprEnd,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        ...wasmBrOnCast(0, refNullS0, refNullS1),
        // Fallthrough is statically (ref $s0).
        kGCPrefix, kExprRefTest, s1,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 2,
    ]);

// 5. br_on_cast_fail taken-branch non-null narrowing with a Phi
//    WasmTypeCheck.
builder.addFunction('brOnCastFailTaken', makeSig([kWasmEqRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(refNullS0, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, blkRefS0,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, s0,
          kExprLocalTee, 2,
          kGCPrefix, kExprRefTestNull, s1,
          kExprIf, kWasmVoid,
          kExprEnd,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        ...wasmBrOnCastFail(0, refNullS0, refNullS1),
        kExprDrop,
        kExprI32Const, 2,
        kExprReturn,
      kExprEnd,
      // Taken branch is statically (ref $s0).
      kGCPrefix, kExprRefTest, s1,
    ]);

// 6. br_on_cast_fail where the heap type check always succeeds and only null
//    takes the branch (decoder lowers this via Forward + BrOnNull).
builder.addFunction('brOnCastFailAlwaysSucceeds',
                    makeSig([kWasmEqRef], [kWasmI32]))
    .addLocals(kWasmI32, 1)
    .addLocals(refNullS0, 1)
    .exportFunc()
    .addBody([
      ...wasmI32Const(100),
      kExprLocalSet, 1,
      kExprBlock, blkRefNullS0,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kGCPrefix, kExprRefCastNull, s0,
          kExprLocalTee, 2,
          kExprBlock, kWasmVoid,
            kExprLocalGet, 2,
            kExprBrOnNull, 0,
            kExprDrop,
          kExprEnd,
          kExprDrop,
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 0,
        kExprEnd,
        kExprLocalGet, 2,
        ...wasmBrOnCastFail(0, refNullS0, refS0),
        // Fallthrough is statically (ref $s0).
        kGCPrefix, kExprRefCast, s1,
        kGCPrefix, kExprStructGet, s1, 1,
        kExprReturn,
      kExprEnd,
      kExprRefIsNull,
      kExprIf, kWasmI32,
        ...wasmI32Const(-1),
      kExprElse,
        ...wasmI32Const(-2),
      kExprEnd,
    ]);

const wasm = builder.instantiate().exports;
const obj0 = wasm.makeS0(10);
const obj1 = wasm.makeS1(10, 42);

assertEquals(-1, wasm.brOnNull(null));
assertEquals(42, wasm.brOnNull(obj1));
assertTraps(kTrapIllegalCast, () => wasm.brOnNull(obj0));

assertEquals(-1, wasm.brOnNonNull(null));
assertEquals(42, wasm.brOnNonNull(obj1));
assertTraps(kTrapIllegalCast, () => wasm.brOnNonNull(obj0));

assertEquals(-1, wasm.brOnCastTaken(null));
assertEquals(42, wasm.brOnCastTaken(obj1));
assertTraps(kTrapIllegalCast, () => wasm.brOnCastTaken(obj0));

assertEquals(2, wasm.brOnCastFallthrough(null));
assertEquals(2, wasm.brOnCastFallthrough(obj1));
assertEquals(0, wasm.brOnCastFallthrough(obj0));

assertEquals(2, wasm.brOnCastFailTaken(null));
assertEquals(2, wasm.brOnCastFailTaken(obj1));
assertEquals(0, wasm.brOnCastFailTaken(obj0));

assertEquals(-1, wasm.brOnCastFailAlwaysSucceeds(null));
assertEquals(42, wasm.brOnCastFailAlwaysSucceeds(obj1));
assertTraps(kTrapIllegalCast, () => wasm.brOnCastFailAlwaysSucceeds(obj0));
