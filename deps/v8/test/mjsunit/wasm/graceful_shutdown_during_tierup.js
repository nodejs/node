// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-wait-for-background-tasks --wasm-tier-up

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function ShutdownDuringTierUp() {
  // Create a big module.
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1);
  for (i = 0; i < 100; i++) {
    builder.addFunction("sub" + i, kSig_i_i)
      .addBody([                // --
        kExprLocalGet, 0,       // --
        kExprI32Const, i % 61,  // --
        kExprI32Sub])           // --
      .exportFunc()
  }

  var buffer = builder.toBuffer();
  // Wait for compilation to finish, but then shutdown while tier-up is still
  // running.
  assertPromiseResult(WebAssembly.compile(buffer));
})();
