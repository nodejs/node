// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-num-compilation-tasks=0

load('test/mjsunit/wasm/wasm-module-builder.js');

(function testSyncCompilation() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_d_d)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  const instance = builder.instantiate();
})();

(function testAsyncCompilation() {
  print(arguments.callee.name);
  const builder = new WasmModuleBuilder();
  builder.addFunction("main", kSig_i_i)
      .addBody([kExprLocalGet, 0])
      .exportFunc();

  const instance = builder.asyncInstantiate();
})();
