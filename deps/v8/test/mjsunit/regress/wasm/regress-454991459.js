// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let memory = new WebAssembly.Memory({ initial: 1, maximum: 2, shared: true });
let builder = new WasmModuleBuilder();
builder.addImportedMemory("m", "memory", 1, 2, "shared");
builder.addFunction("wait", kSig_i_ii)
  .addBody([
    kExprLocalGet, 0, // address
    kExprLocalGet, 1, // expected_value
    kExprI64Const, 0, // timeout
    kAtomicPrefix, kExprI32AtomicWait, 2, 0
  ])
  .exportFunc();
let instance = builder.instantiate({m: {memory}});
memory.grow(1);
instance.exports.wait(kPageSize);
