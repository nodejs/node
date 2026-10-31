// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wasmfx --cpu-profiler-sampling-interval=10

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestStackSwitchingProfiler() {
  let builder = new WasmModuleBuilder();
  let sig_v_v = builder.addType(kSig_v_v);
  let cont_index = builder.addCont(sig_v_v);
  let nop = builder.addFunction("nop", sig_v_v).addBody([]).exportFunc();
  let kCount = 10;
  let table = builder.addTable(wasmRefNullType(cont_index), kCount, kCount);
  builder.addFunction("fill", kSig_v_v)
      .addLocals(kWasmI32, 1)
      .addBody([
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kExprRefFunc, nop.index,
          kExprContNew, cont_index,
          kExprTableSet, table.index,
          kExprLocalGet, 0,
          kExprI32Const, 1,
          kExprI32Add,
          kExprLocalTee, 0,
          kExprI32Const, ...wasmSignedLeb(kCount),
          kExprI32LtU,
          kExprBrIf, 0,
        kExprEnd,
      ]).exportFunc();
  builder.addFunction("run", kSig_v_v)
      .addLocals(kWasmI32, 1)
      .addBody([
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kExprTableGet, table.index,
          kExprResume, cont_index, 0,
          kExprLocalGet, 0,
          kExprI32Const, 1,
          kExprI32Add,
          kExprLocalTee, 0,
          kExprI32Const, ...wasmSignedLeb(kCount),
          kExprI32LtU,
          kExprBrIf, 0,
        kExprEnd,
      ]).exportFunc();
  const instance = builder.instantiate();
  console.profile();
  for (let i = 0; i < 100; ++i) {
    instance.exports.fill();
    instance.exports.run();
  }
})();
