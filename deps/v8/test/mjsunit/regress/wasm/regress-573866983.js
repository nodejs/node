// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-shared

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestStructAtomicGetSignedZeroExtension() {
  for (let is_shared of [true, false]) {
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1);
    const struct = builder.addStruct({
      fields: [makeField(kWasmI8, true), makeField(kWasmI16, true)],
      is_shared,
    });
    const structRef = wasmRefNullType(struct);

    builder
      .addFunction('makeStruct', makeSig([kWasmI32, kWasmI32], [structRef]))
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kGCPrefix, kExprStructNew, struct,
      ])
      .exportFunc();

    builder.addFunction('extendU8', makeSig([structRef], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 0,
        kExprI64UConvertI32,
      ])
      .exportFunc();

    builder.addFunction('extendU16', makeSig([structRef], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 1,
        kExprI64UConvertI32,
      ])
      .exportFunc();

    // Using the i32 result as a 32-bit memory index emits a ChangeUint32ToUint64
    // in Turboshaft. On arm64, instruction selection elides this zero-extension
    // because ZeroExtendsWord32ToWord64 returns true for the atomic load. If the
    // atomic load sign-extends a negative value (-1) to 64 bits instead of
    // 32 bits, the load accesses mem_base - 1 instead of trapping out-of-bounds.
    builder.addFunction('memLoadIdx8', makeSig([structRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 0,
        kExprI32LoadMem, 0, 0,
      ])
      .exportFunc();

    builder.addFunction('memLoadIdx16', makeSig([structRef], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kAtomicPrefix, kExprStructAtomicGetS, kAtomicSeqCst, struct, 1,
        kExprI32LoadMem, 0, 0,
      ])
      .exportFunc();

    const wasm = builder.instantiate().exports;

    const sNeg1 = wasm.makeStruct(-1, -1);
    assertEquals(0xffffffffn, wasm.extendU8(sNeg1));
    assertEquals(0xffffffffn, wasm.extendU16(sNeg1));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx8(sNeg1));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx16(sNeg1));

    const sMin = wasm.makeStruct(-128, -32768);
    assertEquals(0xffffff80n, wasm.extendU8(sMin));
    assertEquals(0xffff8000n, wasm.extendU16(sMin));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx8(sMin));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx16(sMin));
  }
})();

(function TestArrayAtomicGetSignedZeroExtension() {
  for (let is_shared of [true, false]) {
    const builder = new WasmModuleBuilder();
    builder.addMemory(1, 1);
    const array8 = builder.addArray(kWasmI8, {is_shared});
    const array16 = builder.addArray(kWasmI16, {is_shared});
    const array8Ref = wasmRefNullType(array8);
    const array16Ref = wasmRefNullType(array16);

    builder.addFunction('makeArray8', makeSig([kWasmI32], [array8Ref]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprArrayNewFixed, array8, 1,
      ])
      .exportFunc();

    builder.addFunction('makeArray16', makeSig([kWasmI32], [array16Ref]))
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprArrayNewFixed, array16, 1,
      ])
      .exportFunc();

    builder.addFunction('extendU8', makeSig([array8Ref], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array8,
        kExprI64UConvertI32,
      ])
      .exportFunc();

    builder.addFunction('extendU16', makeSig([array16Ref], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array16,
        kExprI64UConvertI32,
      ])
      .exportFunc();

    builder.addFunction('memLoadIdx8', makeSig([array8Ref], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array8,
        kExprI32LoadMem, 0, 0,
      ])
      .exportFunc();

    builder.addFunction('memLoadIdx16', makeSig([array16Ref], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kAtomicPrefix, kExprArrayAtomicGetS, kAtomicSeqCst, array16,
        kExprI32LoadMem, 0, 0,
      ])
      .exportFunc();

    const wasm = builder.instantiate().exports;

    const a8Neg1 = wasm.makeArray8(-1);
    const a16Neg1 = wasm.makeArray16(-1);
    assertEquals(0xffffffffn, wasm.extendU8(a8Neg1));
    assertEquals(0xffffffffn, wasm.extendU16(a16Neg1));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx8(a8Neg1));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx16(a16Neg1));

    const a8Min = wasm.makeArray8(-128);
    const a16Min = wasm.makeArray16(-32768);
    assertEquals(0xffffff80n, wasm.extendU8(a8Min));
    assertEquals(0xffff8000n, wasm.extendU16(a16Min));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx8(a8Min));
    assertTraps(kTrapMemOutOfBounds, () => wasm.memLoadIdx16(a16Min));
  }
})();
