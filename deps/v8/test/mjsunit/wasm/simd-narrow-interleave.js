// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

// Test that low-half widening followed by two whole-lane shuffles and matching
// narrowing can be replaced with a single shuffle of the original inputs,
// if the shuffle width is equal to vector lanes' widths after widening.

// Shuffles for the low and high halves of the narrowed result that concatenate
// to give the expected mask, for the cases of 16 and 32-bit shuffles.
const halfwordShuffles = [{
  name: 'Zip',
  low: [0, 8, 1, 9, 2, 10, 3, 11],
  high: [4, 12, 5, 13, 6, 14, 7, 15],
}, {
  name: 'Unzip',
  low: [0, 2, 4, 6, 8, 10, 12, 14],
  high: [1, 3, 5, 7, 9, 11, 13, 15],
}, {
  name: 'Transpose',
  low: [0, 8, 2, 10, 4, 12, 6, 14],
  high: [1, 9, 3, 11, 5, 13, 7, 15],
}, {
  name: 'Extract',
  low: [1, 2, 3, 4, 5, 6, 7, 8],
  high: [7, 8, 9, 10, 11, 12, 13, 14],
}, {
  name: 'PermutationWithDuplicates',
  low: [15, 0, 9, 9, 6, 2, 12, 4],
  high: [8, 7, 1, 14, 3, 11, 5, 10],
}];

const wordShuffles = [{
  name: 'Zip',
  low: [0, 4, 1, 5],
  high: [2, 6, 3, 7],
}, {
  name: 'Unzip',
  low: [0, 2, 4, 6],
  high: [1, 3, 5, 7],
}, {
  name: 'Transpose',
  low: [0, 4, 2, 6],
  high: [1, 5, 3, 7],
}, {
  name: 'Extract',
  low: [1, 2, 3, 4],
  high: [3, 4, 5, 6],
}, {
  name: 'PermutationWithDuplicates',
  low: [7, 0, 5, 5],
  high: [2, 4, 1, 6],
}];

function ShuffleBytes(lanes, laneBytes) {
  // Expand each whole source lane into consecutive byte indices. For example,
  // halfword lanes [0, 8] become bytes [0, 1, 16, 17].
  return lanes.flatMap(lane =>
      Array.from({length: laneBytes}, (_, byte) => lane * laneBytes + byte));
}

// Creates the values and intermediates
function ExpectedNarrowShuffle(lhs, rhs, config) {
  const laneBytes = config.arrayType.BYTES_PER_ELEMENT;
  const wideBytes = 2 * laneBytes;
  const widened = new DataView(new ArrayBuffer(32));
  for (const [input, values] of [lhs, rhs].entries()) {
    const start = input === 1 && config.highRight ? 8 / laneBytes : 0;
    for (let lane = 0; lane < 8 / laneBytes; ++lane) {
      const offset = 16 * input + wideBytes * lane;
      if (wideBytes === 2) {
        widened.setInt16(offset, values[start + lane], true);
      } else {
        widened.setInt32(offset, values[start + lane], true);
      }
    }
  }
  const bits = 8 * laneBytes;
  const min = config.signed ? -(2 ** (bits - 1)) : 0;
  const max = config.signed ? 2 ** (bits - 1) - 1 : 2 ** bits - 1;
  const expected = [];
  const intermediates = Array.from(new Uint8Array(widened.buffer));
  for (const mask of [config.lowShuffle, config.highShuffle]) {
    const shuffled = new DataView(new ArrayBuffer(16));
    for (let byte = 0; byte < 16; ++byte) {
      shuffled.setUint8(byte, widened.getUint8(mask[byte]));
    }
    intermediates.push(...new Uint8Array(shuffled.buffer));
    for (let byte = 0; byte < 16; byte += wideBytes) {
      // Both signed and unsigned Wasm narrowing interpret the input as signed.
      const value = wideBytes === 2 ? shuffled.getInt16(byte, true) :
                                      shuffled.getInt32(byte, true);
      expected.push(Math.min(max, Math.max(min, value)));
    }
  }
  return {result: expected, intermediates};
}

// Take two input vector lanes, widen their lower halves, narrow with the same
// signedness, then check if this equal the expected shuffling.
function NarrowShuffleTest(config) {
  print(config.name);
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  const simd = [
    kExprLocalGet, 0, // Push lhs/rhs inputs and widen
    ...SimdInstr(kExprS128LoadMem), 0, 0,
    ...SimdInstr(config.extend),
    kExprLocalSet, 3,
    kExprLocalGet, 1,
    ...SimdInstr(kExprS128LoadMem), 0, 0,
    ...SimdInstr(config.highRight ? config.extendHigh : config.extend),
    kExprLocalSet, 4,
    kExprLocalGet, 2, // Get output address
    kExprLocalGet, 3, kExprLocalGet, 4, // Do low/high shuffles
    ...SimdInstr(kExprI8x16Shuffle), ...config.lowShuffle,
    kExprLocalTee, 5,
    kExprLocalGet, 3, kExprLocalGet, 4,
    ...SimdInstr(kExprI8x16Shuffle), ...config.highShuffle,
    kExprLocalTee, 6,
    ...SimdInstr(config.narrow), // Narrow the result and store
    ...SimdInstr(kExprS128StoreMem), 0, 0,
  ];

  // For the sharedInputs case, store widens and shuffles after narrowing.
  if (config.sharedInputs) {
    for (let i = 0; i < 4; ++i) {
      simd.push(
          kExprLocalGet, 2,
          kExprLocalGet, 3 + i,
          ...SimdInstr(kExprS128StoreMem), 0, 16 * (i + 1));
    }
  }

  builder.addFunction('simd', kSig_v_iii)
      .addLocals(kWasmS128, 4)
      .addBody(simd)
      .exportFunc();

  const wasm = builder.instantiate().exports;
  const laneBytes = config.arrayType.BYTES_PER_ELEMENT;
  const numLanes = 16 / laneBytes;
  const lhs = new config.arrayType(numLanes);
  const rhs = new config.arrayType(numLanes);
  const result = new config.arrayType(numLanes);
  const memory = new DataView(wasm.memory.buffer);
  const values = config.values;

  for (let i = 0; i < values.length; ++i) {
    for (let lane = 0; lane < numLanes; ++lane) {
      // Right lane reads numLanes ahead of the left to catch bad shuffles.
      lhs[lane] = values[(i + lane) % values.length];
      rhs[lane] = values[(i + numLanes + lane) % values.length];
      if (laneBytes === 1) {
        memory.setInt8(lane, lhs[lane]);
        memory.setInt8(16 + lane, rhs[lane]);
      } else {
        memory.setInt16(2 * lane, lhs[lane], true);
        memory.setInt16(16 + 2 * lane, rhs[lane], true);
      }
    }
    const expected = ExpectedNarrowShuffle(lhs, rhs, config);
    // Call with offsets for the lhs, rhs and result.
    wasm.simd(0, 16, 32);
    for (let lane = 0; lane < numLanes; ++lane) {
      result[lane] = laneBytes === 1 ? memory.getInt8(32 + lane) :
                                      memory.getInt16(32 + 2 * lane, true);
    }
    assertEquals(expected.result, Array.from(result), `${config.name}: ${i}`);
    if (config.sharedInputs) {
      assertEquals(
          expected.intermediates,
          // Reads widened left/right inputs and both shuffle results.
          Array.from(new Uint8Array(wasm.memory.buffer, 48, 64)),
          `${config.name}: intermediates ${i}`);
    }
  }
}

const configs = [{
  name: 'Signed8To16To8',
  extend: kExprI16x8SConvertI8x16Low,
  extendHigh: kExprI16x8SConvertI8x16High,
  narrow: kExprI8x16SConvertI16x8,
  signed: true,
  arrayType: Int8Array,
  values: int8_array,
}, {
  name: 'Unsigned8To16To8',
  extend: kExprI16x8UConvertI8x16Low,
  extendHigh: kExprI16x8UConvertI8x16High,
  narrow: kExprI8x16UConvertI16x8,
  signed: false,
  arrayType: Uint8Array,
  values: int8_array,
}, {
  name: 'Signed16To32To16',
  extend: kExprI32x4SConvertI16x8Low,
  extendHigh: kExprI32x4SConvertI16x8High,
  narrow: kExprI16x8SConvertI32x4,
  signed: true,
  arrayType: Int16Array,
  values: int16_array,
}, {
  name: 'Unsigned16To32To16',
  extend: kExprI32x4UConvertI16x8Low,
  extendHigh: kExprI32x4UConvertI16x8High,
  narrow: kExprI16x8UConvertI32x4,
  signed: false,
  arrayType: Uint16Array,
  values: int16_array,
}];

(function NarrowShuffleTestAll() {
  for (const config of configs) {
    const laneBytes = config.arrayType.BYTES_PER_ELEMENT;
    const wideBytes = 2 * laneBytes;
    const shuffles = laneBytes === 1 ? halfwordShuffles : wordShuffles;
    for (const shuffle of shuffles) {
      const base = {
        ...config,
        name: `${config.name}/${shuffle.name}`,
        lowShuffle: ShuffleBytes(shuffle.low, wideBytes),
        highShuffle: ShuffleBytes(shuffle.high, wideBytes),
      };
      function TestCase(name, overrides) {
        NarrowShuffleTest({ ...base, ...overrides, name: `${base.name}/${name}` });
      }

      NarrowShuffleTest(base);
      TestCase('SharedInputs', {sharedInputs: true});
      TestCase('HighHalfRightInput', {highRight: true});

      // Break each shuffle independently: both masks must preserve whole lanes.
      for (const field of ['lowShuffle', 'highShuffle']) {
        const mask = base[field];
        const dataInUpperHalf = mask.slice();
        dataInUpperHalf[wideBytes - 1] = mask[0];
        TestCase(`${field}/DataInUpperHalf`, {[field]: dataInUpperHalf});

        const extensionInLowerHalf = mask.slice();
        extensionInLowerHalf[0] = mask[laneBytes];
        TestCase(`${field}/ExtensionInLowerHalf`,
                 {[field]: extensionInLowerHalf});

        const otherExtension = mask.slice();
        // Replace the first vector lane's upper half with another upper half.
        // Reject since shuffling is done at a finer level than vector masks.
        otherExtension[laneBytes] = (mask[0] + wideBytes + laneBytes) % 32;
        TestCase(`${field}/OtherExtension`, {[field]: otherExtension});
      }
    }
  }
})();
