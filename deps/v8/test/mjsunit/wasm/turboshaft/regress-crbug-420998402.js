// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestInlinedMemcpyUnreachableInCFG() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  const callee = builder.addFunction("callee", kSig_v_v).addBody([
    kExprUnreachable,
  ]);
  builder.addFunction("main", makeSig([kWasmI32], []))
    .addBody([
        kExprCallFunction, callee.index,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kNumericPrefix, kExprMemoryCopy, 0, 0,
    ])
    .exportFunc();

  const wasm = builder.instantiate().exports;
  assertTraps(kTrapUnreachable, () => wasm.main(0));
})();
