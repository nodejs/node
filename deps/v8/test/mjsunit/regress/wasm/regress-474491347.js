// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop --harmony-struct

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $array_externref =
  builder.addArray(kWasmExternRef, true, kNoSuperType, true);
let $array_funcref = builder.addArray(kWasmFuncRef, true, kNoSuperType, true);
let $array_i8 = builder.addArray(kWasmI8, true, kNoSuperType, true);
let $configureAll = builder.addImport(
  "wasm:js-prototypes", "configureAll",
  makeSig([wasmRefNullType($array_externref),
           wasmRefNullType($array_funcref),
           wasmRefNullType($array_i8), kWasmExternRef], []));

let data = [
  1,  // number of prototypes to configure
  0,  // no constructor
  0,  // no methods
  0x7f,  // no parent
];
let data_seg = builder.addPassiveDataSegment(data);

builder.addFunction("bad_proto", kSig_v_r).exportFunc().addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprArrayNewFixed, $array_externref, 1,
  kGCPrefix, kExprArrayNewFixed, $array_funcref, 0,
  kExprI32Const, 0,
  kExprI32Const, ...wasmSignedLeb(data.length),
  kGCPrefix, kExprArrayNewData, $array_i8, data_seg,
  kExprRefNull, kExternRefCode,
  kExprCallFunction, $configureAll,
]);

let builtins = ["js-prototypes"];
let instance = builder.instantiate({}, { builtins });

const t0 = this.SharedStructType(['a']);
const shared_struct = new t0();
assertTraps(kTrapIllegalCast, () => instance.exports.bad_proto(shared_struct));
