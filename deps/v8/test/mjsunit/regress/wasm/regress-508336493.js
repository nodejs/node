// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let wq_type = wasmRefType(kWasmWaitqueueRef).shared();
builder.addGlobal(wq_type, true, false, [kAtomicPrefix, kExprWaitqueueNew]);
builder.addExportOfKind("g", kExternalGlobal, 0);
let instance = builder.instantiate();

assertThrows(() => instance.exports.g.value, TypeError, /invalid type/);
