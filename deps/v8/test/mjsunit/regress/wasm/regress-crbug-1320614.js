// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

var builder = new WasmModuleBuilder();
let gc_func = builder.addImport("imports", "gc", { params: [], results: [] });
let callee = builder.addFunction('callee', {
  params: [
    // More tagged parameters than we can pass in registers on any platform.
    kWasmExternRef, kWasmExternRef, kWasmExternRef, kWasmExternRef,
    kWasmExternRef,
    // An untagged parameter to trip up the stack walker.
    kWasmI32, kWasmExternRef,
  ],
  results: [kWasmI64]  // An i64 to trigger replacement.
}).addBody([kExprCallFunction, gc_func, kExprI64Const, 0]);

builder.addFunction("main", { params: [], results: [] }).addBody([
  kExprRefNull, kExternRefCode,
  kExprRefNull, kExternRefCode,
  kExprRefNull, kExternRefCode,
  kExprRefNull, kExternRefCode,
  kExprRefNull, kExternRefCode,
  kExprI32Const, 0xf,
  kExprRefNull, kExternRefCode,
  kExprCallFunction, callee.index, kExprDrop
]).exportFunc();

var instance = builder.instantiate({ imports: { gc: () => { gc(); } } });
instance.exports.main();
