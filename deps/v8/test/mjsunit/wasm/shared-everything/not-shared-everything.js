// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestDisallowedSharedTypes() {
  print(arguments.callee.name);

  // TODO(mliedtke): Add other disallowed types
  for (const [i, type] of [
      kWasmFuncRef, kWasmNullFuncRef,
      kWasmExnRef, kWasmNullExnRef,
      kWasmContRef, kWasmNullContRef,
  ].entries()) {
    print(`- test type ${i}: ${type}`);
    let builder = new WasmModuleBuilder();
    builder.addFunction("consumer", kSig_v_v)
    .addLocals(wasmRefNullType(type).shared(), 1)
    .addBody([])
    .exportFunc();
    assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
  }
})();
