// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let array_extern = builder.addArray(kWasmExternRef, {final: true});
let array_func = builder.addArray(kWasmFuncRef, {final: true});
let array_i8 = builder.addArray(kWasmI8, {final: true});
let data_bytes = [
    2, // num_prototypes
    1, // has_constructor
    1, // name_length
    65, // 'A'
    0, // num_statics
    0, // num_methods
    127, // parent_idx = -1
    0, // has_constructor
    0, // num_methods
    0  // parent_idx = 0
];
let data_segment = builder.addPassiveDataSegment(data_bytes);
let configureAll = builder.addImport(
    'wasm:js-prototypes', 'configureAll',
    makeSig([wasmRefNullType(array_extern),
             wasmRefNullType(array_func),
             wasmRefNullType(array_i8),
             kWasmExternRef], []));

let dummy = builder.addFunction('', makeSig([], [])).addBody([]).exportFunc();

builder.addFunction(
    'test', makeSig([wasmRefNullType(array_extern), kWasmExternRef], []))
  .addBody([
    kExprLocalGet, 0,  // Prototypes
    kExprRefFunc, dummy.index,
    kGCPrefix, kExprArrayNewFixed, array_func, 1,  // Functions
    kExprI32Const, 0,
    kExprI32Const, data_bytes.length,
    kGCPrefix, kExprArrayNewData, array_i8, data_segment,  // Data
    kExprLocalGet, 1,  // Constructors
    kExprCallFunction, configureAll,
  ]).exportFunc();

builder.addFunction(
    'create_prototypes',
    makeSig([kWasmExternRef, kWasmExternRef], [wasmRefType(array_extern)]))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kGCPrefix, kExprArrayNewFixed, array_extern, 2,
  ]).exportFunc();

builder.addFunction(
    'array_set',
    makeSig([wasmRefType(array_extern), kWasmI32, kWasmExternRef], []))
  .addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kGCPrefix, kExprArraySet, array_extern,
  ]).exportFunc();

let instance = builder.instantiate({}, { builtins: ["js-prototypes"] });
let proxy1 = new Proxy({}, {
    defineProperty() {
      instance.exports.array_set(prototypes, 0, undefined);
      return true;
    }
});
let proxy2 = new Proxy({}, {});
let prototypes = instance.exports.create_prototypes(proxy1, proxy2);
let constructors = {};
assertThrows(
    () => instance.exports.test(prototypes, constructors), TypeError,
    "Object prototype may only be an Object or null: undefined");
