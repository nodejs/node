// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const I64AtomicOps = [
  kExprI64AtomicAdd,
  kExprI64AtomicSub,
  kExprI64AtomicAnd,
  kExprI64AtomicOr,
  kExprI64AtomicXor,
  kExprI64AtomicExchange,
];

const Inputs = [
  [0x1ffffffffn, 1n, 0x200000000n],
  [0x200000000n, 1n, 0x1ffffffffn],
  [0x1ffffffffn, 1n, 0x1n],
  [0x1ffffffffn, 1n, 0x1ffffffffn],
  [0x1ffffffffn, 0x10ffffffffn, 0x1100000000n],
  [0x1ffffffffn, 0x10ffffffffn, 0x10ffffffffn],
];

function TestBinOp64(index, memory) {
  const Op = I64AtomicOps[index];
  const sample = Inputs[index];

  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("imports", "mem", 1);
  builder.addType(makeSig([kWasmI32, kWasmI64], [kWasmI64]));
  // Generate function 1 (out of 1).
  builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kAtomicPrefix, Op, 0x03, 0x00,
    kExprEnd
  ]);
  builder.addExport('run', 0);
  const instance = builder.instantiate({ imports: { mem: memory } });

  let i64arr = new BigUint64Array(memory.buffer);
  let dv = new DataView(memory.buffer);
  dv.setBigUint64(index * 8, sample[0], true);
  assertEquals(sample[0], instance.exports.run(index * 8, sample[1]));
  assertEquals(sample[2], dv.getBigUint64(index * 8, true));
}

(function () {
  var mem = new WebAssembly.Memory({ initial: 1 });
  TestBinOp64(0, mem, "i64.atomic.rmw.add");
  TestBinOp64(1, mem, "i64.atomic.rmw.sub");
  TestBinOp64(2, mem, "i64.atomic.rmw.and");
  TestBinOp64(3, mem, "i64.atomic.rmw.or");
  TestBinOp64(4, mem, "i64.atomic.rmw.xor");
  TestBinOp64(5, mem, "i64.atomic.rmw.xchg");
})();
