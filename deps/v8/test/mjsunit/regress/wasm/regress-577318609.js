// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addFunction('test', kSig_v_v)
  .addLocals(wasmRefNullType(kWasmWaitqueueRef).shared(), 1)
  .addBody([
    kAtomicPrefix, kExprWaitqueueNew,
    kExprLocalSet, 0,
    kExprLocalGet, 0,
    kGCPrefix, kExprRefCastNull, kWasmSharedTypeForm, kNullWaitqueueRefCode,
    kExprDrop,
  ])
  .exportFunc();

let instance = builder.instantiate();
assertThrows(() => instance.exports.test(), WebAssembly.RuntimeError,
             /illegal cast/);
