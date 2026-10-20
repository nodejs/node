// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --no-liftoff --trace-turbo-graph --nodebug-code
// Flags: --no-wasm-assert-types
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();

builder.addFunction("externConvertAny",
  makeSig([wasmRefType(kWasmAnyRef)], [kWasmExternRef]))
.addBody([
  kExprLocalGet, 0,
  kGCPrefix, kExprExternConvertAny,
])
.exportFunc();

let instance = builder.instantiate();
// Check that an extern.convert_any is a no-op if the input isn't nullable.
// CHECK-LABEL: V8.TFTurboshaftWasmDeadCodeElimination
// CHECK: MERGE B0
// CHECK-NEXT: 0: Parameter()[1]
// CHECK-NEXT: 1: Constant()[word32: 0]
// CHECK-NEXT: 2: Return(#1, #0)[0]
instance.exports.externConvertAny();
