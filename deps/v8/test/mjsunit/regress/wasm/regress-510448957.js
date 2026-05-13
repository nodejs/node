// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let wq_type = wasmRefType(kWasmWaitqueueRef).shared();
let tag_index = builder.addTag(makeSig([wq_type], []));
builder.addFunction("throw_wq", makeSig([], []))
  .addBody([
    kAtomicPrefix, kExprWaitqueueNew,
    kExprThrow, tag_index
  ])
  .exportFunc();
try {
    let instance = builder.instantiate();
    instance.exports.throw_wq();
} catch (e) {
    let values = %GetWasmExceptionValues(e);
}
