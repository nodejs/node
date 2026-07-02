// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $array_externref = builder.addArray(kWasmExternRef, {final: true});
let $array_funcref = builder.addArray(kWasmFuncRef, {final: true});
let $array_i8 = builder.addArray(kWasmI8, {final: true});

let $configureAll = builder.addImport(
  "wasm:js-prototypes", "configureAll",
  makeSig([wasmRefNullType($array_externref),
           wasmRefNullType($array_funcref),
           wasmRefNullType($array_i8), kWasmExternRef], []));

let $import = builder.addImport("m", "f", kSig_r_v);

let $internal = builder.addFunction("internal", kSig_r_v).exportFunc().addBody([
  kExprI32Const, 43,
  kGCPrefix, kExprRefI31,
  kGCPrefix, kExprExternConvertAny,
]);

let $constructors = builder.addImportedGlobal(
  "c", "constructors", kWasmExternRef, false);
let $p1 = builder.addImportedGlobal("c", "p1", kWasmExternRef, false);
let $p2 = builder.addImportedGlobal("c", "p2", kWasmExternRef, false);

let func_seg = builder.addPassiveElementSegment(
  [[kExprRefFunc, $import], [kExprRefFunc, $internal.index]], kWasmFuncRef);

let proto_seg = builder.addPassiveElementSegment(
  [[kExprGlobalGet, $p1], [kExprGlobalGet, $p2]], kWasmExternRef);

let data = [
  2,  // number of prototypes
  1,  // has constructor
  1, 97,  // name "a"
  0,  // no statics
  0,  // no methods
  0x7f,  // no parent
  1,  // has constructor
  1, 98,  // name "b"
  0,  // no statics
  0,  // no methods
  0x7f,  // no parent
];
let data_seg = builder.addPassiveDataSegment(data);

builder.addFunction("test", kSig_v_v).exportFunc().addBody([
  kExprI32Const, 0,
  kExprI32Const, 2,
  kGCPrefix, kExprArrayNewElem, $array_externref, proto_seg,
  kExprI32Const, 0,
  kExprI32Const, 2,
  kGCPrefix, kExprArrayNewElem, $array_funcref, func_seg,
  kExprI32Const, 0,
  kExprI32Const, ...wasmSignedLeb(data.length),
  kGCPrefix, kExprArrayNewData, $array_i8, data_seg,
  kExprGlobalGet, $constructors,
  kExprCallFunction, $configureAll,
]);

let constructors_obj = {};
let imports = {
  m: { f: () => 42 },
  c: {
    constructors: constructors_obj,
    p1: { name: "proto1" },
    p2: { name: "proto2" },
  }
};
let builtins = ["js-prototypes"];
let instance = builder.instantiate(imports, {builtins});

instance.exports.test();

let C = constructors_obj.a;
let D = constructors_obj.b;

assertEquals("object", typeof Array.from.call(C, [1, 2]));
assertEquals("object", typeof Array.from.call(D, [1, 2]));
