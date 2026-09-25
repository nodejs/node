// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function () {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("imports", "mem", 1);
  builder.addType(makeSig([kWasmI32, kWasmI64, kWasmI64], [kWasmI64]));
  // Generate function 1 (out of 1).
  builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kExprLocalGet, 2,
    kAtomicPrefix, kExprI64AtomicCompareExchange, 0x03, 0x00,
    kExprEnd
  ]);
  builder.addExport('run', 0);
  var mem = new WebAssembly.Memory({ initial: 1 });

  let i64arr = new BigUint64Array(mem.buffer);
  let dv = new DataView(mem.buffer);
  dv.setBigUint64(0, 0n, true)
  dv.setBigUint64(8, 0x8eeeffffffffn, true)

  const instance = builder.instantiate({ imports: { mem: mem } });
  // Equal, new value is stored and old value returned.
  assertEquals(0n, instance.exports.run(0, 0n, 1n));
  assertEquals(1n, dv.getBigUint64(0, true));
  // Not equal, old value stays.
  assertEquals(1n, instance.exports.run(0, 0n, 2n));
  assertEquals(1n, dv.getBigUint64(0, true));

  // Equal, new value is stored and old value returned.
  assertEquals(0x8eeeffffffffn, instance.exports.run(8, 0x8eeeffffffffn, 0x1000ffffffffn));
  assertEquals(0x1000ffffffffn, dv.getBigUint64(8, true));
  // Not equal, old value stays.
  assertEquals(0x1000ffffffffn, instance.exports.run(8, 0x8eeeffffffffn, 0x1n));
  assertEquals(0x1000ffffffffn, dv.getBigUint64(8, true));
})();
