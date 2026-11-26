// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testExceptionInCatchDoesNotReenterItself() {
  const builder = new WasmModuleBuilder();
  const tagA = builder.addTag(kSig_v_v);
  const tagB = builder.addTag(kSig_v_v);
  const global = builder.addGlobal(kWasmI32, true, false);
  builder.addFunction("main", kSig_i_v)
      .addBody([
        kExprTry, kWasmI32,
          kExprThrow, tagA,
        kExprCatch, tagA,
          kExprGlobalGet, global.index,
          kExprI32Eqz,
          kExprIf, kWasmVoid,
            kExprI32Const, 1,
            kExprGlobalSet, global.index,
            kExprTry, kWasmVoid,
              kExprThrow, tagA,
            kExprCatch, tagB,
            kExprEnd,
          kExprEnd,
          kExprI32Const, 5,
        kExprEnd,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  assertThrows(() => instance.exports.main(), WebAssembly.Exception);
})();
