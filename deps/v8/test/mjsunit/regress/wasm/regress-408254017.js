// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-jspi
// Flags: --stress-wasm-stack-switching

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
builder.addMemory(0);
import_index = builder.addImport('m', 'suspending', kSig_v_v);
builder.addFunction("test", kSig_v_v).addBody([
    kExprCallFunction, import_index
]).exportFunc();
builder.addFunction("grow", kSig_i_v).addBody([
    ...wasmI32Const(40000), kExprMemoryGrow, kMemoryZero
]).exportFunc();
let suspending = new WebAssembly.Suspending(() => Promise.resolve());

let instance = builder.instantiate({m: { suspending } });
try {
  instance.exports.test();
} catch (e) {
}
instance.exports.grow();
