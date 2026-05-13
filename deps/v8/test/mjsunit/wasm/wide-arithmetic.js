// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-wide-arithmetic

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// TODO(ryandiaz): these tests are just placeholders for now, until we have
// implemented the wide arithmetic instructions.
let kSig_ll_llll = makeSig(
  [kWasmI64, kWasmI64, kWasmI64, kWasmI64],
  [kWasmI64, kWasmI64]
);
let kSig_ll_ll = makeSig([kWasmI64, kWasmI64], [kWasmI64, kWasmI64]);

function testAdd128() {

  let builder = new WasmModuleBuilder();
  builder.addFunction("add128", kSig_ll_llll).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kExprLocalGet, 3,
    kNumericPrefix, kExprI64Add128,
  ]);

  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  builder.addFunction("add128Load", kSig_ll_llll)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 0,
      kExprLocalGet, 2,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 8,
      kNumericPrefix, kExprI64Add128,
    ]);

  builder.addFunction("add128_alias4", makeSig([kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kNumericPrefix, kExprI64Add128,
    ]);

  builder.addFunction("add128_alias3", makeSig([kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kNumericPrefix, kExprI64Add128,
    ]);

  builder.addFunction("add128_alias2", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kNumericPrefix, kExprI64Add128,
    ]);

  builder.addFunction("add128Load_alias2", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 0,
      kExprLocalGet, 0,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kNumericPrefix, kExprI64Add128,
    ]);

  builder.addFunction("add128Load_alias_high_low", makeSig([kWasmI64, kWasmI64, kWasmI64], [kWasmI64, kWasmI64]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 0, // a_low = mem[arg0]
      kExprLocalGet, 1, // a_high
      kExprLocalGet, 2, // b_low
      kExprLocalGet, 0,
      kExprI32ConvertI64,
      kExprI64LoadMem, 3, 0, // b_high = mem[arg0]
      kNumericPrefix, kExprI64Add128,
    ]);

  let instance;
  try {
    instance = builder.instantiate();
  } catch (e) {
    if (e instanceof WebAssembly.CompileError &&
      e.message.includes("Wide arithmetic opcodes are not yet implemented")) {
      console.log("add128 not implemented on this architecture/compiler.");
      return;
    }
    throw e;
  }
  let add128 = instance.exports.add128;
  assertEquals([0n, 0n], add128(0n, 0n, 0n, 0n));
  assertEquals([5n, 0n], add128(2n, 0n, 3n, 0n));
  assertEquals([1n, 1n], add128(-1n, 0n, 2n, 0n));
  assertEquals([0n, 1n], add128(-1n, 0n, 1n, 0n));
  assertEquals([0n, 0n], add128(-1n, -1n, 1n, 0n));
  assertEquals(
    [0n, 16n],
    add128(0xffffffffffffff00n, 5n, 0x0000000000000100n, 10n)
  );
  assertEquals(
    [0x6666666688888888n, -0x5555555533333334n],
    add128(
      0x1111111122222222n,
      0x3333333344444444n,
      0x5555555566666666n,
      0x7777777788888888n
    )
  );
  assertEquals(
    [1n, 4n],
    add128(0xffffffffffffffffn, 1n, 2n, 2n)
  );

  let view = new DataView(instance.exports.memory.buffer);
  view.setBigInt64(0, 3n, true);
  view.setBigInt64(8, 0n, true);

  let add128Load = instance.exports.add128Load;
  assertEquals([5n, 0n], add128Load(2n, 0n, 0n, 0n));

  view.setBigInt64(16, 1n, true);
  view.setBigInt64(24, 0n, true);
  assertEquals([0n, 1n], add128Load(-1n, 0n, 16n, 0n));

  let add128_alias4 = instance.exports.add128_alias4;
  assertEquals([10n, 10n], add128_alias4(5n));
  assertEquals([-2n, -1n], add128_alias4(-1n));

  let add128_alias3 = instance.exports.add128_alias3;
  assertEquals([10n, 15n], add128_alias3(5n, 10n));

  let add128_alias2 = instance.exports.add128_alias2;
  assertEquals([15n, 20n], add128_alias2(5n, 10n, 15n));

  let add128Load_alias2 = instance.exports.add128Load_alias2;
  view.setBigInt64(32, 5n, true);
  assertEquals([15n, 20n], add128Load_alias2(32n, 10n, 15n));

  let add128Load_alias_high_low = instance.exports.add128Load_alias_high_low;
  view.setBigInt64(40, 5n, true);
  assertEquals([20n, 15n], add128Load_alias_high_low(40n, 10n, 15n));
}

function testSub128() {

  let builder = new WasmModuleBuilder();
  builder.addFunction("sub128", kSig_ll_llll).exportFunc().addBody([
    kExprLocalGet, 0, kExprLocalGet, 1,
    kExprLocalGet, 2, kExprLocalGet, 3,
    kNumericPrefix, kExprI64Sub128,
  ]);
  let instance;
  try {
    instance = builder.instantiate();
  } catch (e) {
    if (e instanceof WebAssembly.CompileError &&
      e.message.includes("Wide arithmetic opcodes are not yet implemented")) {
      console.log("sub128 not implemented on this architecture/compiler.");
      return;
    }
    throw e;
  }
  let sub128 = instance.exports.sub128;
  // Order of args a_lo, a_hi, b_lo, b_hi
  assertEquals([0n, 0n], sub128(0n, 0n, 0n, 0n));
  assertEquals([2n, 0n], sub128(5n, 0n, 3n, 0n));
  assertEquals([0n, 2n], sub128(0n, 5n, 0n, 3n));
  assertEquals([0n, 1n], sub128(0n, 1n, 0n, 0n));
  assertEquals([-1n, 0n], sub128(-1n, 0n, 0n, 0n));
  assertEquals([-1n, 0n], sub128(0n, 1n, 1n, 0n));
  assertEquals([-5n, 0n], sub128(5n, 1n, 10n, 0n));
  assertEquals([-1n, 0x7fffffffffffffffn],
    sub128(0n, -0x8000000000000000n, 1n, 0n)
  );
}

function testMulWideS() {

  let builder = new WasmModuleBuilder();
  builder.addFunction("mulWideS", kSig_ll_ll)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kNumericPrefix, kExprI64MulWideS,
    ]);

  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  builder.addFunction(
    "mulWideSLoad",
    makeSig([kWasmI64, kWasmI32], [kWasmI64, kWasmI64])
  )
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI64LoadMem, 3, 1,
      kNumericPrefix, kExprI64MulWideS,
    ]);

  let instance;
  try {
    instance = builder.instantiate();
  } catch (e) {
    if (e instanceof WebAssembly.CompileError &&
        e.message.includes("Wide arithmetic opcodes are not yet implemented")) {
      console.log("mulWideS not implemented on this architecture/compiler.");
      return;
    }
    throw e;
  }

  let mulWideS = instance.exports.mulWideS;
  assertEquals([0n, 0n], mulWideS(0n, 0n));
  assertEquals([20n, 0n], mulWideS(4n, 5n));
  assertEquals([-20n, -1n], mulWideS(4n, -5n));
  assertEquals([-20n, -1n], mulWideS(-4n, 5n));
  assertEquals([20n, 0n], mulWideS(-4n, -5n));
  assertEquals([-2n, 0n], mulWideS(0x7fffffffffffffffn, 2n));
  assertEquals([-0x8000000000000000n, 0n], mulWideS(-0x8000000000000000n, -1n));
  assertEquals(
      [1n, 0x3fffffffffffffffn],
      mulWideS(0x7fffffffffffffffn, 0x7fffffffffffffffn));
  assertEquals(
      [0n, -0x80000000n], mulWideS(-0x8000000000000000n, 0x100000000n));
  assertEquals(
      [0x7fffffff00000001n, 0x7fffffffn],
      mulWideS(0xffffffffn, 0x7fffffffffffffffn));

  let view = new DataView(instance.exports.memory.buffer);
  view.setBigInt64(1, -5n, true);

  let mulWideSLoad = instance.exports.mulWideSLoad;
  assertEquals([20n, 0n], mulWideSLoad(-4n, 0));
}

function testMulWideU() {

  let builder = new WasmModuleBuilder();
  builder.addFunction("mulWideU", kSig_ll_ll)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kNumericPrefix, kExprI64MulWideU,
    ]);

  builder.addMemory(1, 1);
  builder.exportMemoryAs('memory');

  builder.addFunction(
    "mulWideULoad",
    makeSig([kWasmI64, kWasmI32], [kWasmI64, kWasmI64])
  )
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI64LoadMem, 3, 1,
      kNumericPrefix, kExprI64MulWideU,
    ]);

  let instance;
  try {
    instance = builder.instantiate();
  } catch (e) {
    if (e instanceof WebAssembly.CompileError &&
        e.message.includes("Wide arithmetic opcodes are not yet implemented")) {
      console.log("mulWideU not implemented on this architecture/compiler.");
      return;
    }
    throw e;
  }

  let mulWideU = instance.exports.mulWideU;
  assertEquals([0n, 0n], mulWideU(0n, 0n));
  assertEquals([20n, 0n], mulWideU(4n, 5n));
  assertEquals([-1n, 0n], mulWideU(0xffffffffffffffffn, 1n));
  assertEquals([-2n, 1n], mulWideU(0xffffffffffffffffn, 2n));
  assertEquals([20n, -9n], mulWideU(-4n, -5n));
  assertEquals([1n, -2n], mulWideU(0xffffffffffffffffn, 0xffffffffffffffffn));
  assertEquals([0n, 0x80000000n], mulWideU(0x8000000000000000n, 0x100000000n));
  assertEquals(
      [0x7fffffff00000001n, 0x7fffffffn],
      mulWideU(0xffffffffn, 0x7fffffffffffffffn));

  let view = new DataView(instance.exports.memory.buffer);
  view.setBigInt64(1, -5n, true);

  let mulWideULoad = instance.exports.mulWideULoad;
  assertEquals([20n, -9n], mulWideULoad(-4n, 0));
}

testAdd128();
testSub128();
testMulWideS();
testMulWideU();
