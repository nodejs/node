// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestArrayFillImmutable() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let array = builder.addArray(kWasmI32, false);

  // Parameters: array, starting index, value, length.
  builder.addFunction(
      "array_fill",
      makeSig([wasmRefNullType(array), kWasmI32, kWasmI32, kWasmI32], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kExprLocalGet, 2, kExprLocalGet, 3,
              kGCPrefix, kExprArrayFill, array])
    .exportFunc();

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
               /immediate array type #0 is immutable/);
})();

(function TestArrayFill() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(wasmRefNullType(struct), true);

  builder.addFunction(
      "make_array", makeSig([kWasmI32], [wasmRefType(array)]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprArrayNewDefault, array])
    .exportFunc();

  builder.addFunction(
      "array_get", makeSig([wasmRefNullType(array), kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprArrayGet, array,
              kGCPrefix, kExprStructGet, struct, 0])
    .exportFunc();

  // Parameters: array, starting index, value in struct, length.
  builder.addFunction(
      "array_fill",
      makeSig([wasmRefNullType(array), kWasmI32, kWasmI32, kWasmI32], []))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kExprLocalGet, 2, kGCPrefix, kExprStructNew, struct,
              kExprLocalGet, 3, kGCPrefix, kExprArrayFill, array])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  assertTraps(kTrapNullDereference, () => wasm.array_fill(null, 10, 20, 30));

  let array_obj = wasm.make_array(8);

  assertTraps(kTrapArrayOutOfBounds,
              () => wasm.array_fill(array_obj, 5, 42, 4));
  // Out-of-bounds array.fill traps even if length is 0, if the index is greater
  // than array.len.
  assertTraps(kTrapArrayOutOfBounds,
              () => wasm.array_fill(array_obj, 10, 42, 0));
  // Overflow of (index + length) traps.
  assertTraps(kTrapArrayOutOfBounds,
              () => wasm.array_fill(array_obj, 5, 42, -1));

  wasm.array_fill(array_obj, 2, 42, 3);
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 0));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 1));
  assertEquals(42, wasm.array_get(array_obj, 2));
  assertEquals(42, wasm.array_get(array_obj, 3));
  assertEquals(42, wasm.array_get(array_obj, 4));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 5));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 6));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 7));

  // Index = array.len and length = 0 works, it just does nothing.
  wasm.array_fill(array_obj, 8, 42, 0)

  wasm.array_fill(array_obj, 4, 54, 2);
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 0));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 1));
  assertEquals(42, wasm.array_get(array_obj, 2));
  assertEquals(42, wasm.array_get(array_obj, 3));
  assertEquals(54, wasm.array_get(array_obj, 4));
  assertEquals(54, wasm.array_get(array_obj, 5));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 6));
  assertTraps(kTrapNullDereference, () => wasm.array_get(array_obj, 7));
})();

(function TestArrayNewNonNullable() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)]);
  let array = builder.addArray(wasmRefType(struct), true);

  builder.addFunction(
      "make_array", makeSig([wasmRefType(struct), kWasmI32],
                            [wasmRefType(array)]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprArrayNew, array])
    .exportFunc();

  builder.addFunction(
      "array_get", makeSig([wasmRefNullType(array), kWasmI32],
                           [wasmRefType(struct)]))
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1,
              kGCPrefix, kExprArrayGet, array])
    .exportFunc();

  builder.addFunction(
      "make_struct", makeSig([], [wasmRefType(struct)]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let length = 50;  // Enough to go through initialization builtin.
  let struct_obj = wasm.make_struct();
  let array_obj = wasm.make_array(struct_obj, length);

  for (let i = 0; i < length; i++) {
    assertEquals(struct_obj, wasm.array_get(array_obj, i));
  }
})();
