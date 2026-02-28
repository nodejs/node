// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --flush-denormals

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_v).addBody(wasmI32Const(0)).exportFunc();
const wire_bytes = builder.toBuffer();
const module = new WebAssembly.Module(wire_bytes);
let instance = new WebAssembly.Instance(module);
instance.exports.f();
const serialized_bytes = d8.wasm.serializeModule(module);
d8.wasm.deserializeModule(serialized_bytes, wire_bytes);
