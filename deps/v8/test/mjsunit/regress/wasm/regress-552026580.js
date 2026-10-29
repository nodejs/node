// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let kSharedRefExtern = wasmRefType(kWasmExternRef).shared();

let sig_f64_to_shared_extern =
    builder.addType(makeSig([kWasmF64], [kSharedRefExtern]));
let double_to_string_import =
    builder.addImport('mod', 'doubleToString', sig_f64_to_shared_extern);

let struct = builder.addStruct(
    {fields: [makeField(kSharedRefExtern, true)], shared: true});

builder.addFunction("test", makeSig([kWasmF64], [wasmRefType(struct)]))
  .addBody([
    kExprLocalGet, 0,
    kExprCallFunction, double_to_string_import,
    kGCPrefix, kExprStructNew, struct
  ])
  .exportFunc();

let instance = builder.instantiate({
  mod: {
    doubleToString: Function.prototype.call.bind(Number.prototype.toString)
  }
});
instance.exports.test(3.14159);
