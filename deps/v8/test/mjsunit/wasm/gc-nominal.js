// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function() {
  var builder = new WasmModuleBuilder();
  let struct1 = builder.addStructSubtype([makeField(kWasmI32, true)]);
  let struct2 = builder.addStructSubtype(
      [makeField(kWasmI32, true), makeField(kWasmI32, true)], struct1);

  let array1 = builder.addArraySubtype(kWasmI32, true);
  let array2 = builder.addArraySubtype(kWasmI32, true, array1);

  builder.addFunction("main", kSig_v_v)
      .addLocals(wasmOptRefType(struct1), 1)
      .addLocals(wasmOptRefType(array1), 1)
      .addBody([
        // Check that we can create a struct with explicit RTT...
        kGCPrefix, kExprRttCanon, struct2, kGCPrefix,
        kExprStructNewDefaultWithRtt, struct2,
        // ...and upcast it.
        kExprLocalSet, 0,
        // Check that we can create a struct with implicit RTT.
        kGCPrefix, kExprStructNewDefault, struct2, kExprLocalSet, 0,
        // Check that we can create an array with explicit RTT...
        kExprI32Const, 10,  // length
        kGCPrefix, kExprRttCanon, array2, kGCPrefix,
        kExprArrayNewDefaultWithRtt, array2,
        // ...and upcast it.
        kExprLocalSet, 1,
        // Check that we can create an array with implicit RTT.
        kExprI32Const, 10,  // length
        kGCPrefix, kExprArrayNewDefault, array2, kExprLocalSet, 1
      ])
      .exportFunc();

  // This test is only interested in type checking.
  builder.instantiate();
})();

(function () {
    let builder = new WasmModuleBuilder();
    let t0 = builder.addStructSubtype([]);
    for (let i = 0; i < 32; i++) {
        builder.addStructSubtype([], i);
    }
    assertThrows(
        () => builder.instantiate(), WebAssembly.CompileError,
        /subtyping depth is greater than allowed/);
})();
