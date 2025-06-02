// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestAtomicStructGetSet() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let anyRefT = is_shared
      ? wasmRefNullType(kWasmAnyRef).shared()
      : wasmRefNullType(kWasmAnyRef);
    let struct = builder.addStruct({
      fields: [
        // DO NOT REORDER OR INSERT EXTRA FIELDS IN BETWEEN!
        // The i64 is intentionally "unaligned".
        makeField(kWasmI32, true),
        makeField(kWasmI64, true),
        makeField(anyRefT, true)
      ],
      is_shared,
    });
    let producer_sig = makeSig(
      [kWasmI32, kWasmI64, anyRefT],
      [wasmRefType(struct)]);
    builder.addFunction("newStruct", producer_sig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kGCPrefix, kExprStructNew, struct])
      .exportFunc();
    builder.addFunction("atomicGet32",
        makeSig([wasmRefNullType(struct)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicGet64",
        makeSig([wasmRefNullType(struct)], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicGetRef", makeSig(
        [wasmRefNullType(struct)],
        [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicGetRefRef", makeSig(
        [wasmRefNullType(struct)],
        [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 2,
        kGCPrefix, kExprRefCast, struct,
        kAtomicPrefix, kExprStructAtomicGet, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicSet32",
        makeSig([wasmRefNullType(struct), kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicSet64",
        makeSig([wasmRefNullType(struct), kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicSetRef",
        makeSig([wasmRefNullType(struct), anyRefT], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let structObj = wasm.newStruct(42, -64n, "test");
    assertEquals(42, wasm.atomicGet32(structObj));
    wasm.atomicSet32(structObj, 123);
    assertEquals(123, wasm.atomicGet32(structObj));
    assertEquals(-64n, wasm.atomicGet64(structObj));
    wasm.atomicSet64(structObj, 456n);
    assertEquals(456n, wasm.atomicGet64(structObj));
    assertEquals("test", wasm.atomicGetRef(structObj));
    let structStruct = wasm.newStruct(1, 2n, structObj);
    assertEquals("test", wasm.atomicGetRefRef(structStruct));
    wasm.atomicSetRef(structObj, "another string");
    assertEquals("another string", wasm.atomicGetRef(structObj));
    assertEquals("another string", wasm.atomicGetRefRef(structStruct));
    assertTraps(kTrapNullDereference, () => wasm.atomicGet32(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicGet64(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetRef(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet32(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet64(null, 1n));
    assertTraps(kTrapNullDereference, () => wasm.atomicSetRef(null, "a"));
  }
})();

(function TestAtomicStructGetPacked() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let struct = builder.addStruct({
      fields: [
        makeField(kWasmI8, true),
        makeField(kWasmI8, true),
        makeField(kWasmI16, true),
        makeField(kWasmI16, true),
      ],
      is_shared,
    });
    let producer_sig = makeSig([kWasmI32, kWasmI32], [wasmRefType(struct)]);
    builder.addFunction("newStruct", producer_sig)
      .addBody([
        kExprI32Const, 42,
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        ...wasmI32Const(12_345),
        kGCPrefix, kExprStructNew, struct])
      .exportFunc();
    builder.addFunction("atomicGetS8",
        makeSig([wasmRefNullType(struct)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicGetS16",
        makeSig([wasmRefNullType(struct)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicGetU8",
        makeSig([wasmRefNullType(struct)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetU, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicGetU16",
        makeSig([wasmRefNullType(struct)], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetU, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicSet8",
        makeSig([wasmRefNullType(struct), kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicSet16",
        makeSig([wasmRefNullType(struct), kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 2,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let structPos = wasm.newStruct(12, 3456);
    assertEquals(12, wasm.atomicGetS8(structPos));
    assertEquals(12, wasm.atomicGetU8(structPos));
    assertEquals(3456, wasm.atomicGetS16(structPos));
    assertEquals(3456, wasm.atomicGetU16(structPos));
    let structNeg = wasm.newStruct(-12, -3456);
    assertEquals(-12, wasm.atomicGetS8(structNeg));
    assertEquals(244, wasm.atomicGetU8(structNeg));
    assertEquals(-3456, wasm.atomicGetS16(structNeg));
    assertEquals(62080, wasm.atomicGetU16(structNeg));
    wasm.atomicSet8(structNeg, -100);
    wasm.atomicSet16(structNeg, -200);
    assertEquals(-100, wasm.atomicGetS8(structNeg));
    assertEquals(-200, wasm.atomicGetS16(structNeg));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetS8(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetS16(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet8(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet16(null));
  }
})();

(function TestAtomicArrayGetSet() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let anyRefT = is_shared
      ? wasmRefNullType(kWasmAnyRef).shared()
      : wasmRefNullType(kWasmAnyRef);
    let array32 =
      builder.addArray(kWasmI32, true, kNoSuperType, false, is_shared);
    let array64 =
      builder.addArray(kWasmI64, true, kNoSuperType, false, is_shared);
    let arrayRef =
      builder.addArray(anyRefT, true, kNoSuperType, false, is_shared);
    builder.addFunction("newArray32", makeSig([kWasmI32, kWasmI32], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array32, 2,
      ])
      .exportFunc();
    builder.addFunction("newArray64", makeSig([kWasmI64, kWasmI64], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array64, 2,
      ])
      .exportFunc();
    builder.addFunction("newArrayRef", makeSig([anyRefT, anyRefT], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, arrayRef, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicGet32",
        makeSig([wasmRefNullType(array32), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGet, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicGet64",
        makeSig([wasmRefNullType(array64), kWasmI32], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGet, kAtomicSeqCst, array64,
      ])
      .exportFunc();
    builder.addFunction("atomicGetRef",
        makeSig([wasmRefNullType(arrayRef), kWasmI32], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGet, kAtomicSeqCst, arrayRef,
      ])
      .exportFunc();
    builder.addFunction("atomicSet32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicSet64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array64,
      ])
      .exportFunc();
    builder.addFunction("atomicSetRef",
        makeSig([wasmRefNullType(arrayRef), kWasmI32, anyRefT], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, arrayRef,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let array32Obj = wasm.newArray32(42, 43);
    assertEquals(42, wasm.atomicGet32(array32Obj, 0));
    assertEquals(43, wasm.atomicGet32(array32Obj, 1));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGet32(array32Obj, 2));
    assertTraps(kTrapNullDereference, () => wasm.atomicGet32(null, 0));
    wasm.atomicSet32(array32Obj, 0, -12345);
    assertEquals(-12345, wasm.atomicGet32(array32Obj, 0));
    assertEquals(43, wasm.atomicGet32(array32Obj, 1));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicSet32(array32Obj, 2));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet32(null, 0, 0));

    let array64Obj = wasm.newArray64(42n, 43n);
    assertEquals(42n, wasm.atomicGet64(array64Obj, 0));
    assertEquals(43n, wasm.atomicGet64(array64Obj, 1));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGet64(array64Obj, 2));
    assertTraps(kTrapNullDereference, () => wasm.atomicGet64(null, 0));
    wasm.atomicSet64(array64Obj, 0, -123_456_789_012n);
    assertEquals(-123_456_789_012n, wasm.atomicGet64(array64Obj, 0));
    assertEquals(43n, wasm.atomicGet64(array64Obj, 1));
    assertTraps(kTrapArrayOutOfBounds,
      () => wasm.atomicSet64(array64Obj, 2, 1n));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet64(null, 0, 1n));

    let arrayRefObj = wasm.newArrayRef("First", "Second");
    assertEquals("First", wasm.atomicGetRef(arrayRefObj, 0));
    assertEquals("Second", wasm.atomicGetRef(arrayRefObj, 1));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGetRef(arrayRefObj, 2));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetRef(null, 0));
    wasm.atomicSetRef(arrayRefObj, 0, "A new value");
    assertEquals("A new value", wasm.atomicGetRef(arrayRefObj, 0));
    assertEquals("Second", wasm.atomicGetRef(arrayRefObj, 1));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicSetRef(arrayRefObj, 2));
    assertTraps(kTrapNullDereference, () => wasm.atomicSetRef(null, 0));
  }
})();

(function TestAtomicArrayGetPacked() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let anyRefT = is_shared
      ? wasmRefNullType(kWasmAnyRef).shared()
      : wasmRefNullType(kWasmAnyRef);
    let array8 =
      builder.addArray(kWasmI8, true, kNoSuperType, false, is_shared);
    let array16 =
      builder.addArray(kWasmI16, true, kNoSuperType, false, is_shared);
    builder.addFunction("newArray8", makeSig([kWasmI32, kWasmI32], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array8, 2,
      ])
      .exportFunc();
    builder.addFunction("newArray16", makeSig([kWasmI32, kWasmI32], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array16, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicGetS8",
        makeSig([wasmRefNullType(array8), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array8,
      ])
      .exportFunc();
    builder.addFunction("atomicGetU8",
        makeSig([wasmRefNullType(array8), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGetU, kAtomicSeqCst, array8,
      ])
      .exportFunc();
    builder.addFunction("atomicGetS16",
        makeSig([wasmRefNullType(array16), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array16,
      ])
      .exportFunc();
    builder.addFunction("atomicGetU16",
        makeSig([wasmRefNullType(array16), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprArrayAtomicGetU, kAtomicSeqCst, array16,
      ])
      .exportFunc();
    builder.addFunction("atomicSet8",
        makeSig([wasmRefNullType(array8), kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array8,
      ])
      .exportFunc();
    builder.addFunction("atomicSet16",
        makeSig([wasmRefNullType(array16), kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSet, kAtomicSeqCst, array16,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let array8Obj = wasm.newArray8(42, -55);
    assertEquals(42, wasm.atomicGetS8(array8Obj, 0));
    assertEquals(-55, wasm.atomicGetS8(array8Obj, 1));
    assertEquals(42, wasm.atomicGetU8(array8Obj, 0));
    assertEquals(201, wasm.atomicGetU8(array8Obj, 1));
    wasm.atomicSet8(array8Obj, 1, 123);
    assertEquals(42, wasm.atomicGetS8(array8Obj, 0));
    assertEquals(123, wasm.atomicGetS8(array8Obj, 1));
    let array16Obj = wasm.newArray16(4200, -5500);
    assertEquals(4200, wasm.atomicGetS16(array16Obj, 0));
    assertEquals(-5500, wasm.atomicGetS16(array16Obj, 1));
    assertEquals(4200, wasm.atomicGetU16(array16Obj, 0));
    assertEquals(60_036, wasm.atomicGetU16(array16Obj, 1));
    wasm.atomicSet16(array16Obj, 1, 123);
    assertEquals(4200, wasm.atomicGetS16(array16Obj, 0));
    assertEquals(123, wasm.atomicGetS16(array16Obj, 1));

    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGetS8(array8Obj, 2));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGetU8(array8Obj, 2));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicSet8(array8Obj, 2 , 0));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGetS16(array16Obj, 2));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicGetU16(array16Obj, 2));
    assertTraps(kTrapArrayOutOfBounds, () => wasm.atomicSet16(array16Obj, 2 , 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetS8(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetU8(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet8(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetS16(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicGetU16(null));
    assertTraps(kTrapNullDereference, () => wasm.atomicSet16(null, 0, 0));
  }
})();

(function TestAtomicStructRMW() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let struct = builder.addStruct({
      fields: [makeField(kWasmI32, true), makeField(kWasmI64, true)],
      is_shared,
    });
    let producer_sig = makeSig([kWasmI32, kWasmI64], [wasmRefType(struct)]);
    builder.addFunction("newStruct", producer_sig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructNew, struct])
      .exportFunc();
    builder.addFunction("atomicAdd32",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicAdd, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicSub32",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSub, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicAnd32",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicAnd, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicOr32",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicOr, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicXor32",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicXor, kAtomicSeqCst, struct, 0,
      ])
      .exportFunc();

      builder.addFunction("atomicAdd64",
        makeSig([wasmRefNullType(struct), kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicAdd, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicSub64",
        makeSig([wasmRefNullType(struct), kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSub, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicAnd64",
        makeSig([wasmRefNullType(struct), kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicAnd, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicOr64",
        makeSig([wasmRefNullType(struct), kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicOr, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();
    builder.addFunction("atomicXor64",
        makeSig([wasmRefNullType(struct), kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicXor, kAtomicSeqCst, struct, 1,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let structObj = wasm.newStruct(42, -42n);
    assertEquals(42, wasm.atomicAdd32(structObj, 24));
    assertEquals(66, wasm.atomicAdd32(structObj, -1));
    assertEquals(65, wasm.atomicAdd32(structObj, 1));
    assertEquals(66, wasm.atomicSub32(structObj, 1));
    assertEquals(65, wasm.atomicSub32(structObj, -10));
    assertEquals(75, wasm.atomicSub32(structObj, 0));

    assertEquals(-42n, wasm.atomicAdd64(structObj, 24n));
    assertEquals(-18n, wasm.atomicAdd64(structObj, -1n));
    assertEquals(-19n, wasm.atomicAdd64(structObj, 1n));
    assertEquals(-18n, wasm.atomicSub64(structObj, 1n));
    assertEquals(-19n, wasm.atomicSub64(structObj, -10n));
    assertEquals(-9n, wasm.atomicSub64(structObj, 0n));

    structObj = wasm.newStruct(0b1101 << 16, 0b1101n << 35n);
    assertEquals(0b1101 << 16, wasm.atomicAnd32(structObj, 0b1011 << 16));
    assertEquals(0b1001 << 16, wasm.atomicOr32(structObj, 0b0011 << 16));
    assertEquals(0b1011 << 16, wasm.atomicXor32(structObj, 0b0110 << 16));
    assertEquals(0b1101 << 16, wasm.atomicXor32(structObj, 0));

    assertEquals(0b1101n << 35n, wasm.atomicAnd64(structObj, 0b1011n << 35n));
    assertEquals(0b1001n << 35n, wasm.atomicOr64(structObj, 0b0011n << 35n));
    assertEquals(0b1011n << 35n, wasm.atomicXor64(structObj, 0b0110n << 35n));
    assertEquals(0b1101n << 35n, wasm.atomicXor64(structObj, 0n));

    assertTraps(kTrapNullDereference, () => wasm.atomicAdd32(null, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicSub32(null, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicAnd32(null, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicOr32(null, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicXor32(null, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicAdd64(null, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicSub64(null, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicAnd64(null, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicOr64(null, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicXor64(null, 0n));
  }
})();

(function TestLoadEliminationAtomicOperations() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let struct = builder.addStruct({
      fields: [makeField(kWasmI32, true)],
      is_shared,
    });
    let producer_sig = makeSig([kWasmI32], [wasmRefType(struct)]);
    builder.addFunction("newStruct", producer_sig)
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStructNew, struct])
      .exportFunc();
    builder.addFunction("atomicSetUpdates",
        makeSig([wasmRefNullType(struct), kWasmI32], [kWasmI32, kWasmI32]))
      .addBody([
        // First non-atomic load.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct, 0,
        // Perform atomic set.
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, struct, 0,
        // Second non-atomic load. Must retrieve the value set by the atomic
        // operation, not the first load.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct, 0,
      ])
      .exportFunc();
    builder.addFunction("atomicRMWUpdates",
        makeSig(
          [wasmRefNullType(struct), kWasmI32], [kWasmI32, kWasmI32, kWasmI32]))
      .addBody([
        // First non-atomic load.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct, 0,
        // Perform atomic rmw.add.
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprStructAtomicAdd, kAtomicSeqCst, struct, 0,
        // Second non-atomic load. Must retrieve the value set by the atomic
        // operation, not the first load.
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct, 0,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let structObj = wasm.newStruct(12345);
    assertEquals([12345, 42], wasm.atomicSetUpdates(structObj, 42));
    assertEquals([42, 42, 142], wasm.atomicRMWUpdates(structObj, 100));
  }
})();

(function TestAtomicArrayRMW() {
  for (let is_shared of [true, false]) {
    print(`${arguments.callee.name} ${is_shared ? "shared" : "unshared"}`);
    let builder = new WasmModuleBuilder();
    let anyRefT = is_shared
      ? wasmRefNullType(kWasmAnyRef).shared()
      : wasmRefNullType(kWasmAnyRef);
    let array32 =
      builder.addArray(kWasmI32, true, kNoSuperType, false, is_shared);
    let array64 =
      builder.addArray(kWasmI64, true, kNoSuperType, false, is_shared);
    builder.addFunction("newArray32", makeSig([kWasmI32, kWasmI32], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array32, 2,
      ])
      .exportFunc();
    builder.addFunction("newArray64", makeSig([kWasmI64, kWasmI64], [anyRefT]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprArrayNewFixed, array64, 2,
      ])
      .exportFunc();
    builder.addFunction("atomicAdd32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicAdd, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicSub32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSub, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicAnd32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicAnd, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicOr32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicOr, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicXor32",
        makeSig([wasmRefNullType(array32), kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicXor, kAtomicSeqCst, array32,
      ])
      .exportFunc();
    builder.addFunction("atomicAdd64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicAdd, kAtomicSeqCst, array64,
      ])
      .exportFunc();
    builder.addFunction("atomicSub64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicSub, kAtomicSeqCst, array64,
      ])
      .exportFunc();
    builder.addFunction("atomicAnd64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicAnd, kAtomicSeqCst, array64,
      ])
      .exportFunc();
    builder.addFunction("atomicOr64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicOr, kAtomicSeqCst, array64,
      ])
      .exportFunc();
      builder.addFunction("atomicXor64",
        makeSig([wasmRefNullType(array64), kWasmI32, kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kAtomicPrefix, kExprArrayAtomicXor, kAtomicSeqCst, array64,
      ])
      .exportFunc();

    let wasm = builder.instantiate().exports;
    let array32Obj = wasm.newArray32(42, -42);
    let array64Obj = wasm.newArray64(42n, -42n);

    assertEquals(42, wasm.atomicAdd32(array32Obj, 0, 24));
    assertEquals(66, wasm.atomicAdd32(array32Obj, 0, -1));
    assertEquals(65, wasm.atomicAdd32(array32Obj, 0, 1));
    assertEquals(-42, wasm.atomicSub32(array32Obj, 1, 1));
    assertEquals(-43, wasm.atomicSub32(array32Obj, 1, -10));
    assertEquals(-33, wasm.atomicSub32(array32Obj, 1, 0));

    assertEquals(42n, wasm.atomicAdd64(array64Obj, 0, 24n));
    assertEquals(66n, wasm.atomicAdd64(array64Obj, 0, -1n));
    assertEquals(65n, wasm.atomicAdd64(array64Obj, 0, 1n));
    assertEquals(-42n, wasm.atomicSub64(array64Obj, 1, 1n));
    assertEquals(-43n, wasm.atomicSub64(array64Obj, 1, -10n));
    assertEquals(-33n, wasm.atomicSub64(array64Obj, 1, 0n));

    array64Obj = wasm.newArray64(0n, 0b1101n << 35n);
    array32Obj = wasm.newArray32(0, 0b1101 << 16);

    assertEquals(0b1101 << 16, wasm.atomicAnd32(array32Obj, 1, 0b1011 << 16));
    assertEquals(0b1001 << 16, wasm.atomicOr32(array32Obj, 1, 0b0011 << 16));
    assertEquals(0b1011 << 16, wasm.atomicXor32(array32Obj, 1, 0b0110 << 16));
    assertEquals(0b1101 << 16, wasm.atomicXor32(array32Obj, 1, 0));

    assertEquals(0b1101n << 35n,
      wasm.atomicAnd64(array64Obj, 1, 0b1011n << 35n));
    assertEquals(0b1001n << 35n,
      wasm.atomicOr64(array64Obj, 1, 0b0011n << 35n));
    assertEquals(0b1011n << 35n,
      wasm.atomicXor64(array64Obj, 1, 0b0110n << 35n));
    assertEquals(0b1101n << 35n, wasm.atomicXor64(array64Obj, 1, 0n));

    assertTraps(kTrapNullDereference, () => wasm.atomicAdd32(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicSub32(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicAnd32(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicOr32(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicXor32(null, 0, 0));
    assertTraps(kTrapNullDereference, () => wasm.atomicAdd64(null, 0, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicSub64(null, 0, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicAnd64(null, 0, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicOr64(null, 0, 0n));
    assertTraps(kTrapNullDereference, () => wasm.atomicXor64(null, 0, 0n));

    let trapOob = kTrapArrayOutOfBounds;
    assertTraps(trapOob, () => wasm.atomicAdd32(array32Obj, 2, 0));
    assertTraps(trapOob, () => wasm.atomicSub32(array32Obj, 2, 0));
    assertTraps(trapOob, () => wasm.atomicAnd32(array32Obj, 2, 0));
    assertTraps(trapOob, () => wasm.atomicOr32(array32Obj, 2, 0));
    assertTraps(trapOob, () => wasm.atomicXor32(array32Obj, 2, 0));
    assertTraps(trapOob, () => wasm.atomicAdd64(array64Obj, 2, 0n));
    assertTraps(trapOob, () => wasm.atomicSub64(array64Obj, 2, 0n));
    assertTraps(trapOob, () => wasm.atomicAnd64(array64Obj, 2, 0n));
    assertTraps(trapOob, () => wasm.atomicOr64(array64Obj, 2, 0n));
    assertTraps(trapOob, () => wasm.atomicXor64(array64Obj, 2, 0n));
  }
})();
