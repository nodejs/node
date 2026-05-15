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
    kExprLocalGet, 0, kExprLocalGet, 1,
    kExprLocalGet, 2, kExprLocalGet, 3,
    kNumericPrefix, kExprI64Add128,
  ]);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /add128.*Wide arithmetic opcodes are not yet implemented./);
}

function testSub128() {

  let builder = new WasmModuleBuilder();
  builder.addFunction("sub128", kSig_ll_llll).exportFunc().addBody([
    kExprLocalGet, 0, kExprLocalGet, 1,
    kExprLocalGet, 2, kExprLocalGet, 3,
    kNumericPrefix, kExprI64Sub128,
  ]);
  assertThrows(() => builder.instantiate(), WebAssembly.CompileError,
    /sub128.*Wide arithmetic opcodes are not yet implemented./);
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

  let view = new DataView(instance.exports.memory.buffer);
  view.setBigInt64(1, -5n, true);

  let mulWideULoad = instance.exports.mulWideULoad;
  assertEquals([20n, -9n], mulWideULoad(-4n, 0));
}

testAdd128();
testSub128();
testMulWideS();
testMulWideU();
