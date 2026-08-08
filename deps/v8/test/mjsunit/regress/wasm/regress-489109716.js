// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop --nowasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $struct = builder.addStruct([]);
let $array_externref = builder.addArray(kWasmExternRef, {final: true});
let $array_funcref = builder.addArray(kWasmFuncRef, {final: true});
let $array_i8 = builder.addArray(kWasmI8, {final: true});
let $configureAll = builder.addImport(
  "wasm:js-prototypes", "configureAll",
  makeSig([wasmRefNullType($array_externref),
           wasmRefNullType($array_funcref),
           wasmRefNullType($array_i8), kWasmExternRef], [kWasmI32]));

let $constructors = builder.addImportedGlobal(
  "c", "constructors", kWasmExternRef, false);

let $ctor = builder.addFunction("ctor", kSig_v_v).addBody([]);

let func_seg = builder.addPassiveElementSegment([$ctor.index]);

builder.addFunction("make_struct", kSig_r_v).exportFunc().addBody([
  kGCPrefix, kExprStructNew, $struct,
  kGCPrefix, kExprExternConvertAny,
]);

let proto_segment = builder.addPassiveElementSegment([], kWasmExternRef);
let func_segment = builder.addPassiveElementSegment([]);
let data = [
  1,  // number of prototypes to configure
  0,  // no constructor
  0,  // no methods
  0x7f,  // no parent
];
let data_seg = builder.addPassiveDataSegment(data);

builder.addFunction("bad", kSig_i_ii).exportFunc().addBody([
  kExprI32Const, 0,
  kExprI32Const, 0,
  kGCPrefix, kExprArrayNewElem, $array_externref, proto_segment,

  kExprI32Const, 0,
  kExprI32Const, 0,
  kGCPrefix, kExprArrayNewElem, $array_funcref, func_segment,

  kExprI32Const, 0,
  kExprI32Const, ...wasmSignedLeb(data.length),
  kGCPrefix, kExprArrayNewData, $array_i8, data_seg,
  kExprGlobalGet, $constructors,
  kExprCallFunction, $configureAll,
]);

let instance = builder.instantiate({
  c: {constructors: {}},
  "wasm:js-prototypes": {
    "configureAll": function() {
      return 42;
    }
  }
});
assertEquals(42, instance.exports.bad(1, 2));
