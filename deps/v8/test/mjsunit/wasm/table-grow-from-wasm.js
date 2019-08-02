// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-anyref

load("test/mjsunit/wasm/wasm-module-builder.js");

function dummy_func(val) {
  let builder = new WasmModuleBuilder();
  builder.addFunction("dummy", kSig_i_v)
    .addBody([kExprI32Const, val])
    .exportAs("dummy");
  return builder.instantiate().exports.dummy;
}

let kSig_i_ri = makeSig([kWasmAnyRef, kWasmI32], [kWasmI32]);
let kSig_r_i = makeSig([kWasmI32], [kWasmAnyRef]);
let kSig_i_ai = makeSig([kWasmAnyFunc, kWasmI32], [kWasmI32]);

function testGrowInternalAnyRefTable(table_index) {
  print(arguments.callee.name, table_index);

  const builder = new WasmModuleBuilder();
  const initial_size = 5;
  // Add 10 tables, we only test one.
  for (let i = 0; i < 10; ++i) {
    builder.addTable(kWasmAnyRef, initial_size).index;
  }
  builder.addFunction('grow', kSig_i_ri)
    .addBody([kExprGetLocal, 0,
      kExprGetLocal, 1,
      kNumericPrefix, kExprTableGrow, table_index])
    .exportFunc();

  builder.addFunction('size', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, table_index])
    .exportFunc();

  builder.addFunction('get', kSig_r_i)
    .addBody([kExprGetLocal, 0, kExprGetTable, table_index])
    .exportFunc();

  const instance = builder.instantiate();

  let size = initial_size;
  assertEquals(null, instance.exports.get(size - 2));

  function growAndCheck(element, grow_by) {
    assertEquals(size, instance.exports.size());
    assertTraps(kTrapTableOutOfBounds, () => instance.exports.get(size));
    assertEquals(size, instance.exports.grow(element, grow_by));
    for (let i = 0; i < grow_by; ++i) {
      assertEquals(element, instance.exports.get(size + i));
    }
    size += grow_by;
  }
  growAndCheck("Hello", 3);
  growAndCheck(undefined, 4);
  growAndCheck(4, 2);
  growAndCheck({Hello: "World"}, 3);
  growAndCheck(null, 2);
}

testGrowInternalAnyRefTable(0);
testGrowInternalAnyRefTable(7);
testGrowInternalAnyRefTable(9);

function testGrowInternalAnyFuncTable(table_index) {
  print(arguments.callee.name, table_index);

  const builder = new WasmModuleBuilder();
  let size = 5;
  for (let i = 0; i < 10; ++i) {
    builder.addTable(kWasmAnyFunc, size).index;
  }
  builder.addFunction('grow', kSig_i_ai)
    .addBody([kExprGetLocal, 0,
      kExprGetLocal, 1,
      kNumericPrefix, kExprTableGrow, table_index])
    .exportFunc();

  builder.addFunction('size', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, table_index])
    .exportFunc();

  const sig_index = builder.addType(kSig_i_v);
  builder.addFunction('call', kSig_i_i)
    .addBody([kExprGetLocal, 0, kExprCallIndirect, sig_index, table_index])
    .exportFunc();

  const instance = builder.instantiate();
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.call(size - 2));
  function growAndCheck(element, grow_by) {
    assertEquals(size, instance.exports.size());
    assertTraps(kTrapFuncInvalid, () => instance.exports.call(size));
    assertEquals(size, instance.exports.grow(dummy_func(element), grow_by));
    for (let i = 0; i < grow_by; ++i) {
      assertEquals(element, instance.exports.call(size + i));
    }
    size += grow_by;
  }
  growAndCheck(56, 3);
  growAndCheck(12, 4);

  assertEquals(size, instance.exports.grow(null, 1));
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.call(size));
}

testGrowInternalAnyFuncTable(0);
testGrowInternalAnyFuncTable(7);
testGrowInternalAnyFuncTable(9);

(function testGrowImportedTable() {
  print(arguments.callee.name);

  let size = 3;
  const builder = new WasmModuleBuilder();
  const table_index = builder.addImportedTable("imp", "table", size, undefined, kWasmAnyRef);
  builder.addFunction('grow', kSig_i_ri)
    .addBody([kExprGetLocal, 0,
      kExprGetLocal, 1,
      kNumericPrefix, kExprTableGrow, table_index])
    .exportFunc();

  builder.addFunction('size', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, table_index])
    .exportFunc();

  const table = new WebAssembly.Table({element: "anyref", initial: size});

  const instance = builder.instantiate({imp: {table: table}});
  assertEquals(null, table.get(size - 2));

  function growAndCheck(element, grow_by) {
    assertEquals(size, instance.exports.size());
    assertEquals(size, instance.exports.grow(element, grow_by));
    for (let i = 0; i < grow_by; ++i) {
      assertEquals(element, table.get(size + i));
    }
    size += grow_by;
  }
  growAndCheck("Hello", 3);
  growAndCheck(undefined, 4);
  growAndCheck(4, 2);
  growAndCheck({ Hello: "World" }, 3);
  growAndCheck(null, 2);
})();

(function testGrowTableOutOfBounds() {
  print(arguments.callee.name);

  const initial = 3;
  const maximum = 10;
  const max_delta = maximum - initial;
  const invalid_delta = max_delta + 1;

  const builder = new WasmModuleBuilder();
  const import_ref = builder.addImportedTable(
    "imp", "table_ref", initial, maximum, kWasmAnyRef);
  const import_func = builder.addImportedTable(
    "imp", "table_func", initial, maximum, kWasmAnyFunc);
  const internal_ref = builder.addTable(kWasmAnyRef, initial, maximum).index;
  const internal_func = builder.addTable(kWasmAnyFunc, initial, maximum).index;

  builder.addFunction('grow_imported_ref', kSig_i_ri)
  .addBody([kExprGetLocal, 0,
    kExprGetLocal, 1,
    kNumericPrefix, kExprTableGrow, import_ref])
  .exportFunc();

  builder.addFunction('grow_imported_func', kSig_i_ai)
  .addBody([kExprGetLocal, 0,
    kExprGetLocal, 1,
    kNumericPrefix, kExprTableGrow, import_func])
  .exportFunc();

  builder.addFunction('grow_internal_ref', kSig_i_ri)
  .addBody([kExprGetLocal, 0,
    kExprGetLocal, 1,
    kNumericPrefix, kExprTableGrow, internal_ref])
  .exportFunc();

  builder.addFunction('grow_internal_func', kSig_i_ai)
  .addBody([kExprGetLocal, 0,
    kExprGetLocal, 1,
    kNumericPrefix, kExprTableGrow, internal_func])
  .exportFunc();

  builder.addFunction('size_imported_ref', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, import_ref])
    .exportFunc();

  builder.addFunction('size_imported_func', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, import_func])
    .exportFunc();

  builder.addFunction('size_internal_ref', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, internal_ref])
    .exportFunc();

  builder.addFunction('size_internal_func', kSig_i_v)
    .addBody([kNumericPrefix, kExprTableSize, internal_func])
    .exportFunc();

  const table_ref = new WebAssembly.Table(
    { element: "anyref", initial: initial, maximum: maximum });
  const table_func = new WebAssembly.Table(
    {element: "anyfunc", initial: initial, maximum: maximum});

  const instance = builder.instantiate(
    {imp: {table_ref: table_ref, table_func: table_func}});

  const ref = { foo: "bar" };
  const func = dummy_func(17);

  // First check that growing out-of-bounds is not possible.
  assertEquals(-1, instance.exports.grow_imported_ref(ref, invalid_delta));
  assertEquals(initial, table_ref.length);
  assertEquals(initial, instance.exports.size_imported_ref());
  assertEquals(-1, instance.exports.grow_imported_func(func, invalid_delta));
  assertEquals(initial, table_func.length);
  assertEquals(initial, instance.exports.size_imported_func());
  assertEquals(-1, instance.exports.grow_internal_ref(ref, invalid_delta));
  assertEquals(initial, instance.exports.size_internal_ref());
  assertEquals(-1, instance.exports.grow_internal_func(func, invalid_delta));
  assertEquals(initial, instance.exports.size_internal_func());

  // Check that we can grow to the maximum size.
  assertEquals(initial, instance.exports.grow_imported_ref(ref, max_delta));
  assertEquals(maximum, table_ref.length);
  assertEquals(maximum, instance.exports.size_imported_ref());
  assertEquals(initial, instance.exports.grow_imported_func(func, max_delta));
  assertEquals(maximum, table_func.length);
  assertEquals(maximum, instance.exports.size_imported_func());
  assertEquals(initial, instance.exports.grow_internal_ref(ref, max_delta));
  assertEquals(maximum, instance.exports.size_internal_ref());
  assertEquals(initial, instance.exports.grow_internal_func(func, max_delta));
  assertEquals(maximum, instance.exports.size_internal_func());
})();
