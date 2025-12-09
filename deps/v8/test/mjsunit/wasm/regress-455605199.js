// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-lazy-compilation

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 0]).exportFunc();
const module = builder.toModule();
const serialized = d8.wasm.serializeModule(module);

const arr = new Uint8Array(new ArrayBuffer(8));
const imports = new Proxy({}, {
  get() {
    arr.buffer.transfer();
  }
});
assertThrows(
    () => d8.wasm.deserializeModule(serialized, arr, imports), Error,
    /buffer is detached/);
