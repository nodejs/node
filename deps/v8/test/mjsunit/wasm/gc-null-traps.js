// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestNullDereferences() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(kWasmI64, true);
  let sig = builder.addType(kSig_i_i);

  builder.addFunction(
      "structGet", makeSig([wasmRefNullType(struct)], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  builder.addFunction(
      "structSet", makeSig([wasmRefNullType(struct), kWasmI32], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprStructSet, struct, 0])
    .exportFunc();

  builder.addFunction(
      "arrayGet", makeSig([wasmRefNullType(array), kWasmI32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprArrayGet, array])
    .exportFunc();

  builder.addFunction(
      "arraySet", makeSig([wasmRefNullType(array), kWasmI32, kWasmI64], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
              kGCPrefix, kExprArraySet, array])
    .exportFunc();

  builder.addFunction(
      "arrayLen", makeSig([wasmRefNullType(array)], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayLen])
    .exportFunc();

  builder.addFunction(
      "refAsNonNullStruct",
      makeSig([wasmRefNullType(struct)], [wasmRefType(struct)]))
    .addBody([kExprLocalGet, 0, kExprRefAsNonNull])
    .exportFunc();

  builder.addFunction(
      "refAsNonNullFunction",
      makeSig([wasmRefNullType(sig)], [wasmRefType(sig)]))
    .addBody([kExprLocalGet, 0, kExprRefAsNonNull])
    .exportFunc();

  builder.addFunction(
      "refAsNonNullAny",
      makeSig([kWasmAnyRef], [wasmRefType(kWasmAnyRef)]))
    .addBody([kExprLocalGet, 0, kExprRefAsNonNull])
    .exportFunc();

  builder.addFunction(
      "refAsNonNullI31",
      makeSig([kWasmI31Ref], [wasmRefType(kWasmI31Ref)]))
    .addBody([kExprLocalGet, 0, kExprRefAsNonNull])
    .exportFunc();

  builder.addFunction(
      "refAsNonNullExtern",
      makeSig([kWasmExternRef], [wasmRefType(kWasmExternRef)]))
    .addBody([kExprLocalGet, 0, kExprRefAsNonNull])
    .exportFunc();

  let instance = builder.instantiate();

  assertTraps(kTrapNullDereference, () => instance.exports.structGet(null));
  assertTraps(kTrapNullDereference, () => instance.exports.structSet(null, 15));
  assertTraps(kTrapNullDereference, () => instance.exports.arrayGet(null, 0));
  assertTraps(kTrapNullDereference,
              () => instance.exports.arrayGet(null, 2000000000));
  assertTraps(kTrapNullDereference,
              () => instance.exports.arraySet(null, 0, 42n));
  assertTraps(kTrapNullDereference,
              () => instance.exports.arraySet(null, 2000000000, 42n));
  assertTraps(kTrapNullDereference, () => instance.exports.arrayLen(null));
  assertTraps(kTrapNullDereference,
              () => instance.exports.refAsNonNullStruct(null));
  assertTraps(kTrapNullDereference,
              () => instance.exports.refAsNonNullFunction(null));
  assertTraps(kTrapNullDereference,
              () => instance.exports.refAsNonNullAny(null));
  assertEquals(42, instance.exports.refAsNonNullAny(42));
  assertTraps(kTrapNullDereference,
              () => instance.exports.refAsNonNullI31(null));
  assertEquals(42, instance.exports.refAsNonNullI31(42));
  assertTraps(kTrapNullDereference,
              () => instance.exports.refAsNonNullExtern(null));
  let object = {};
  assertEquals(object, instance.exports.refAsNonNullExtern(object));
})();
