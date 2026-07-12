// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let $struct = builder.addStruct({ fields: [] });
let $array_externref =
  builder.addArray(kWasmExternRef, true, kNoSuperType, true);
let $array_funcref = builder.addArray(kWasmFuncRef, true, kNoSuperType, true);
let $array_i8 = builder.addArray(kWasmI8, true, kNoSuperType, true);
let $configureAll = builder.addImport(
  "wasm:js-prototypes", "configureAll",
  makeSig([wasmRefNullType($array_externref),
           wasmRefNullType($array_funcref),
           wasmRefNullType($array_i8), kWasmExternRef], []));

let $constructors = builder.addImportedGlobal(
  "c", "constructors", kWasmExternRef, false);

let $ctor = builder.addFunction("ctor", kSig_v_v).addBody([]);

let func_seg = builder.addPassiveElementSegment([$ctor.index]);

builder.addFunction("make_struct", kSig_r_v).exportFunc().addBody([
  kGCPrefix, kExprStructNew, $struct,
  kGCPrefix, kExprExternConvertAny,
]);

{
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
}
{
  let data = [
    1,  // number of prototypes to configure
    0,  // no constructor (nor statics)
    1,  // one method
    0,  // kind=method
    1, 48,  // "0"
    0x7f,   // no parent
  ];
  let data_seg = builder.addPassiveDataSegment(data);

  builder.addFunction("bad_name", kSig_v_r).exportFunc().addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprArrayNewFixed, $array_externref, 1,
    kExprI32Const, 0,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_funcref, func_seg,
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(data.length),
    kGCPrefix, kExprArrayNewData, $array_i8, data_seg,
    kExprGlobalGet, $constructors,
    kExprCallFunction, $configureAll,
  ]);
}

let imports = {c: {constructors: {}}};
let builtins = ["js-prototypes"];
let instance = builder.instantiate(imports, { builtins });

instance.exports.bad_proto(this);

let p = {};
instance.exports.bad_name(p);
assertEquals("function", typeof p[0]);
