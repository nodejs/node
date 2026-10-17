// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-verify-load-elimination

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

let kSig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);
let charCodeAt = builder.addImport("wasm:js-string", "charCodeAt", kSig_i_ri);

builder.addFunction("main", kSig_i_r)
  .addBody([
    kExprLocalGet, 0,
    kExprRefAsNonNull,
    kExprI32Const, 0,
    kExprCallFunction, charCodeAt,

    kExprLocalGet, 0,
    kExprI32Const, 0,
    kExprCallFunction, charCodeAt,

    kExprI32Add,
  ])
  .exportFunc();

let instance = builder.instantiate({}, { builtins: ["js-string"] });
instance.exports.main("abcd");
