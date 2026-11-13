// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const kOutputOffset = 2048;
const kVectorCount = 48;
const kMaxIterations = 16;

function loadV128(localIndex, vecIndex) {
  return [
    kExprLocalGet, localIndex,
    kSimdPrefix, kExprS128LoadMem, 4, ...wasmUnsignedLeb(vecIndex * 16),
  ];
}

function dotAt(localIndex, a, b) {
  return [
    ...loadV128(localIndex, a),
    ...loadV128(localIndex, b),
    ...SimdInstr(kExprI32x4DotI16x8S),
  ];
}

function chainOfDots(accumulatorBytes, localIndex, pairs) {
  const bytes = [...accumulatorBytes];
  for (const [a, b] of pairs) {
    bytes.push(...dotAt(localIndex, a, b), ...SimdInstr(kExprI32x4Add));
  }
  return bytes;
}

const kManyPairs = [
  [1, 2], [3, 4], [5, 6], [7, 8], [9, 1],
  [2, 3], [4, 5], [6, 7], [8, 9], [1, 3],
];

function dotReference(i16, aIndex, bIndex) {
  const result = [0, 0, 0, 0];
  for (let lane = 0; lane < 4; ++lane) {
    const a = aIndex * 8 + lane * 2;
    const b = bIndex * 8 + lane * 2;
    result[lane] = (Math.imul(i16[a], i16[b]) +
                    Math.imul(i16[a + 1], i16[b + 1])) | 0;
  }
  return result;
}

function addReference(a, b) {
  return a.map((value, lane) => (value + b[lane]) | 0);
}

function vectorAt(i32, index) {
  return Array.from(i32.subarray(index * 4, index * 4 + 4));
}

function checkOutput(i32, expected) {
  for (let lane = 0; lane < 4; ++lane) {
    assertEquals(expected[lane], i32[kOutputOffset / 4 + lane]);
  }
}

function makePattern(kind) {
  const pattern = new Int16Array(kVectorCount * 8);
  let seed = kind + 1;
  for (let i = 0; i < pattern.length; ++i) {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    if (kind === 0) {
      pattern[i] = -32768;
    } else if (kind === 1 && i % 5 === 0) {
      pattern[i] = (i % 2) ? 32767 : -32768;
    } else {
      pattern[i] = seed >>> 16;
    }
  }
  return pattern;
}

(function TestSimdDotProductSum() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.exportMemoryAs('memory');

  // Loop-carried accumulator summing four dot products per iteration.
  builder.addFunction('loop', kSig_v_i)
      .addLocals(kWasmS128, 1)  // local 1: $acc
      .addLocals(kWasmI32, 2)   // local 2: $i, local 3: $p
      .addBody([
        ...wasmI32Const(0),
        kSimdPrefix, kExprS128LoadMem, 4, 0,
        kExprLocalSet, 1,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 2,
          ...wasmI32Const(4),
          kExprI32Shl,
          kExprLocalSet, 3,
          ...chainOfDots([kExprLocalGet, 1], 3, [[1, 2], [3, 4], [5, 6], [7, 8]]),
          kExprLocalSet, 1,
          kExprLocalGet, 2,
          ...wasmI32Const(1),
          kExprI32Add,
          kExprLocalTee, 2,
          kExprLocalGet, 0,
          kExprI32LtU,
          kExprBrIf, 0,
        kExprEnd,
        ...wasmI32Const(kOutputOffset),
        kExprLocalGet, 1,
        kSimdPrefix, kExprS128StoreMem, 4, 0,
      ])
      .exportFunc();

  // Sum of dot products mixed with signed/unsigned widening multiplies (with no
  // extra non-fused addends, so the final widening multiply defines the root).
  builder.addFunction('mixed', kSig_v_v)
      .addLocals(kWasmI32, 1)  // local 0: $p
      .addBody([
        ...wasmI32Const(kOutputOffset),
        ...chainOfDots(dotAt(0, 1, 2), 0, [[3, 4]]),
        ...loadV128(0, 5),
        ...loadV128(0, 6),
        ...SimdInstr(kExprI32x4ExtMulLowI16x8S),
        ...loadV128(0, 7),
        ...loadV128(0, 8),
        ...SimdInstr(kExprI32x4ExtMulHighI16x8S),
        ...SimdInstr(kExprI32x4Add),
        ...loadV128(0, 5),
        ...loadV128(0, 6),
        ...SimdInstr(kExprI32x4ExtMulLowI16x8U),
        ...loadV128(0, 7),
        ...loadV128(0, 8),
        ...SimdInstr(kExprI32x4ExtMulHighI16x8U),
        ...SimdInstr(kExprI32x4Add),
        ...SimdInstr(kExprI32x4Add),
        ...SimdInstr(kExprI32x4Add),
        kSimdPrefix, kExprS128StoreMem, 4, 0,
      ])
      .exportFunc();

  // Balanced tree with dot products on one side and plain adds on the other.
  builder.addFunction('balanced', kSig_v_v)
      .addLocals(kWasmI32, 1)  // local 0: $p
      .addBody([
        ...wasmI32Const(kOutputOffset),
        ...dotAt(0, 1, 2),
        ...dotAt(0, 3, 4),
        ...SimdInstr(kExprI32x4Add),
        ...loadV128(0, 5),
        ...loadV128(0, 6),
        ...SimdInstr(kExprI32x4Add),
        ...loadV128(0, 7),
        ...loadV128(0, 8),
        ...SimdInstr(kExprI32x4Add),
        ...SimdInstr(kExprI32x4Add),
        ...SimdInstr(kExprI32x4Add),
        kSimdPrefix, kExprS128StoreMem, 4, 0,
      ])
      .exportFunc();

  // Chain of 10 dot products (exceeding kMaxDotProducts = 6).
  builder.addFunction('many', kSig_v_v)
      .addLocals(kWasmI32, 1)  // local 0: $p
      .addBody([
        ...wasmI32Const(kOutputOffset),
        ...chainOfDots(
            [...wasmI32Const(0), kSimdPrefix, kExprS128LoadMem, 4, 0],
            0, kManyPairs),
        kSimdPrefix, kExprS128StoreMem, 4, 0,
      ])
      .exportFunc();

  // Shared dot product (used twice via local.tee) alongside two single-use dots.
  builder.addFunction('shared', kSig_v_v)
      .addLocals(kWasmI32, 1)   // local 0: $p
      .addLocals(kWasmS128, 1)  // local 1: $shared
      .addBody([
        ...wasmI32Const(kOutputOffset),
        ...dotAt(0, 1, 2),
        kExprLocalTee, 1,
        ...dotAt(0, 3, 4),
        ...SimdInstr(kExprI32x4Add),
        ...dotAt(0, 5, 6),
        ...SimdInstr(kExprI32x4Add),
        kExprLocalGet, 1,
        ...SimdInstr(kExprI32x4Add),
        kSimdPrefix, kExprS128StoreMem, 4, 0,
      ])
      .exportFunc();

  const instance = builder.instantiate();
  const { memory, loop, mixed, balanced, many, shared } = instance.exports;
  const i16 = new Int16Array(memory.buffer);
  const u16 = new Uint16Array(memory.buffer);
  const i32 = new Int32Array(memory.buffer);

  for (let kind = 0; kind < 3; ++kind) {
    i16.set(makePattern(kind));

    let acc = vectorAt(i32, 0);
    for (let n = 1; n <= kMaxIterations; ++n) {
      for (const [a, b] of [[1, 2], [3, 4], [5, 6], [7, 8]]) {
        acc = addReference(acc, dotReference(i16, (n - 1) + a, (n - 1) + b));
      }
      loop(n);
      checkOutput(i32, acc);
    }

    const sum2 = addReference(dotReference(i16, 1, 2), dotReference(i16, 3, 4));
    const lowS = [0, 0, 0, 0];
    const highS = [0, 0, 0, 0];
    const lowU = [0, 0, 0, 0];
    const highU = [0, 0, 0, 0];
    for (let lane = 0; lane < 4; ++lane) {
      lowS[lane] = Math.imul(i16[5 * 8 + lane], i16[6 * 8 + lane]);
      highS[lane] = Math.imul(i16[7 * 8 + 4 + lane], i16[8 * 8 + 4 + lane]);
      lowU[lane] = (u16[5 * 8 + lane] * u16[6 * 8 + lane]) | 0;
      highU[lane] = (u16[7 * 8 + 4 + lane] * u16[8 * 8 + 4 + lane]) | 0;
    }
    mixed();
    checkOutput(
        i32,
        addReference(
            sum2,
            addReference(addReference(lowS, highS), addReference(lowU, highU))));

    const dots = addReference(dotReference(i16, 1, 2), dotReference(i16, 3, 4));
    const others = addReference(
        addReference(vectorAt(i32, 5), vectorAt(i32, 6)),
        addReference(vectorAt(i32, 7), vectorAt(i32, 8)));
    balanced();
    checkOutput(i32, addReference(dots, others));

    let manyExpected = vectorAt(i32, 0);
    for (const [a, b] of kManyPairs) {
      manyExpected = addReference(manyExpected, dotReference(i16, a, b));
    }
    many();
    checkOutput(i32, manyExpected);

    const sharedDot = dotReference(i16, 1, 2);
    const sharedExpected = addReference(
        addReference(addReference(sharedDot, dotReference(i16, 3, 4)),
                     dotReference(i16, 5, 6)),
        sharedDot);
    shared();
    checkOutput(i32, sharedExpected);
  }
})();
