// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestSharedInAnyRefToAbstractSubtype() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let sharedArrayT = builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  let sharedStructT = builder.addStruct(
      [makeField(kWasmI32, true)], kNoSuperType, false, true);
  let arrayT = builder.addArray(kWasmI32, true);
  let structT = builder.addStruct([makeField(kWasmI32, true)]);
  builder.addFunction("newSharedStruct",
      makeSig([kWasmI32], [wasmRefType(sharedStructT)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, sharedStructT])
    .exportFunc();
  builder.addFunction("newSharedArray",
      makeSig([kWasmI32], [wasmRefType(sharedArrayT)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, sharedArrayT])
    .exportFunc();
  builder.addFunction("newStruct", makeSig([kWasmI32], [wasmRefType(structT)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructNew, structT])
    .exportFunc();
  builder.addFunction("newArray", makeSig([kWasmI32], [wasmRefType(arrayT)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, arrayT])
    .exportFunc();

  let testInstructions = [
    ["refTest", kExprRefTest],
    ["refTestNull", kExprRefTestNull],
  ];
  let castInstructions = [
    ["refCast", kExprRefCast],
    ["refCastNull", kExprRefCastNull],
  ];
  let types = [
    ["Eq", kEqRefCode],
    ["Array", kArrayRefCode],
    ["Struct", kStructRefCode],
  ];

  for (let [typeName, typeCode] of types) {
    for (let [instName, instruction] of testInstructions) {
      builder.addFunction(`${instName}${typeName}`,
        makeSig([kWasmAnyRef], [kWasmI32]))
      .exportFunc()
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, instruction, typeCode,
      ]);
    }
    for (let [instName, instruction] of castInstructions) {
      builder.addFunction(`${instName}${typeName}`,
        makeSig([kWasmAnyRef], []))
      .exportFunc()
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, instruction, typeCode,
        kExprDrop,
      ]);
    }

    builder.addFunction(`brOnCast${typeName}`,
      makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprBlock, kWasmRef, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCast, 0b01, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
    ]);

    builder.addFunction(`brOnCastNull${typeName}`,
      makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprBlock, kWasmRefNull, typeCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCast, 0b11, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
    ]);

    builder.addFunction(`brOnCastFail${typeName}`,
      makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprBlock, kWasmRefNull, kAnyRefCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCastFail, 0b01, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
    ]);

    builder.addFunction(`brOnCastFailNull${typeName}`,
      makeSig([kWasmAnyRef], [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprBlock, kWasmRef, kAnyRefCode,
        kExprLocalGet, 0,
        kGCPrefix, kExprBrOnCastFail, 0b11, 0, kAnyRefCode, typeCode,
        kExprI32Const, 0,
        kExprReturn,
      kExprEnd,
      kExprDrop,
      kExprI32Const, 1,
    ]);
  }

  let wasm = builder.instantiate().exports;
  let sharedStruct = wasm.newSharedStruct(42);
  let sharedArray = wasm.newSharedArray(42);
  let struct = wasm.newStruct(42);
  let array = wasm.newArray(42);

  assertEquals(0, wasm.refTestEq(sharedStruct));
  assertEquals(0, wasm.refTestEq(sharedArray));
  assertEquals(0, wasm.refTestEq(null));
  assertEquals(1, wasm.refTestEq(123));
  assertEquals(1, wasm.refTestEq(struct));
  assertEquals(1, wasm.refTestEq(array));

  assertEquals(0, wasm.refTestArray(sharedStruct));
  assertEquals(0, wasm.refTestArray(sharedArray));
  assertEquals(0, wasm.refTestArray(null));
  assertEquals(0, wasm.refTestArray(123));
  assertEquals(0, wasm.refTestArray(struct));
  assertEquals(1, wasm.refTestArray(array));

  assertEquals(0, wasm.refTestStruct(sharedStruct));
  assertEquals(0, wasm.refTestStruct(sharedArray));
  assertEquals(0, wasm.refTestStruct(null));
  assertEquals(0, wasm.refTestStruct(123));
  assertEquals(1, wasm.refTestStruct(struct));
  assertEquals(0, wasm.refTestStruct(array));

  assertEquals(0, wasm.refTestNullEq(sharedStruct));
  assertEquals(0, wasm.refTestNullEq(sharedArray));
  assertEquals(1, wasm.refTestNullEq(null));
  assertEquals(1, wasm.refTestNullEq(123));
  assertEquals(1, wasm.refTestNullEq(struct));
  assertEquals(1, wasm.refTestNullEq(array));

  assertEquals(0, wasm.refTestNullArray(sharedStruct));
  assertEquals(0, wasm.refTestNullArray(sharedArray));
  assertEquals(1, wasm.refTestNullArray(null));
  assertEquals(0, wasm.refTestNullArray(123));
  assertEquals(0, wasm.refTestNullArray(struct));
  assertEquals(1, wasm.refTestNullArray(array));

  assertEquals(0, wasm.refTestNullStruct(sharedStruct));
  assertEquals(0, wasm.refTestNullStruct(sharedArray));
  assertEquals(1, wasm.refTestNullStruct(null));
  assertEquals(0, wasm.refTestNullStruct(123));
  assertEquals(1, wasm.refTestNullStruct(struct));
  assertEquals(0, wasm.refTestNullStruct(array));

  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(sharedArray));
  assertTraps(kTrapIllegalCast, () => wasm.refCastEq(null));
  wasm.refCastEq(123);
  wasm.refCastEq(struct);
  wasm.refCastEq(array);

  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(sharedArray));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(123));
  assertTraps(kTrapIllegalCast, () => wasm.refCastArray(struct));
  wasm.refCastArray(array);

  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(sharedArray));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(null));
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(123));
  wasm.refCastStruct(struct);
  assertTraps(kTrapIllegalCast, () => wasm.refCastStruct(array));

  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullEq(sharedArray));
  wasm.refCastNullEq(null);
  wasm.refCastNullEq(123);
  wasm.refCastNullEq(struct);
  wasm.refCastNullEq(array);

  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(sharedArray));
  wasm.refCastNullArray(null);
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(123));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullArray(struct));
  wasm.refCastNullArray(array);

  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(sharedStruct));
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(sharedArray));
  wasm.refCastNullStruct(null);
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(123));
  wasm.refCastNullStruct(struct);
  assertTraps(kTrapIllegalCast, () => wasm.refCastNullStruct(array));

  assertEquals(0, wasm.brOnCastEq(sharedStruct));
  assertEquals(0, wasm.brOnCastEq(sharedArray));
  assertEquals(0, wasm.brOnCastEq(null));
  assertEquals(1, wasm.brOnCastEq(123));
  assertEquals(1, wasm.brOnCastEq(struct));
  assertEquals(1, wasm.brOnCastEq(array));

  assertEquals(0, wasm.brOnCastArray(sharedStruct));
  assertEquals(0, wasm.brOnCastArray(sharedArray));
  assertEquals(0, wasm.brOnCastArray(null));
  assertEquals(0, wasm.brOnCastArray(123));
  assertEquals(0, wasm.brOnCastArray(struct));
  assertEquals(1, wasm.brOnCastArray(array));

  assertEquals(0, wasm.brOnCastStruct(sharedStruct));
  assertEquals(0, wasm.brOnCastStruct(sharedArray));
  assertEquals(0, wasm.brOnCastStruct(null));
  assertEquals(0, wasm.brOnCastStruct(123));
  assertEquals(1, wasm.brOnCastStruct(struct));
  assertEquals(0, wasm.brOnCastStruct(array));

  assertEquals(0, wasm.brOnCastNullEq(sharedStruct));
  assertEquals(0, wasm.brOnCastNullEq(sharedArray));
  assertEquals(1, wasm.brOnCastNullEq(null));
  assertEquals(1, wasm.brOnCastNullEq(123));
  assertEquals(1, wasm.brOnCastNullEq(struct));
  assertEquals(1, wasm.brOnCastNullEq(array));

  assertEquals(0, wasm.brOnCastNullArray(sharedStruct));
  assertEquals(0, wasm.brOnCastNullArray(sharedArray));
  assertEquals(1, wasm.brOnCastNullArray(null));
  assertEquals(0, wasm.brOnCastNullArray(123));
  assertEquals(0, wasm.brOnCastNullArray(struct));
  assertEquals(1, wasm.brOnCastNullArray(array));

  assertEquals(0, wasm.brOnCastNullStruct(sharedStruct));
  assertEquals(0, wasm.brOnCastNullStruct(sharedArray));
  assertEquals(1, wasm.brOnCastNullStruct(null));
  assertEquals(0, wasm.brOnCastNullStruct(123));
  assertEquals(1, wasm.brOnCastNullStruct(struct));
  assertEquals(0, wasm.brOnCastNullStruct(array));

  assertEquals(1, wasm.brOnCastFailEq(sharedStruct));
  assertEquals(1, wasm.brOnCastFailEq(sharedArray));
  assertEquals(1, wasm.brOnCastFailEq(null));
  assertEquals(0, wasm.brOnCastFailEq(123));
  assertEquals(0, wasm.brOnCastFailEq(struct));
  assertEquals(0, wasm.brOnCastFailEq(array));

  assertEquals(1, wasm.brOnCastFailArray(sharedStruct));
  assertEquals(1, wasm.brOnCastFailArray(sharedArray));
  assertEquals(1, wasm.brOnCastFailArray(null));
  assertEquals(1, wasm.brOnCastFailArray(123));
  assertEquals(1, wasm.brOnCastFailArray(struct));
  assertEquals(0, wasm.brOnCastFailArray(array));

  assertEquals(1, wasm.brOnCastFailStruct(sharedStruct));
  assertEquals(1, wasm.brOnCastFailStruct(sharedArray));
  assertEquals(1, wasm.brOnCastFailStruct(null));
  assertEquals(1, wasm.brOnCastFailStruct(123));
  assertEquals(0, wasm.brOnCastFailStruct(struct));
  assertEquals(1, wasm.brOnCastFailStruct(array));

  assertEquals(1, wasm.brOnCastFailNullEq(sharedStruct));
  assertEquals(1, wasm.brOnCastFailNullEq(sharedArray));
  assertEquals(0, wasm.brOnCastFailNullEq(null));
  assertEquals(0, wasm.brOnCastFailNullEq(123));
  assertEquals(0, wasm.brOnCastFailNullEq(struct));
  assertEquals(0, wasm.brOnCastFailNullEq(array));

  assertEquals(1, wasm.brOnCastFailNullArray(sharedStruct));
  assertEquals(1, wasm.brOnCastFailNullArray(sharedArray));
  assertEquals(0, wasm.brOnCastFailNullArray(null));
  assertEquals(1, wasm.brOnCastFailNullArray(123));
  assertEquals(1, wasm.brOnCastFailNullArray(struct));
  assertEquals(0, wasm.brOnCastFailNullArray(array));

  assertEquals(1, wasm.brOnCastFailNullStruct(sharedStruct));
  assertEquals(1, wasm.brOnCastFailNullStruct(sharedArray));
  assertEquals(0, wasm.brOnCastFailNullStruct(null));
  assertEquals(1, wasm.brOnCastFailNullStruct(123));
  assertEquals(0, wasm.brOnCastFailNullStruct(struct));
  assertEquals(1, wasm.brOnCastFailNullStruct(array));
})();
