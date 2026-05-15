// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function Rotate64NoXor() {
  print(arguments.callee.name);

  const shift_pairs = [
    [8, 56],
    [16, 48],
    [24, 40],
  ];

  for (const [shl, shr] of shift_pairs) {
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, true);
    builder.exportMemoryAs('memory');

    builder.addFunction('shl_shr', makeSig([kWasmI32], [kWasmI64]))
        .addLocals(kWasmS128, 2)
        .addBody([
          kExprLocalGet, 0,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          kExprLocalTee, 1,
          kExprI32Const, shl,
          ...SimdInstr(kExprI64x2Shl),
          kExprLocalGet, 1,
          kExprI32Const, shr,
          ...SimdInstr(kExprI64x2ShrU),
          kSimdPrefix, kExprS128Or,
          kExprLocalTee, 2,
          kSimdPrefix, kExprI64x2ExtractLane, 0,
          kExprLocalGet, 2,
          kSimdPrefix, kExprI64x2ExtractLane, 1,
          kExprI64Sub,
        ]).exportFunc();

    builder.addFunction('shr_shl', makeSig([kWasmI32], [kWasmI64]))
        .addLocals(kWasmS128, 2)
        .addBody([
          kExprLocalGet, 0,
          kSimdPrefix, kExprS128LoadMem, 0, 0,
          kExprLocalTee, 1,
          kExprI32Const, shr,
          ...SimdInstr(kExprI64x2ShrU),
          kExprLocalGet, 1,
          kExprI32Const, shl,
          ...SimdInstr(kExprI64x2Shl),
          kSimdPrefix, kExprS128Or,
          kExprLocalTee, 2,
          kSimdPrefix, kExprI64x2ExtractLane, 0,
          kExprLocalGet, 2,
          kSimdPrefix, kExprI64x2ExtractLane, 1,
          kExprI64Sub,
        ]).exportFunc();

    builder.addFunction('scalar', makeSig([kWasmI32], [kWasmI64]))
        .addLocals(kWasmI64, 2)
        .addBody([
          kExprLocalGet, 0,
          kExprI64LoadMem, 0, 0,
          kExprLocalTee, 1,
          kExprI64Const, shl,
          kExprI64Shl,
          kExprLocalGet, 1,
          kExprI64Const, shr,
          kExprI64ShrU,
          kExprI64Ior,

          kExprLocalGet, 0,
          kExprI64LoadMem, 0, 8,
          kExprLocalTee, 2,
          kExprI64Const, shl,
          kExprI64Shl,
          kExprLocalGet, 2,
          kExprI64Const, shr,
          kExprI64ShrU,
          kExprI64Ior,

          kExprI64Sub,
        ]).exportFunc();

    const wasm = builder.instantiate().exports;
    const mem8 = new Uint8Array(wasm.memory.buffer);
    for (let i = 0; i < 32; ++i) {
      mem8[i] = int8_array[i % int8_array.length];
    }

    const scalar = wasm.scalar(0);
    assertEquals(scalar, wasm.shl_shr(0));
    assertEquals(scalar, wasm.shr_shl(0));
  }
})();
