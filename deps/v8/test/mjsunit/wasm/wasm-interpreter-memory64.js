// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-memory64

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testMultipleMemoriesOfDifferentKinds() {
  print(arguments.callee.name);

  var builder = new WasmModuleBuilder();
  const mem64_idx = 0;
  builder.addMemory64(1, 1, false);
  const mem32_idx = 1;
  builder.addMemory(1, 1, false);

  builder.addFunction("main", kSig_v_v)
    .addBody([
      ...wasmI32Const(42),
      kExprI32LoadMem, 0x40, mem32_idx, 0,
      kExprDrop
    ])
    .exportAs("main");
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
})();
