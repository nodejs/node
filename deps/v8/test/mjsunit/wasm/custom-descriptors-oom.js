// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

const final = true;
let array_externref =
  builder.addArray(kWasmExternRef, {final});
let array_funcref = builder.addArray(kWasmFuncRef, {final});
let array_i8 = builder.addArray(kWasmI8, {final});

let sig_index = builder.addType(makeSig([
  wasmRefNullType(array_externref),
  wasmRefNullType(array_funcref),
  wasmRefNullType(array_i8),
  kWasmExternRef
], []));

let configureAll =
  builder.addImport("wasm:js-prototypes", "configureAll", sig_index);
let constructors =
  builder.addImportedGlobal("c", "constructors", kWasmExternRef, false);

// Data payload 1: too many static properties
let data_statics = [
  1, // num_prototypes
  1, // has_constructor
  1, // constructor_name_length
  65, // "A"
  ...wasmUnsignedLeb(0xFFFFFFF) // num_statics
];
let data_segment_statics = builder.addPassiveDataSegment(data_statics);

// Data payload 2: too many methods
let data_methods = [
  1, // num_prototypes
  0, // has_constructor
  ...wasmUnsignedLeb(0xFFFFFFF) // num_methods
];
let data_segment_methods = builder.addPassiveDataSegment(data_methods);

let dummy_func =
  builder.addFunction("dummy", kSig_v_v).addBody([]).exportFunc();

let addRunFunction = (name, data_length, data_segment) => {
  builder.addFunction(name, kSig_v_v)
    .addBody([
      // array of prototypes
      kExprGlobalGet, constructors,
      kGCPrefix, kExprArrayNewFixed, array_externref, 1,

      // array of functions
      kExprRefFunc, dummy_func.index,
      kGCPrefix, kExprArrayNewFixed, array_funcref, 1,

      // array of data bytes
      kExprI32Const, 0,
      kExprI32Const, data_length,
      kGCPrefix, kExprArrayNewData, array_i8, data_segment,

      // constructors
      kExprGlobalGet, constructors,

      kExprCallFunction, configureAll
    ])
    .exportFunc();
};

addRunFunction("run_statics", data_statics.length, data_segment_statics);
addRunFunction("run_methods", data_methods.length, data_segment_methods);

let instance = builder.instantiate(
  {"c": { "constructors": {} }},
  { builtins: ['js-prototypes'] }
);

assertThrows(instance.exports.run_statics,
  RangeError, /too many constructor properties in configureAll/);
assertThrows(instance.exports.run_methods,
  RangeError, /too many methods in configureAll/);
