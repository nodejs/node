// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --nowasm-lazy-compilation
// Flags: --wasm-caching-threshold=0 --wasm-caching-timeout-ms=0

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

let builder = new WasmModuleBuilder();
let f0 = builder.addFunction('f0', kSig_i_i).addBody([kExprLocalGet, 0]);
let wasm_bytes = builder.toBuffer();
let fake_response = {
  then(resolve) {
    resolve(wasm_bytes);
  }
};
WebAssembly.compileStreaming(fake_response).catch();
