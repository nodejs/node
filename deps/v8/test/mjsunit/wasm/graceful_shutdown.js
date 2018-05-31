// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --wasm-async-compilation --no-wait-for-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function CompileFunctionsTest() {
  // Create a big module.
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  for (i = 0; i < 100; i++) {
    builder.addFunction("sub" + i, kSig_i_i)
      .addBody([                // --
        kExprGetLocal, 0,       // --
        kExprI32Const, i % 61,  // --
        kExprI32Sub])           // --
      .exportFunc()
  }

  var buffer = builder.toBuffer();
  // Start the compilation but do not wait for the promise to resolve
  // with assertPromiseResult. This should not cause a crash.
  WebAssembly.compile(buffer).then(
    () => { print("success")},
    () => { print("failed"); });
})();
