// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --liftoff --no-wasm-tier-up

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function () {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("imports", "mem", 1);
  builder.addType(makeSig([kWasmI32, kWasmI64], [kWasmI64]));
  // Generate function 1 (out of 1).
  builder.addFunction(undefined, 0 /* sig */).addBodyWithEnd([
    kExprLocalGet, 0,
    kExprLocalGet, 1,
    kAtomicPrefix, kExprI64AtomicAdd, 0x03, 0x00,
    kExprEnd
  ]);
  builder.addExport('atomicAddI64', 0);
  var mem = new WebAssembly.Memory({ initial: 1 });

  let i64arr = new BigUint64Array(mem.buffer);
  new DataView(mem.buffer).setBigUint64(0, 0n, true);
  new DataView(mem.buffer).setBigUint64(8, 0xffffffffn, true);

  const instance = builder.instantiate({ imports: { mem: mem } });
  assertEquals(0n, instance.exports.atomicAddI64(0, 1n));
  assertEquals(1n, instance.exports.atomicAddI64(0, 0n));

  assertEquals(0xffffffffn, instance.exports.atomicAddI64(8, 1n));
  assertEquals(0x100000000n, instance.exports.atomicAddI64(8, 0n));
})();
