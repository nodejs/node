// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Tests SIMD load with 64-bit memory.
(function TestSimdLoadMem64() {
  const builder = new WasmModuleBuilder();
  builder.addMemory64(6);

  const SRC_OFFSET_LEB = [0xE0, 0x02];
  builder
    .addFunction("main", makeSig([], []))
    .addLocals(kWasmS128, 2)
    .addBodyWithEnd([
    ...wasmI64Const(0x100000000),
      kSimdPrefix, kExprS128Load16x4U, 0, ...SRC_OFFSET_LEB,
      kExprLocalSet, 0,
      kExprEnd,
    ]);
  builder.addExport('main', 0);
  const instance = builder.instantiate({});
  assertTraps(kTrapMemOutOfBounds, () => instance.exports.main());
})();
