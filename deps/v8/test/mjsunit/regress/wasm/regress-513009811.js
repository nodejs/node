// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --future

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestAssertNotNullVsTrustedLoad() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let type = builder.addType(kSig_i_i);

  let inc = builder.addFunction("inc", type)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add])
    .exportFunc();

  builder.addFunction(
      "main", makeSig([wasmRefNullType(type), kWasmI32], [kWasmI32]))
    .addBody([kExprLocalGet, 1,
              kExprLocalGet, 0, kExprRefAsNonNull, kExprCallRef, type])
    .exportFunc();

  let wasm = builder.instantiate().exports;

  assertEquals(11, wasm.main(wasm.inc, 10));
})();
