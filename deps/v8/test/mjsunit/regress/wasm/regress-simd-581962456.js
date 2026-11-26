// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Exercise WebAssembly return_call with positive stack_param_delta where
// unaligned 16-byte s128 outgoing stack arguments overlap adjacent 8-byte i64
// caller spill slots across diamond and loop merges.
(function TestTailCallMultiSlotStackOverlap() {
  const kNumDomI64 = 12;
  const kNumDomS128 = 16;
  const kNumTailS128 = 16;
  // Use an odd count of i64 stack arguments so s128 stack arguments land at odd
  // 8-byte frame slot indices (spanning pairs [2k, 2k+1]) after
  // ConvertToTailCallerLocation.
  const kNumI64Params = kNumDomI64 + 3;
  const kTotalS128Params = kNumDomS128 + kNumTailS128;

  const calleeParams = [
    ...new Array(kNumI64Params).fill(kWasmI64),
    ...new Array(kTotalS128Params).fill(kWasmS128),
  ];

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  const noop = builder.addFunction('noop', kSig_v_v).addBody([
    ...new Array(2000).fill(kExprNop),
  ]);

  const calleeBody = [
    // Keep callee above the inlining budget so return_call emits a real tail
    // call instruction.
    ...new Array(2000).fill(kExprNop),
  ];
  for (let i = 0; i < kTotalS128Params; ++i) {
    calleeBody.push(
        ...wasmI32Const(1024 + i * 16),
        kExprLocalGet, ...wasmUnsignedLeb(kNumI64Params + i),
        kSimdPrefix, kExprS128StoreMem, 0, ...wasmUnsignedLeb(0));
  }
  calleeBody.push(kExprLocalGet, 0);
  for (let i = 1; i < kNumI64Params; ++i) {
    calleeBody.push(
        kExprLocalGet, ...wasmUnsignedLeb(i),
        ...wasmI64Const(i + 1), kExprI64Mul, kExprI64Add);
  }
  const callee =
      builder.addFunction('callee', makeSig(calleeParams, [kWasmI64]))
          .addBody(calleeBody);

  // Locals in caller(mode: i32, iters: i32):
  //   2 .. 2 + kNumDomI64 - 1: dominating i64 locals
  //   2 + kNumDomI64 .. + kNumDomS128 - 1: dominating s128 locals
  //   phiI64A, phiS128, phiI64B: mixed-width Phis whose spill slots pack an
  //   aligned s128 slot alongside adjacent 8-byte i64 slots.
  const firstDomI64 = 2;
  const firstDomS128 = firstDomI64 + kNumDomI64;
  const phiI64A = firstDomS128 + kNumDomS128;
  const phiS128 = phiI64A + 1;
  const phiI64B = phiS128 + 1;

  const v1 = [
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01,
  ];
  const v2 = [
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78,
  ];

  const body = [];
  for (let i = 0; i < kNumDomS128; ++i) {
    body.push(
        ...wasmI32Const(i * 16),
        kSimdPrefix, kExprS128LoadMem, 0, ...wasmUnsignedLeb(0),
        kExprLocalSet, ...wasmUnsignedLeb(firstDomS128 + i));
  }
  for (let i = 0; i < kNumDomI64; ++i) {
    body.push(
        ...wasmI32Const(i * 8),
        kExprI64LoadMem, 0, ...wasmUnsignedLeb(0),
        kExprLocalSet, ...wasmUnsignedLeb(firstDomI64 + i));
  }

  body.push(
      ...wasmI64Const(0x0102030405060708n),
      kExprLocalSet, ...wasmUnsignedLeb(phiI64A),
      ...wasmS128Const(v1),
      kExprLocalSet, ...wasmUnsignedLeb(phiS128),
      ...wasmI64Const(0x1112131415161718n),
      kExprLocalSet, ...wasmUnsignedLeb(phiI64B),
      kExprLoop, kWasmVoid,
        kExprLocalGet, 1,
        kExprIf, kWasmVoid,
          kExprCallFunction, noop.index,
          kExprLocalGet, ...wasmUnsignedLeb(phiI64A),
          ...wasmI64Const(3), kExprI64Add,
          kExprLocalSet, ...wasmUnsignedLeb(phiI64A),
          kExprLocalGet, ...wasmUnsignedLeb(phiI64B),
          ...wasmI64Const(5), kExprI64Add,
          kExprLocalSet, ...wasmUnsignedLeb(phiI64B),
          kExprLocalGet, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprLocalTee, 1,
          kExprBrIf, 1,
        kExprEnd,
      kExprEnd,
      kExprLocalGet, 0,
      kExprIf, kWasmVoid,
        ...wasmS128Const(v1),
        kExprLocalSet, ...wasmUnsignedLeb(phiS128),
        ...wasmI64Const(0x1111222233334444n),
        kExprLocalGet, ...wasmUnsignedLeb(phiI64A),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(phiI64A),
        ...wasmI64Const(0x5555666677778888n),
        kExprLocalGet, ...wasmUnsignedLeb(phiI64B),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(phiI64B),
      kExprElse,
        ...wasmS128Const(v2),
        kExprLocalSet, ...wasmUnsignedLeb(phiS128),
        ...wasmI64Const(0x2222333344445555n),
        kExprLocalGet, ...wasmUnsignedLeb(phiI64A),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(phiI64A),
        ...wasmI64Const(0x6666777788889999n),
        kExprLocalGet, ...wasmUnsignedLeb(phiI64B),
        kExprI64Add,
        kExprLocalSet, ...wasmUnsignedLeb(phiI64B),
      kExprEnd);

  for (let i = 0; i < kNumDomI64; ++i) {
    body.push(kExprLocalGet, ...wasmUnsignedLeb(firstDomI64 + i));
  }
  body.push(
      ...wasmI64Const(0x777788889999aaaan),
      kExprLocalGet, ...wasmUnsignedLeb(phiI64A),
      kExprLocalGet, ...wasmUnsignedLeb(phiI64B));
  for (let i = 0; i < kNumDomS128; ++i) {
    body.push(kExprLocalGet, ...wasmUnsignedLeb(firstDomS128 + i));
  }
  for (let i = 0; i < kNumTailS128; ++i) {
    body.push(kExprLocalGet, ...wasmUnsignedLeb(phiS128));
  }
  body.push(kExprReturnCall, callee.index);

  builder.addFunction('caller', makeSig([kWasmI32, kWasmI32], [kWasmI64]))
      .addLocals(kWasmI64, kNumDomI64)
      .addLocals(kWasmS128, kNumDomS128)
      .addLocals(kWasmI64, 1)
      .addLocals(kWasmS128, 1)
      .addLocals(kWasmI64, 1)
      .addBody(body)
      .exportFunc();

  const instance = builder.instantiate();
  const u8 = new Uint8Array(instance.exports.memory.buffer);
  for (let i = 0; i < 512; ++i) {
    u8[i] = (i * 37 + 13) & 0xff;
  }

  const outLen = kTotalS128Params * 16;
  const ref0 = instance.exports.caller(0, 2);
  const mem0 = u8.slice(1024, 1024 + outLen);
  const ref1 = instance.exports.caller(1, 2);
  const mem1 = u8.slice(1024, 1024 + outLen);

  %WasmTierUpFunction(instance.exports.caller);

  assertEquals(ref0, instance.exports.caller(0, 2));
  assertEquals(mem0, u8.slice(1024, 1024 + outLen));
  assertEquals(ref1, instance.exports.caller(1, 2));
  assertEquals(mem1, u8.slice(1024, 1024 + outLen));
})();

(function TestReturnCallMixedSimd128AndI64StackParams() {
  // Tail-call from a function with spilled i64/s128 values to a callee with an
  // odd number of i64 stack parameters followed by many s128 stack parameters,
  // so 16-byte outgoing stack parameters span odd/even pairs of 8-byte slots.
  const kNumI64 = 11;
  const kNumS128 = 20;
  const calleeParams = [
    ...new Array(kNumI64).fill(kWasmI64),
    ...new Array(kNumS128).fill(kWasmS128),
  ];

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  const calleeBody = [...new Array(2000).fill(kExprNop)];
  for (let i = 0; i < kNumS128; ++i) {
    calleeBody.push(
        ...wasmI32Const(512 + i * 16),
        kExprLocalGet, ...wasmUnsignedLeb(kNumI64 + i),
        kSimdPrefix, kExprS128StoreMem, 0, ...wasmUnsignedLeb(0));
  }
  calleeBody.push(kExprLocalGet, 0);
  for (let i = 1; i < kNumI64; ++i) {
    calleeBody.push(kExprLocalGet, ...wasmUnsignedLeb(i), kExprI64Add);
  }
  const callee =
      builder.addFunction('callee', makeSig(calleeParams, [kWasmI64]))
          .addBody(calleeBody);

  const vA = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
  const vB = [16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1];
  const callerBody = [
    kExprLocalGet, 0,
    kExprIf, kWasmVoid,
      ...wasmI64Const(0x100000001n), kExprLocalSet, 1,
      ...wasmS128Const(vA), kExprLocalSet, 2,
      ...wasmI64Const(0x200000002n), kExprLocalSet, 3,
    kExprElse,
      ...wasmI64Const(0x300000003n), kExprLocalSet, 1,
      ...wasmS128Const(vB), kExprLocalSet, 2,
      ...wasmI64Const(0x400000004n), kExprLocalSet, 3,
    kExprEnd,
  ];
  for (let i = 0; i < kNumI64 - 2; ++i) {
    callerBody.push(...wasmI64Const(10 + i));
  }
  callerBody.push(kExprLocalGet, 1, kExprLocalGet, 3);
  for (let i = 0; i < kNumS128; ++i) {
    callerBody.push(kExprLocalGet, 2);
  }
  callerBody.push(kExprReturnCall, callee.index);

  builder.addFunction('main', makeSig([kWasmI32], [kWasmI64]))
      .addLocals(kWasmI64, 1)
      .addLocals(kWasmS128, 1)
      .addLocals(kWasmI64, 1)
      .addBody(callerBody)
      .exportFunc();

  const instance = builder.instantiate();
  const u8 = new Uint8Array(instance.exports.memory.buffer);
  let sumA = 0x100000001n + 0x200000002n;
  let sumB = 0x300000003n + 0x400000004n;
  for (let i = 0; i < kNumI64 - 2; ++i) {
    sumA += BigInt(10 + i);
    sumB += BigInt(10 + i);
  }

  assertEquals(sumA, instance.exports.main(1));
  for (let i = 0; i < kNumS128; ++i) {
    assertEquals(vA, Array.from(u8.slice(512 + i * 16, 512 + (i + 1) * 16)));
  }
  %WasmTierUpFunction(instance.exports.main);
  assertEquals(sumB, instance.exports.main(0));
  for (let i = 0; i < kNumS128; ++i) {
    assertEquals(vB, Array.from(u8.slice(512 + i * 16, 512 + (i + 1) * 16)));
  }
})();
