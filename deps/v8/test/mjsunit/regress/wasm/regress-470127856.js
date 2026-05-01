// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-js-interop

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

let $constructors = builder.addImportedGlobal(
  "c", "constructors", kWasmExternRef, false);
let $prototype = builder.addImportedGlobal(
  "c", "prototype", kWasmExternRef, false);

let $ctor = builder.addFunction("ctor", kSig_v_v).addBody([]);

let func_seg = builder.addPassiveElementSegment(
  [[kExprRefFunc, $ctor.index], [kExprRefNull, kFuncRefCode]], kWasmFuncRef);

let proto_seg = builder.addPassiveElementSegment(
  [[kExprGlobalGet, $prototype]], kWasmExternRef);

{
  let bad_ctor_data = [
    1,  // number of prototypes to configure
    1,  // constructor
    1, 97,  // "a"
    0,  // no statics
    0,  // no methods
    0x7f,  // no parent
  ];
  let bad_ctor_data_seg = builder.addPassiveDataSegment(bad_ctor_data);

  builder.addFunction("bad_ctor", kSig_v_v).exportFunc().addBody([
    kExprI32Const, 0,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_externref, proto_seg,
    kExprI32Const, 1,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_funcref, func_seg,
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(bad_ctor_data.length),
    kGCPrefix, kExprArrayNewData, $array_i8, bad_ctor_data_seg,
    kExprGlobalGet, $constructors,
    kExprCallFunction, $configureAll,
  ]);
}
{
  let bad_method_data = [
    1,  // number of prototypes to configure
    0,  // no constructor (nor statics)
    1,  // one method
    0,  // kind=method
    1, 97,  // "a"
    0x7f,  // no parent
  ];
  let bad_method_data_seg = builder.addPassiveDataSegment(bad_method_data);

  builder.addFunction("bad_method", kSig_v_v).exportFunc().addBody([
    kExprI32Const, 0,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_externref, proto_seg,
    kExprI32Const, 1,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_funcref, func_seg,
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(bad_method_data.length),
    kGCPrefix, kExprArrayNewData, $array_i8, bad_method_data_seg,
    kExprGlobalGet, $constructors,
    kExprCallFunction, $configureAll,
  ]);
}
{
  let bad_static_data = [
    1,  // number of prototypes to configure
    1,  // constructor
    1, 97,  // "a"
    1,  // one static func
    0,  // kind=method
    1, 98,  // "b"
    0,  // no methods
    0x7f,  // no parent
  ];
  let bad_static_data_seg = builder.addPassiveDataSegment(bad_static_data);

  builder.addFunction("bad_static", kSig_v_v).exportFunc().addBody([
    kExprI32Const, 0,
    kExprI32Const, 1,
    kGCPrefix, kExprArrayNewElem, $array_externref, proto_seg,
    kExprI32Const, 0,
    kExprI32Const, 2,
    kGCPrefix, kExprArrayNewElem, $array_funcref, func_seg,
    kExprI32Const, 0,
    kExprI32Const, ...wasmSignedLeb(bad_static_data.length),
    kGCPrefix, kExprArrayNewData, $array_i8, bad_static_data_seg,
    kExprGlobalGet, $constructors,
    kExprCallFunction, $configureAll,
  ]);
}
let imports = {c: {constructors: {}, prototype: {}}};
let builtins = ["js-prototypes"];
let instance = builder.instantiate(imports, {builtins});

assertTraps(kTrapNullFunc, () => instance.exports.bad_ctor());
assertTraps(kTrapNullFunc, () => instance.exports.bad_method());
assertTraps(kTrapNullFunc, () => instance.exports.bad_static());
