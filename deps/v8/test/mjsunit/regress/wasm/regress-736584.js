// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let mem = new WebAssembly.Memory({initial: 0});
let builder = new WasmModuleBuilder();
builder.addImportedMemory("mod", "imported_mem");
builder.addFunction('mem_size', kSig_i_v)
    .addBody([kExprMemorySize, kMemoryZero])
    .exportFunc();
let instance = builder.instantiate({mod: {imported_mem: mem}});
instance.exports.mem_size();
