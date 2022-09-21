// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestArrayNewElem() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);

  function makeStruct(element) {
    return [...wasmI32Const(element),
            kGCPrefix, kExprStructNew, struct_type_index];
  }

  let table = builder.addTable(kWasmAnyRef, 10, 10);

  let elems = [10, -10, 42, 55];

  let passive_segment = builder.addPassiveElementSegment(
    [makeStruct(elems[0]), makeStruct(elems[1]),
     [kExprRefNull, struct_type_index]],
    struct_type);

  let active_segment = builder.addActiveElementSegment(
      table, wasmI32Const(0), [makeStruct(elems[2]), makeStruct(elems[3])],
      struct_type);

  function generator(name, segment) {
    builder.addFunction(name, makeSig([kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprI32Const, 0,  // offset
        kExprLocalGet, 0,  // length
        kGCPrefix, kExprArrayNewElem, array_type_index,
        segment,
        kExprLocalGet, 1,  // index in the array
        kGCPrefix, kExprArrayGet, array_type_index,
        kGCPrefix, kExprStructGet, struct_type_index, 0])
      .exportFunc()
  }

  generator("init_and_get", passive_segment);
  generator("init_and_get_active", active_segment);

  builder.addFunction("drop", kSig_v_v)
    .addBody([kNumericPrefix, kExprElemDrop, passive_segment])
    .exportFunc();

  let instance = builder.instantiate();

  let init_and_get = instance.exports.init_and_get;
  let init_and_get_active = instance.exports.init_and_get_active;
  // Initializing from a passive segment works. The third element is null, so
  // we get a null dereference.
  assertEquals(elems[0], init_and_get(3, 0));
  assertEquals(elems[1], init_and_get(3, 1));
  assertTraps(kTrapNullDereference, () => init_and_get(3, 2));
  // The array has the correct length.
  assertTraps(kTrapArrayOutOfBounds, () => init_and_get(3, 3));
  // Too large arrays are disallowed, in and out of Smi range.
  assertTraps(kTrapArrayTooLarge, () => init_and_get(1000000000, 10));
  assertTraps(kTrapArrayTooLarge, () => init_and_get(1 << 31, 10));
  // Element is out of bounds.
  assertTraps(kTrapElementSegmentOutOfBounds, () => init_and_get(5, 0));
  // Now drop the segment.
  instance.exports.drop();
  // A 0-length array should still be created...
  assertTraps(kTrapArrayOutOfBounds, () => init_and_get(0, 0));
  // ... but not a longer one.
  assertTraps(kTrapElementSegmentOutOfBounds, () => init_and_get(1, 0));
  // Same holds for an active segment.
  assertTraps(kTrapArrayOutOfBounds, () => init_and_get_active(0, 0));
  assertTraps(kTrapElementSegmentOutOfBounds, () => init_and_get_active(1, 0));
})();

(function TestArrayNewElemConstant() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);
  let array_type = wasmRefNullType(array_type_index);

  function makeStruct(element) {
    return [...wasmI32Const(element),
            kGCPrefix, kExprStructNew, struct_type_index];
  }

  builder.addTable(kWasmAnyRef, 10, 10);
  let table = 0;

  let elems = [10, -10, 42, 55];

  let passive_segment = builder.addPassiveElementSegment(
    [makeStruct(elems[0]), makeStruct(elems[1]),
     [kExprRefNull, struct_type_index]],
    struct_type);

  let active_segment = builder.addActiveElementSegment(
      0, wasmI32Const(0), [makeStruct(elems[2]), makeStruct(elems[3])],
      struct_type);

  let array_segment = builder.addPassiveElementSegment(
    [[...wasmI32Const(0), ...wasmI32Const(3),
      kGCPrefix, kExprArrayNewElem,
      array_type_index, passive_segment],
     [...wasmI32Const(0), ...wasmI32Const(0),
      kGCPrefix, kExprArrayNewElem,
      array_type_index, active_segment]],
    array_type);

  builder.addFunction("init", kSig_v_v)
    .addBody([kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 2,
              kNumericPrefix, kExprTableInit, array_segment, table])
    .exportFunc();

  builder.addFunction("table_get", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,  // offset in table
      kExprTableGet, table,
      kGCPrefix, kExprRefAsData,
      kGCPrefix, kExprRefCast, array_type_index,
      kExprLocalGet, 1,  // index in the array
      kGCPrefix, kExprArrayGet, array_type_index,
      kGCPrefix, kExprStructGet, struct_type_index, 0])
    .exportFunc()

  builder.addFunction("drop", kSig_v_v)
    .addBody([kNumericPrefix, kExprElemDrop, passive_segment])
    .exportFunc();

  let instance = builder.instantiate();

  // First, initialize the table.
  instance.exports.init();
  let table_get = instance.exports.table_get;
  // Initializing from a passive segment works. The third element is null, so
  // we get a null dereference.
  assertEquals(elems[0], table_get(0, 0));
  assertEquals(elems[1], table_get(0, 1));
  assertTraps(kTrapNullDereference, () => table_get(0, 2));
  // The array has the correct length.
  assertTraps(kTrapArrayOutOfBounds, () => table_get(0, 3));
  // The array generated from the active segment should have length 0.
  assertTraps(kTrapArrayOutOfBounds, () => table_get(1, 0));
  // Now drop the segment with the array elements and reload the table.
  instance.exports.drop();
  instance.exports.init();
  // Nothing should change: a constant expression should not observe the dropped
  // segments.
  assertEquals(elems[0], table_get(0, 0));
  assertEquals(elems[1], table_get(0, 1));
  assertTraps(kTrapNullDereference, () => table_get(0, 2));
  assertTraps(kTrapArrayOutOfBounds, () => table_get(0, 3));
})();

(function TestArrayNewElemMistypedSegment() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);

  let passive_segment = builder.addPassiveElementSegment([
    [kExprRefNull, array_type_index]],
    wasmRefNullType(array_type_index));

  builder.addFunction("mistyped", makeSig([kWasmI32, kWasmI32], [kWasmI32]))
      .addBody([
        kExprI32Const, 0,  // offset
        kExprLocalGet, 0,  // length
        kGCPrefix, kExprArrayNewElem, array_type_index,
        passive_segment,
        kExprLocalGet, 1,  // index in the array
        kGCPrefix, kExprArrayGet, array_type_index,
        kGCPrefix, kExprStructGet, struct_type_index, 0])
      .exportFunc()

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
               /segment type.*is not a subtype of array element type.*/);
})();

// Element segments are defined after globals, so currently it is not valid
// to refer to an element segment in the global section.
(function TestArrayNewFixedFromElemInGlobal() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);

  let passive_segment = builder.addPassiveElementSegment([
    [kExprRefNull, struct_type_index]],
    struct_type_index);

  builder.addGlobal(
    wasmRefNullType(array_type_index), false,
    [...wasmI32Const(0), ...wasmI32Const(1),
     kGCPrefix, kExprArrayNewElem,
     array_type_index, passive_segment]);

  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
               /invalid element segment index/);
})();

(function TestArrayNewElemConstantArrayTooLarge() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);
  let array_type = wasmRefNullType(array_type_index);

  function makeStruct(element) {
    return [...wasmI32Const(element),
            kGCPrefix, kExprStructNew, struct_type_index];
  }

  builder.addTable(kWasmAnyRef, 10, 10);
  let table = 0;

  let elems = [10, -10];

  let passive_segment = builder.addPassiveElementSegment(
    [makeStruct(elems[0]), makeStruct(elems[1]),
     [kExprRefNull, struct_type_index]],
    struct_type);

  let array_segment = builder.addPassiveElementSegment(
    [[...wasmI32Const(0), ...wasmI32Const(1 << 30),
      kGCPrefix, kExprArrayNewElem,
      array_type_index, passive_segment]],
    array_type
  );

  builder.addFunction("init", kSig_v_v)
    .addBody([kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 1,
              kNumericPrefix, kExprTableInit, array_segment, table])
    .exportFunc();

  let instance = builder.instantiate();
  assertTraps(kTrapArrayTooLarge, () => instance.exports.init());
})();

(function TestArrayNewElemConstantElementSegmentOutOfBounds() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);
  let array_type = wasmRefNullType(array_type_index);

  function makeStruct(element) {
    return [...wasmI32Const(element),
            kGCPrefix, kExprStructNew, struct_type_index];
  }

  builder.addTable(kWasmAnyRef, 10, 10);
  let table = 0;

  let elems = [10, -10];

  let passive_segment = builder.addPassiveElementSegment(
    [makeStruct(elems[0]), makeStruct(elems[1]),
     [kExprRefNull, struct_type_index]],
    struct_type);

  let array_segment = builder.addPassiveElementSegment(
    [[...wasmI32Const(0), ...wasmI32Const(10),
      kGCPrefix, kExprArrayNewElem,
      array_type_index, passive_segment]],
    array_type
  );

  builder.addFunction("init", kSig_v_v)
    .addBody([kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 1,
              kNumericPrefix, kExprTableInit, array_segment, table])
    .exportFunc();

  let instance = builder.instantiate();
  assertTraps(kTrapElementSegmentOutOfBounds, () => instance.exports.init());
})();

(function TestArrayNewElemConstantActiveSegment() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let struct_type_index = builder.addStruct([makeField(kWasmI32, false)]);
  let struct_type = wasmRefNullType(struct_type_index);
  let array_type_index = builder.addArray(struct_type, true);
  let array_type = wasmRefNullType(array_type_index);

  function makeStruct(element) {
    return [...wasmI32Const(element),
            kGCPrefix, kExprStructNew, struct_type_index];
  }

  builder.addTable(kWasmAnyRef, 10, 10);
  let table = 0;

  let elems = [10, -10];

  let active_segment = builder.addActiveElementSegment(
    table, wasmI32Const(0),
    [makeStruct(elems[0]), makeStruct(elems[1]),
     [kExprRefNull, struct_type_index]],
    struct_type);

  let array_segment = builder.addPassiveElementSegment(
    [[...wasmI32Const(0), ...wasmI32Const(3),
      kGCPrefix, kExprArrayNewElem,
      array_type_index, active_segment]],
    array_type
  );

  builder.addFunction("init", kSig_v_v)
    .addBody([kExprI32Const, 0, kExprI32Const, 0, kExprI32Const, 1,
              kNumericPrefix, kExprTableInit, array_segment, table])
    .exportFunc();

  let instance = builder.instantiate();
  // An active segment counts as having 0 length.
  assertTraps(kTrapElementSegmentOutOfBounds, () => instance.exports.init());
})();
