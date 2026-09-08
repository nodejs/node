// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wasm-generic-wrapper

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const num_repetitions = 10_000_000;

(function MeasureAbstractStructTypeWrapper() {
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  builder.addFunction("producer", makeSig([], [kWasmStructRef]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct])
    .exportFunc();

  builder.addFunction("id", makeSig([kWasmStructRef], [kWasmStructRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let struct_obj = wasm.producer();

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(struct_obj);
  }

  print("Abstract type result: " + (Date.now() - start) + "ms");
})();

(function MeasureIndexedStructTypeWrapper() {
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  let struct_type = wasmRefNullType(struct);

  builder.addFunction("producer", makeSig([], [struct_type]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct])
    .exportFunc();

  builder.addFunction("id", makeSig([struct_type], [struct_type]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let struct_obj = wasm.producer();

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(struct_obj);
  }

  print("Indexed type result: " + (Date.now() - start) + "ms");
})();

(function MeasureIndexedStructSubtypeWrapper() {
  let builder = new WasmModuleBuilder();

  let super_struct = builder.addStruct([makeField(kWasmI32, true)]);
  let sub_struct = builder.addStruct({
    fields: [makeField(kWasmI32, true), makeField(kWasmI32, true)],
    supertype: super_struct
  });

  builder.addFunction("producer", makeSig([], [wasmRefType(sub_struct)]))
    .addBody([kGCPrefix, kExprStructNewDefault, sub_struct])
    .exportFunc();

  builder.addFunction("id", makeSig([wasmRefNullType(super_struct)],
                                    [wasmRefNullType(super_struct)]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let struct_obj = wasm.producer();

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(struct_obj);
  }

  print("Indexed subtype result: " + (Date.now() - start) + "ms");
})();

(function MeasureI31() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("id", makeSig([kWasmI31Ref], [kWasmI31Ref]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(42);
  }

  print("i31 result: " + (Date.now() - start) + "ms");
})();

(function MeasureEqI31() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("id", makeSig([kWasmEqRef], [kWasmEqRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(42);
  }

  print("eq-i31 result: " + (Date.now() - start) + "ms");
})();

(function MeasureEqWasmObject() {
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct({fields: [makeField(kWasmI32, true)]});

  builder.addFunction("producer", makeSig([], [wasmRefType(struct)]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct])
    .exportFunc();

  builder.addFunction("id", makeSig([kWasmEqRef], [kWasmEqRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  let struct_obj = wasm.producer();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(struct_obj);
  }

  print("eq-wasm object result: " + (Date.now() - start) + "ms");
})();

(function MeasureAnyI31() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("id", makeSig([kWasmAnyRef], [kWasmAnyRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(42);
  }

  print("any-i31 result: " + (Date.now() - start) + "ms");
})();

(function MeasureAnyWasmObject() {
  let builder = new WasmModuleBuilder();

  let struct = builder.addStruct({fields: [makeField(kWasmI32, true)]});

  builder.addFunction("producer", makeSig([], [wasmRefType(struct)]))
    .addBody([kGCPrefix, kExprStructNewDefault, struct])
    .exportFunc();

  builder.addFunction("id", makeSig([kWasmAnyRef], [kWasmAnyRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  let struct_obj = wasm.producer();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id(struct_obj);
  }

  print("any-wasm object result: " + (Date.now() - start) + "ms");
})();

(function MeasureAnyJSObject() {
  let builder = new WasmModuleBuilder();

  builder.addFunction("id", makeSig([kWasmAnyRef], [kWasmAnyRef]))
    .addBody([kExprLocalGet, 0])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  let start = Date.now();

  for (let i = 0; i < num_repetitions; i++) {
    wasm.id({});
  }

  print("any-JS object result: " + (Date.now() - start) + "ms");
})();
