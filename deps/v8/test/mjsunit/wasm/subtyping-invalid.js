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
  builder.addStruct({fields: [makeField(wasmRefType(0), false)]});
  builder.addStruct({fields: [makeField(wasmRefType(1), false)], supertype: 0});
}, OK);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(1), false)]});
  builder.addStruct({fields: [makeField(wasmRefType(0), false)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(0), true)]});
  builder.addStruct({fields: [makeField(wasmRefType(1), true)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(1), false)]});
  builder.addStruct({fields: [makeField(wasmRefType(1), true)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(1), true)]});
  builder.addStruct({fields: [makeField(wasmRefType(1), false)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(0), false)], final: true});
  builder.addStruct({fields: [makeField(wasmRefType(1), false)], supertype: 0});
}, /type 1 extends final type 0/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(0), false)], shared: true});
  builder.addStruct({fields: [makeField(wasmRefType(1), false)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: [makeField(wasmRefType(0), false)]});
  builder.addStruct({fields: [makeField(wasmRefType(1), false)],
                     supertype: 0, shared: true});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addStruct({fields: []});
  builder.addStruct({fields: [makeField(wasmRefType(0), false)], shared: true});
}, /shared struct must have shared field types/);

Test((builder) => {
  builder.addStruct({fields: [makeField(kWasmI32, true),
                              makeField(kWasmI32, true)]});
  builder.addStruct({fields: [makeField(kWasmI32, true)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, {mutable: false});
  builder.addStruct({fields: [makeField(kWasmI32, false)], supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32);
  builder.addArray(kWasmI32, {mutable: false, supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32, {mutable: false});
  builder.addArray(kWasmI32, {mutable: true, supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0));
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0));
  builder.addArray(wasmRefType(1), {supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0));
  builder.addArray(wasmRefType(0), {supertype: 0});
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 0});
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 0});
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(0), {mutable: false, supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(kWasmI32);
  builder.addArray(wasmRefType(0), {final: true, shared: true});
}, /shared array must have shared element type/);

Test((builder) => {
  builder.addArray(kWasmI32, {shared: true});
  builder.addArray(wasmRefType(0));
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
}, (builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 1});
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
}, (builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(0), {mutable: false, supertype: 0});
}, OK);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
}, (builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 0});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
}, (builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
  builder.addArray(wasmRefType(0), {mutable: false, supertype: 1});
}, /invalid explicit supertype/);

Test((builder) => {
  builder.addArray(wasmRefType(1), {mutable: false});
}, (builder) => {
  builder.addArray(wasmRefType(1), {mutable: false, supertype: 0});
}, /Type index 1 is out of bounds/);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false, supertype: 1});
  builder.addArray(wasmRefType(0), {mutable: false});
}, /invalid supertype 1/);

Test((builder) => {
  builder.addArray(wasmRefType(0), {mutable: false, supertype: 1});
}, (builder) => {
  builder.addArray(wasmRefType(0), {mutable: false});
}, /invalid supertype 1/);
