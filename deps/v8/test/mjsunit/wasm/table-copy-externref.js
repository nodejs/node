// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let kTableSize = 5;

let table = new WebAssembly.Table(
    {element: 'externref', initial: kTableSize, maximum: kTableSize});

let builder = new WasmModuleBuilder();
builder.addImportedTable('m', 'table', kTableSize, kTableSize, kWasmExternRef);
builder.addTable(kWasmAnyFunc, 1000);

builder.addFunction('copy', kSig_v_iii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kNumericPrefix,
      kExprTableCopy, kTableZero, kTableZero
    ])
    .exportFunc();

const instance = builder.instantiate({m: {table: table}});

function resetTable() {
  table.set(0, 1000);
  table.set(1, 1001);
  table.set(2, 1002);
  table.set(3, 1003);
  table.set(4, 1004);
}

function assertTable(values) {
  for (let i = 0; i < kTableSize; ++i) {
    assertEquals(table.get(i), values[i]);
  }
}

resetTable();
instance.exports.copy(0, 1, 1);
assertTable([1001, 1001, 1002, 1003, 1004]);

resetTable();
instance.exports.copy(0, 1, 2);
assertTable([1001, 1002, 1002, 1003, 1004]);

resetTable();
instance.exports.copy(3, 0, 2);
assertTable([1000, 1001, 1002, 1000, 1001]);

// Non-overlapping, src < dst. Because of src < dst, we copy backwards.
// Therefore the first access already traps, and the table is not changed.
resetTable();
assertTraps(kTrapTableOutOfBounds, () => instance.exports.copy(3, 0, 3));
assertTable([1000, 1001, 1002, 1003, 1004]);

// Non-overlapping, dst < src.
resetTable();
assertTraps(kTrapTableOutOfBounds, () => instance.exports.copy(0, 4, 2));
assertTable([1000, 1001, 1002, 1003, 1004]);

// Overlapping, src < dst. This is required to copy backward, but the first
// access will be out-of-bounds, so nothing changes.
resetTable();
assertTraps(kTrapTableOutOfBounds, () => instance.exports.copy(3, 0, 99));
assertTable([1000, 1001, 1002, 1003, 1004]);

// Overlapping, dst < src.
resetTable();
assertTraps(kTrapTableOutOfBounds, () => instance.exports.copy(0, 1, 99));
assertTable([1000, 1001, 1002, 1003, 1004]);
