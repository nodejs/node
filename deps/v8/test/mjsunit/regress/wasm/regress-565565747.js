// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let struct_type = builder.addStruct({fields: [makeField(kWasmI32, true)],
                                     shared: true});
builder.addFunction('make_struct', makeSig([], [wasmRefType(struct_type)]))
  .addBody([kExprI32Const, 42, kGCPrefix, kExprStructNew, struct_type])
  .exportFunc();
let instance = builder.instantiate();
let shared_struct = instance.exports.make_struct();
let mem = new WebAssembly.Memory({initial: 1, maximum: 2, shared: true});
let wasm_sab = mem.buffer;
let regular_sab = new SharedArrayBuffer();
let w = new Worker('onmessage = function(e) { postMessage(1); };',
                   {type: 'string'});
w.postMessage([shared_struct, wasm_sab, regular_sab, mem]);
w.getMessage();
