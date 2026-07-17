// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const two_gb = 2 * 1024 * 1024 * 1024;
builder.addMemory(two_gb / kPageSize + 1);
builder.addFunction('load', kSig_v_v)
    .addBody([
      kExprI32Const, 0, kExprI32LoadMem, 0, ...wasmUnsignedLeb(two_gb),
      kExprDrop
    ])
    .exportFunc();
let instance;
try {
  instance = builder.instantiate();
} catch (RangeError) { }
if (instance) {
  instance.exports.load();
}
