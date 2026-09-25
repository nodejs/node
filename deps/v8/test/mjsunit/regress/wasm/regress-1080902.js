// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let memory = new WebAssembly.Memory({
  initial: 1,
  maximum: 10,
  shared: true
});

let builder = new WasmModuleBuilder();
builder.addImportedMemory("m", "imported_mem", 0, 1 << 16, "shared");
builder.addFunction("main", kSig_i_v).addBody([
    kExprI32Const, 0,
    kAtomicPrefix,
    kExprI32AtomicLoad16U, 1, 0]).exportAs("main");
let module = new WebAssembly.Module(builder.toBuffer());
let instance = new WebAssembly.Instance(module, {
  m: {
    imported_mem: memory
  }
});
instance.exports.main();
