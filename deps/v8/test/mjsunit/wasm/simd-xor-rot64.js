// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function XorRotate64() {
  print(arguments.callee.name);

  for (let shl = 1; shl <= 63 ; shl += 8) {
    for (let shr = 63; shr >= 1; shr -= 8) {
      const builder = new WasmModuleBuilder();
      builder.addMemory(1, 1, true);
      builder.exportMemoryAs("memory");

      builder.addFunction("shr_shl", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
          .addLocals(kWasmS128, 2).addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprS128Xor,
        kExprLocalTee, 2,
        kExprI32Const, shr,
        ...SimdInstr(kExprI64x2ShrU),
        kExprLocalGet, 2,
        kExprI32Const, shl,
        ...SimdInstr(kExprI64x2Shl),
        kSimdPrefix, kExprS128Or,
        kExprLocalTee, 3,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 3,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ]).exportFunc();

      builder.addFunction("shl_shr", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
          .addLocals(kWasmS128, 2).addBody([
        kExprLocalGet, 0,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128LoadMem, 0, 0,
        kSimdPrefix, kExprS128Xor,
        kExprLocalTee, 2,
        kExprI32Const, shl,
        ...SimdInstr(kExprI64x2Shl),
        kExprLocalGet, 2,
        kExprI32Const, shr,
        ...SimdInstr(kExprI64x2ShrU),
        kSimdPrefix, kExprS128Or,
        kExprLocalTee, 3,
        kSimdPrefix, kExprI64x2ExtractLane, 0,
        kExprLocalGet, 3,
        kSimdPrefix, kExprI64x2ExtractLane, 1,
        kExprI64Sub,
      ]).exportFunc();

      builder.addFunction("scalar", makeSig([kWasmI32, kWasmI32], [kWasmI64]))
          .addLocals(kWasmI64, 2).addBody([
        kExprLocalGet, 0,
        kExprI64LoadMem, 0, 0,
        kExprLocalGet, 1,
        kExprI64LoadMem, 0, 0,
        kExprI64Xor,
        kExprLocalTee, 2,
        kExprI64Const, shl,
        kExprI64Shl,
        kExprLocalGet, 2,
        kExprI64Const, shr,
        kExprI64ShrU,
        kExprI64Ior,
        kExprLocalGet, 0,
        kExprI64LoadMem, 0, 8,
        kExprLocalGet, 1,
        kExprI64LoadMem, 0, 8,
        kExprI64Xor,
        kExprLocalTee, 3,
        kExprI64Const, shl,
        kExprI64Shl,
        kExprLocalGet, 3,
        kExprI64Const, shr,
        kExprI64ShrU,
        kExprI64Ior,
        kExprI64Sub,
      ]).exportFunc();

      const wasm = builder.instantiate().exports;
      for (let i = 0; i < 32; ++i) {
        wasm.memory[i] = int8_array[i % int8_array.length];
      }
      const simd_0 = wasm.shl_shr(0, 16)
      const simd_1 = wasm.shr_shl(0, 16);
      assertEquals(simd_0, simd_1);
      assertEquals(simd_1, wasm.scalar(0, 16));
    }
  }
})();
