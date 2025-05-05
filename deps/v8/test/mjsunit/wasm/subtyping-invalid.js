// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const OK = true;

// {recgroup2} is optional.
function Test(recgroup1, recgroup2, result) {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  recgroup1(builder);
  builder.endRecGroup();
  if (typeof recgroup2 === "function") {
    builder.startRecGroup();
    recgroup2(builder);
    builder.endRecGroup();
  } else {
    result = recgroup2;
  }
  if (result === OK) {
    return builder.instantiate();  // Does not throw.
  }
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError, result);
}

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(0), false)]);
  builder.addStruct([makeField(wasmRefType(1), false)], 0);
}, OK);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(1), false)]);
  builder.addStruct([makeField(wasmRefType(0), false)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(0), true)]);
  builder.addStruct([makeField(wasmRefType(1), true)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(1), false)]);
  builder.addStruct([makeField(wasmRefType(1), true)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(1), true)]);
  builder.addStruct([makeField(wasmRefType(1), false)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(0), false)], kNoSuperType, true);
  builder.addStruct([makeField(wasmRefType(1), false)], 0);
}, /type 1 extends final type 0/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(0), false)], kNoSuperType, false, true);
  builder.addStruct([makeField(wasmRefType(1), false)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([makeField(wasmRefType(0), false)]);
  builder.addStruct([makeField(wasmRefType(1), false)], 0, false, true);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct([]);
  builder.addStruct([makeField(wasmRefType(0), false)], kNoSuperType, false, true);
}, /shared struct must have shared field types/);

Test((builder) => {
  builder.addStruct([makeField(kWasmI32, true), makeField(kWasmI32, true)]);
  builder.addStruct([makeField(kWasmI32, true)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, false);
  builder.addStruct([makeField(kWasmI32, false)], 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, true);
  builder.addArray(kWasmI32, false, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, false);
  builder.addArray(kWasmI32, true, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0), true);
  builder.addArray(wasmRefType(1), false, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0), true);
  builder.addArray(wasmRefType(1), true, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0), true);
  builder.addArray(wasmRefType(0), true, 0);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), false);
  builder.addArray(wasmRefType(1), false, 0);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(1), false, 0);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(0), false, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, true);
  builder.addArray(wasmRefType(0), true, kNoSuperType, true, true);
}, /shared array must have shared element type/);

Test((builder) => {
  builder.addArray(kWasmI32, true, kNoSuperType, false, true);
  builder.addArray(wasmRefType(0), true);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), false);
}, (builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(1), false, 1);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), false);
}, (builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(0), false, 0);
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), false);
}, (builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(1), false, 0);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0), false);
}, (builder) => {
  builder.addArray(wasmRefType(1), false);
  builder.addArray(wasmRefType(0), false, 1);
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(1), false);
}, (builder) => {
  builder.addArray(wasmRefType(1), false, 0);
}, /Type index 1 is out of bounds/);

Test((builder) => {
  builder.addArray(wasmRefType(0), false, 1);
  builder.addArray(wasmRefType(0), false);
}, /invalid supertype 1/);

Test((builder) => {
  builder.addArray(wasmRefType(0), false, 1);
}, (builder) => {
  builder.addArray(wasmRefType(0), false);
}, /invalid supertype 1/);
