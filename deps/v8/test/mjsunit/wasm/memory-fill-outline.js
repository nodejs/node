// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/value-helper.js');

(function OutlineMemoryFillWithInt32() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 10, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('fill', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0,                     // local.get 0 (dst)
      kExprLocalGet, 1,                     // local.get 1 (value)
      kExprLocalGet, 2,                     // local.get 1 (size)
      kNumericPrefix, kExprMemoryFill, 0    // memory.fill dstmem=0
    ])
    .exportFunc();

  const num_bytes = 111;
  const module = builder.instantiate();
  const array = new Uint8Array(module.exports.memory.buffer);
  for (let i = 0; i < int32_array.length; ++i) {
    const dst = i * num_bytes;
    const value = int32_array[i];
    module.exports.fill(dst, value, num_bytes);
    // Test that we haven't overwritten a previous write.
    if (i > 0) {
      assertEquals(int32_array[i-1] & 0xFF, array[dst - 1]);
    }
    for (let j = dst; j < dst + num_bytes; ++j) {
      assertEquals(array[j], value & 0xFF);
    }
    // Test that we haven't written past the limit.
    assertEquals(0, array[dst + num_bytes]);
  }
})();

(function Memory32OOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 4, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('fill', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (value)
        kExprLocalGet, 2,                      // local.get 1 (size)
        kNumericPrefix, kExprMemoryFill, 0     // memory.fill dstmem=0
      ])
      .exportFunc();

  const num_bytes = 1;
  const module = builder.instantiate();
  const value = 1;
  const dst = kPageSize - num_bytes + 1;
  assertTraps(kTrapMemOutOfBounds, () =>
              module.exports.fill(dst, value, num_bytes));
})();

(function Memory64OOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(2, 3, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('fill', makeSig([kWasmI64, kWasmI32, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (value)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryFill, 0     // memory.fill dstmem=0
      ])
      .exportFunc();

  const num_bytes = 143;
  const module = builder.instantiate();
  const value = 1;
  const dst = BigInt(2 * kPageSize - num_bytes + 1);
  assertTraps(kTrapMemOutOfBounds, () =>
              module.exports.fill(dst, value, BigInt(num_bytes)));
})();

(function MultiMemory32OOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const num_pages = 2;
  builder.addMemory(num_pages + 1, 4, false);
  builder.addMemory(num_pages, 3, false);
  builder.exportMemoryAs('memory', 1);

  const num_bytes = 143;
  const fill = 'fill' + num_bytes.toString();
  builder.addFunction('fill', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (value)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryFill, 1     // memory.fill dstmem=1
      ])
      .exportFunc();

  const module = builder.instantiate();
  const value = 1;
  const dst = num_pages * kPageSize - num_bytes + 1;
  assertTraps(kTrapMemOutOfBounds, () =>
              module.exports.fill(dst, value, num_bytes));
})();

(function MultiMemory64OOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const num_pages = 1;
  builder.addMemory64(num_pages + 1, 5, false);
  builder.addMemory64(num_pages, 3, false);
  builder.exportMemoryAs('memory', 1);

  builder.addFunction('fill', makeSig([kWasmI64, kWasmI32, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (value)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryFill, 1     // memory.fill dstmem=1
      ])
      .exportFunc();

  const num_bytes = 1;
  const module = builder.instantiate();
  const value = 1;
  const dst = BigInt(num_pages * kPageSize - num_bytes + 1);
  assertTraps(kTrapMemOutOfBounds, () =>
              module.exports.fill(dst, value, BigInt(num_bytes)));
})();
