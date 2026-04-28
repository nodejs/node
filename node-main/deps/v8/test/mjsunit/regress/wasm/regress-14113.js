// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const I64AtomicOps = [
  kExprI64AtomicAdd32U,
  kExprI64AtomicSub32U,
  kExprI64AtomicAnd32U,
  kExprI64AtomicOr32U,
  kExprI64AtomicXor32U,
  kExprI64AtomicExchange32U,
];

const Inputs = [
  [0x0ffffffffn, 1n, 0n],
  [0x200000000n, 1n, 0x2ffffffffn],
  [0x1ffffffffn, 1n, 0x100000001n],
  [0x1ffffffffn, 1n, 0x1ffffffffn],
  [0x1ffffffffn, 0xffffffffn, 0x100000000n],
  [0x1ffffffffn, 0xfffffffen, 0x1fffffffen],
];

// Test that bits beyond position 31 of the second
// operand do not affect result at all.
const Inputs2 = [
  [0x0ffffffffn, 0xbee00000001n, 0n],
  [0x200000000n, 0xbee00000001n, 0x2ffffffffn],
  [0x1ffffffffn, 0xbee00000001n, 0x100000001n],
  [0x1ffffffffn, 0xbee00000001n, 0x1ffffffffn],
  [0x1ffffffffn, 0xbeeffffffffn, 0x100000000n],
  [0x1ffffffffn, 0xbeefffffffen, 0x1fffffffen],
];

function TestBinOp64(index, memory, inputs) {
  const Op = I64AtomicOps[index];
  const sample = inputs[index];

  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("imports", "mem", 1);
  builder.addType(makeSig([kWasmI32, kWasmI64], [kWasmI64]));
  // Generate function 1 (out of 1).
  builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kAtomicPrefix, Op, 0x02, 0x00,
    kExprEnd
  ]);
  builder.addExport('run', 0);
  const instance = builder.instantiate({ imports: { mem: memory } });

  let dv = new DataView(memory.buffer);
  dv.setBigUint64(index * 8, sample[0], true);
  assertEquals(sample[0] & 0xffffffffn, instance.exports.run(index * 8, sample[1]));
  assertEquals(sample[2], dv.getBigUint64(index * 8, true));
}

function runTestWithInputs(inputs) {
  var mem = new WebAssembly.Memory({ initial: 1 });
  TestBinOp64(0, mem, inputs, "i64.atomic.rmw32.add");
  TestBinOp64(1, mem, inputs, "i64.atomic.rmw32.sub");
  TestBinOp64(2, mem, inputs, "i64.atomic.rmw32.and");
  TestBinOp64(3, mem, inputs, "i64.atomic.rmw32.or");
  TestBinOp64(4, mem, inputs, "i64.atomic.rmw32.xor");
  TestBinOp64(5, mem, inputs, "i64.atomic.rmw32.xchg");
}

(function () {
  runTestWithInputs(Inputs);
  runTestWithInputs(Inputs2);
})();
