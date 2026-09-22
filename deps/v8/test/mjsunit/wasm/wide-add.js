// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wide-arithmetic

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Tests pattern matching and carry behavior for chained 128-bit additions.
// Verifies both left- and right-associative reduction patterns and all four
// carry combinations (0, 1 from stage 1, 1 from stage 2, and 2 from both).
function testAdd3Pattern() {
  let builder = new WasmModuleBuilder();

  // Pattern 1 (Left-associated): ((a + b) + c)
  // Computes add128(add128(a, 0, b, 0), c, 0).
  // Matches MachineOptimizationReducer:
  //   add128(add128(x, 0, y, 0), z, 0) -> Word64Add3(x, y, z)
  builder.addFunction("add3_pattern", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprLocalGet, 1, // b
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128, // (a + b) -> (low, carry1)
      kExprLocalGet, 2, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128, // ((a + b) + c) -> (sum, carry_out)
    ]);

  // Pattern 2 (Right-associated): (a + (b + c))
  // Computes add128(a, 0, add128(b, 0, c, 0)).
  builder.addFunction("add3_pattern_alt", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprLocalGet, 1, // b
      kExprI64Const, 0,
      kExprLocalGet, 2, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128, // (b + c) -> (low, carry1)
      kNumericPrefix, kExprI64Add128, // (a + (b + c)) -> (sum, carry_out)
    ]);

  let instance = builder.instantiate();
  let add3 = instance.exports.add3_pattern;
  let add3_alt = instance.exports.add3_pattern_alt;

  const max = 0xffffffffffffffffn;

  for (let f of [add3, add3_alt]) {
    // 1. No carry generated in either addition stage:
    assertEquals([6n, 0n], f(1n, 2n, 3n));

    // 2. Carry generated in the first addition stage (a + b), second does not carry:
    assertEquals([4n, 1n], f(max, 2n, 3n));

    // 3. Carry generated in the second addition stage, first stage does not carry:
    assertEquals([1n, 1n], f(max - 1n, 1n, 2n));

    // 4. Both addition stages generate a carry (2-bit carry-out):
    assertEquals([0n, 2n], f(max, 2n, max));
  }
}

// Tests pattern matching and code generation when one of the three operands
// is an immediate / constant value.
function testAdd3Constants() {
  let builder = new WasmModuleBuilder();
  // Tests different variants of const at different positions
  //  in the operation add128(add128(a, 0, 1, 0), c, 0).

  // a is constant 1
  builder.addFunction("add3_const_a", makeSig([kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprI64Const, 1,
      kExprI64Const, 0,
      kExprLocalGet, 0, // b
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprLocalGet, 1, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
    ]);

  // b is constant 1
  builder.addFunction("add3_const_b", makeSig([kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprI64Const, 1,
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprLocalGet, 1, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
    ]);

  // c is constant 1
  builder.addFunction("add3_const_c", makeSig([kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprLocalGet, 1, // b
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprI64Const, 1,
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
    ]);

  let instance = builder.instantiate();

  assertEquals([6n, 0n], instance.exports.add3_const_a(2n, 3n));
  assertEquals([4n, 1n], instance.exports.add3_const_a(0xffffffffffffffffn, 4n));

  assertEquals([6n, 0n], instance.exports.add3_const_b(2n, 3n));
  assertEquals([4n, 1n], instance.exports.add3_const_b(0xffffffffffffffffn, 4n));

  assertEquals([6n, 0n], instance.exports.add3_const_c(2n, 3n));
  assertEquals([0n, 1n], instance.exports.add3_const_c(0xffffffffffffffffn, 0n));
}

// Multi-limb BigInt addition loop variants:
// These functions implement a full multi-precision addition loop (`res = a + b + carry`),
// representing real-world cryptographic / bignum operations.

// Variant 1: (a[i] + b[i]) + carry
function addLoopVariant1(builder) {
  builder.addFunction("loop_v1", makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI64]))
    .exportFunc()
    .addLocals(kWasmI32, 2) // i (local 4), limit (local 5)
    .addLocals(kWasmI64, 1) // carry (local 6)
    .addBody([
      kExprLocalGet, 3, // count
      kExprI32Const, 3,
      kExprI32Shl, // byte limit = count * 8
      kExprLocalSet, 5,
      kExprI32Const, 0,
      kExprLocalSet, 4, // i = 0
      kExprI64Const, 0,
      kExprLocalSet, 6, // carry = 0

      kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
      kExprLocalGet, 4, // i
      kExprLocalGet, 5, // limit
      kExprI32GeS,
      kExprBrIf, 1, // exit loop when i >= limit

      // Compute destination address: res_ptr + i
      kExprLocalGet, 2,
      kExprLocalGet, 4,
      kExprI32Add,

      // Load a[i]
      kExprLocalGet, 0,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // Load b[i]
      kExprLocalGet, 1,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // (a[i] + b[i]) -> (temp_sum, temp_carry)
      kNumericPrefix, kExprI64Add128,

      // Get previous carry
      kExprLocalGet, 6,
      kExprI64Const, 0,

      // ((a[i] + b[i]) + carry) -> (sum[i], next_carry)
      kNumericPrefix, kExprI64Add128,

      kExprLocalSet, 6, // next_carry
      kExprI64StoreMem, 3, 0, // store sum[i] to res[i]

      // i += 8
      kExprLocalGet, 4,
      kExprI32Const, 8,
      kExprI32Add,
      kExprLocalSet, 4,
      kExprBr, 0,
      kExprEnd,
      kExprEnd,
      kExprLocalGet, 6, // return final carry
    ]);
}

// Variant 2: (a[i] + carry) + b[i]
function addLoopVariant2(builder) {
  builder.addFunction("loop_v2", makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI64]))
    .exportFunc()
    .addLocals(kWasmI32, 2) // i (local 4), limit (local 5)
    .addLocals(kWasmI64, 1) // carry (local 6)
    .addBody([
      kExprLocalGet, 3,
      kExprI32Const, 3,
      kExprI32Shl,
      kExprLocalSet, 5,
      kExprI32Const, 0,
      kExprLocalSet, 4,
      kExprI64Const, 0,
      kExprLocalSet, 6,

      kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprI32GeS,
      kExprBrIf, 1,

      kExprLocalGet, 2,
      kExprLocalGet, 4,
      kExprI32Add,

      // Load a[i]
      kExprLocalGet, 0,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // Get previous carry
      kExprLocalGet, 6,
      kExprI64Const, 0,

      // (a[i] + carry) -> (temp_sum, temp_carry)
      kNumericPrefix, kExprI64Add128,

      // Load b[i]
      kExprLocalGet, 1,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // ((a[i] + carry) + b[i]) -> (sum[i], next_carry)
      kNumericPrefix, kExprI64Add128,

      kExprLocalSet, 6,
      kExprI64StoreMem, 3, 0,

      kExprLocalGet, 4,
      kExprI32Const, 8,
      kExprI32Add,
      kExprLocalSet, 4,
      kExprBr, 0,
      kExprEnd,
      kExprEnd,
      kExprLocalGet, 6,
    ]);
}

// Variant 3: (carry + b[i]) + a[i]
function addLoopVariant3(builder) {
  builder.addFunction("loop_v3", makeSig([kWasmI32, kWasmI32, kWasmI32, kWasmI32], [kWasmI64]))
    .exportFunc()
    .addLocals(kWasmI32, 2) // i (local 4), limit (local 5)
    .addLocals(kWasmI64, 1) // carry (local 6)
    .addBody([
      kExprLocalGet, 3,
      kExprI32Const, 3,
      kExprI32Shl,
      kExprLocalSet, 5,
      kExprI32Const, 0,
      kExprLocalSet, 4,
      kExprI64Const, 0,
      kExprLocalSet, 6,

      kExprBlock, kWasmVoid,
      kExprLoop, kWasmVoid,
      kExprLocalGet, 4,
      kExprLocalGet, 5,
      kExprI32GeS,
      kExprBrIf, 1,

      kExprLocalGet, 2,
      kExprLocalGet, 4,
      kExprI32Add,

      // Get previous carry
      kExprLocalGet, 6,
      kExprI64Const, 0,

      // Load b[i]
      kExprLocalGet, 1,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // (carry + b[i]) -> (temp_sum, temp_carry)
      kNumericPrefix, kExprI64Add128,

      // Load a[i]
      kExprLocalGet, 0,
      kExprLocalGet, 4,
      kExprI32Add,
      kExprI64LoadMem, 3, 0,
      kExprI64Const, 0,

      // ((carry + b[i]) + a[i]) -> (sum[i], next_carry)
      kNumericPrefix, kExprI64Add128,

      kExprLocalSet, 6,
      kExprI64StoreMem, 3, 0,

      kExprLocalGet, 4,
      kExprI32Const, 8,
      kExprI32Add,
      kExprLocalSet, 4,
      kExprBr, 0,
      kExprEnd,
      kExprEnd,
      kExprLocalGet, 6,
    ]);
}

function runLoopTest(loopFunc, memory, a_data, b_data, expected_res, expected_carry) {
  let view = new DataView(memory.buffer);
  let N = a_data.length;
  for (let i = 0; i < N; i++) {
    view.setBigUint64(i * 8, a_data[i], true);
  }
  for (let i = 0; i < N; i++) {
    view.setBigUint64((N + i) * 8, b_data[i], true);
  }
  for (let i = 0; i < N; i++) {
    view.setBigUint64((2 * N + i) * 8, 0n, true);
  }

  let carry = loopFunc(0, N * 8, 2 * N * 8, N);

  assertEquals(expected_carry, carry);
  for (let i = 0; i < N; i++) {
    assertEquals(expected_res[i], view.getBigUint64((2 * N + i) * 8, true));
  }
}

function testBigIntAddLoops() {
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  addLoopVariant1(builder);
  addLoopVariant2(builder);
  addLoopVariant3(builder);

  let instance = builder.instantiate();
  let memory = instance.exports.memory;

  const max = 0xffffffffffffffffn;

  const test_cases = [
    // Case 1: Multi-limb addition with no carries across any limbs.
    {
      a: [1n, 2n, 3n, 4n],
      b: [5n, 6n, 7n, 8n],
      expected_res: [6n, 8n, 10n, 12n],
      expected_carry: 0n
    },
    // Case 2: Single-limb carry propagation from limb 0 to limb 1.
    {
      a: [max, 2n, 3n, 4n],
      b: [2n, 6n, 7n, 8n],
      expected_res: [1n, 9n, 10n, 12n],
      expected_carry: 0n
    },
    // Case 3: Cascading / ripple carry across multiple saturated limbs.
    {
      a: [max, max, 3n, 4n],
      b: [1n, 0n, 7n, 8n],
      expected_res: [0n, 0n, 11n, 12n],
      expected_carry: 0n
    },
    // Case 4: Full-width ripple carry through all 4 limbs with loop exit carry.
    {
      a: [max, max, max, max],
      b: [1n, 0n, 0n, 0n],
      expected_res: [0n, 0n, 0n, 0n],
      expected_carry: 1n
    }
  ];

  for (let loopFunc of [instance.exports.loop_v1, instance.exports.loop_v2, instance.exports.loop_v3]) {
    for (let tc of test_cases) {
      runLoopTest(loopFunc, memory, tc.a, tc.b, tc.expected_res, tc.expected_carry);
    }
  }
}

function testAdd3DroppedCarry() {
  let builder = new WasmModuleBuilder();
  builder.addFunction("add3_dropped_carry", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprLocalGet, 1, // b
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprLocalGet, 2, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprDrop, // drop carry-out, return low sum
    ]);

  let instance = builder.instantiate();
  let f = instance.exports.add3_dropped_carry;

  const max = 0xffffffffffffffffn;
  assertEquals(6n, f(1n, 2n, 3n));
  assertEquals(4n, f(max, 2n, 3n));
  assertEquals(1n, f(max - 1n, 1n, 2n));
  assertEquals(0n, f(max, 2n, max));
}

function testAdd3MultiUseIntermediate() {
  let builder = new WasmModuleBuilder();
  // Computes (a + b) and ((a + b) + c), returning (final_sum, final_carry, intermediate_extra).
  // Projection(0) of (a + b) is consumed by an i64.add before the second add128,
  // making its saturated_use_count in the output graph > 0.
  // Therefore, TryMatchAdd3 must reject the fusion, leaving two separate additions.
  builder.addFunction("add3_multi_use", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64, kWasmI64]))
    .exportFunc()
    .addLocals(kWasmI64, 3) // local 3: al, local 4: ah, local 5: al_extra
    .addBody([
      kExprLocalGet, 0, // a
      kExprI64Const, 0,
      kExprLocalGet, 1, // b
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprLocalSet, 4, // ah = Projection(1)
      kExprLocalTee, 3, // al = Projection(0)
      kExprI64Const, 1,
      kExprI64Add,
      kExprLocalSet, 5, // local 5 = al + 1
      kExprLocalGet, 3, // al
      kExprLocalGet, 4, // ah
      kExprLocalGet, 2, // c
      kExprI64Const, 0,
      kNumericPrefix, kExprI64Add128,
      kExprLocalGet, 5, // return [final_sum, final_carry, al + 1]
    ]);

  let instance = builder.instantiate();
  let f = instance.exports.add3_multi_use;

  const max = 0xffffffffffffffffn;
  assertEquals([60n, 0n, 31n], f(10n, 20n, 30n));
  assertEquals([5n, 1n, 2n], f(max, 2n, 4n));
}

testAdd3Pattern();
testAdd3Constants();
testAdd3DroppedCarry();
testAdd3MultiUseIntermediate();
testBigIntAddLoops();
