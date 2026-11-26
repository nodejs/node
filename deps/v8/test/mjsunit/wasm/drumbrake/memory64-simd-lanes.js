// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testMemory64SimdLaneOffset() {
  const offset2To48 = [0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40];
  const builder = new WasmModuleBuilder();
  builder.addMemory64(1, 1);
  const global = builder.addGlobal(
      kWasmS128, true, false,
      [kSimdPrefix, kExprS128Const, ...new Array(16).fill(0)]);
  builder.addFunction("main", kSig_v_v)
      .addBody([
        kExprI64Const, 0,
        kExprGlobalGet, global.index,
        kSimdPrefix, kExprS128Load32Lane, 0, ...offset2To48, 0,
        kExprGlobalSet, global.index,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  assertThrows(() => instance.exports.main(), WebAssembly.RuntimeError,
               /out of bounds/);
})();
