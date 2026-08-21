// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function InlineMemoryCopy() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 10, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy3', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(3),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy4', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(4),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy5', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(5),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy7', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(7),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy8', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(8),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy9', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(9),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy15', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(15),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy16', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(16),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy17', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(17),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy31', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(31),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy32', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(32),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy33', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(33),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy63', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(63),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy64', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(64),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy65', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(65),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy68', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(68),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy95', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(95),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy111', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(111),                  // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  builder.addFunction('copy112', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(112),                  // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  // Initialize memory.
  let array = new Uint8Array(module.exports.memory.buffer);
  for (let i = 0; i < 256; ++i) {
    array[i] = i;
  }

  // Set src and dst so that for some of the tests, their ranges overlap.
  const src = 0;
  const dst = 80;

  // Perform copies where the ranges don't overlap.
  module.exports.copy3(dst, src);
  for (let i = 0; i < 3; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy4(dst, src);
  for (let i = 0; i < 4; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy5(dst, src);
  for (let i = 0; i < 5; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy7(dst, src);
  for (let i = 0; i < 7; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy8(dst, src);
  for (let i = 0; i < 8; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy9(dst, src);
  for (let i = 0; i < 9; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy15(dst, src);
  for (let i = 0; i < 15; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy16(dst, src);
  for (let i = 0; i < 16; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy31(dst, src);
  for (let i = 0; i < 31; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy32(dst, src);
  for (let i = 0; i < 32; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy33(dst, src);
  for (let i = 0; i < 33; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy63(dst, src);
  for (let i = 0; i < 63; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy64(dst, src);
  for (let i = 0; i < 64; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy65(dst, src);
  for (let i = 0; i < 65; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }
  module.exports.copy68(dst, src);
  for (let i = 0; i < 68; ++i) {
    assertEquals(array[i], i);
    assertEquals(array[i], array[dst+i]);
  }

  // Now the ranges overlap, so create a temp buffer for testing.
  let temp = new Uint8Array(array);
  module.exports.copy95(dst, src);
  for (let i = 0; i < dst; ++i) {
    assertEquals(array[i], i);
  }
  for (let i = 0; i < 95; ++i) {
    assertEquals(temp[i], array[dst+i]);
  }

  temp = new Uint8Array(array);
  module.exports.copy111(dst, src);
  for (let i = 0; i < dst; ++i) {
    assertEquals(array[i], i);
  }
  for (let i = 0; i < 111; ++i) {
    assertEquals(temp[i], array[dst+i]);
  }

  temp = new Uint8Array(array);
  module.exports.copy112(dst, src);
  for (let i = 0; i < dst; ++i) {
    assertEquals(array[i], i);
  }
  for (let i = 0; i < 111; ++i) {
    assertEquals(temp[i], array[dst+i]);
  }
})();

(function InlineMemory64Copy() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  const num_pages = 4;
  builder.addMemory64(num_pages, num_pages, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy111', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(111),                  // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  // Initialize memory.
  const array = new Uint8Array(module.exports.memory.buffer);
  for (let i = 0; i < kPageSize * num_pages; ++i) {
    array[i] = i & 255;
  }

  // Set src and dst so that their ranges overlap.
  const src = BigInt(2 * kPageSize);
  const dst = BigInt(2 * kPageSize - 1);
  const num_bytes = 111;

  // The ranges overlap, so create a temp buffer for testing.
  let temp = new Uint8Array(array);
  module.exports.copy111(dst, src);
  for (let i = 0; i < num_bytes; ++i) {
    assertEquals(temp[i], array[2 * kPageSize - 1 + i]);
  }
})();

(function Memory32SrcOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 12, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy71', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(71),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 71;
  const src = kPageSize - num_bytes + 1;
  const dst = kPageSize >> 1;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy71(dst, src));
})();

(function Memory32DstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 5, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy2', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(3),                    // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 2;
  const src = 1;
  const dst = kPageSize - 1;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy2(dst, src));
})();

(function Memory32SrcDstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(3, 13, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy112', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(112),                  // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 112;
  const src = 3 * kPageSize - num_bytes + 1;
  const dst = 3 * kPageSize;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy112(dst, src));
})();

(function MultiMemorySrcOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 12, false);
  builder.addMemory(1, 14, false);
  builder.exportMemoryAs('src_memory', 0);
  builder.exportMemoryAs('dst_memory', 1);

  builder.addFunction('copy64', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(64),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 1, 0  // memory.copy srcmem=0 dstmem=1
      ])
      .exportFunc();

  const module = builder.instantiate();
  const src_array = new Uint8Array(module.exports.src_memory.buffer);
  const num_bytes = 64;
  const src = kPageSize - num_bytes + 1;
  const dst = 0;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy64(dst, src));
})();

(function MultiMemoryDstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 15, false);
  builder.addMemory(1, 16, false);
  builder.exportMemoryAs('src_memory', 0);
  builder.exportMemoryAs('dst_memory', 1);

  builder.addFunction('copy1', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(1),                    // (size)
        kNumericPrefix, kExprMemoryCopy, 1, 0  // memory.copy srcmem=0 dstmem=1
      ])
      .exportFunc();

  const module = builder.instantiate();
  const src_array = new Uint8Array(module.exports.src_memory.buffer);
  const num_bytes = 1;
  const dst = kPageSize;
  const src = 0;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy1(dst, src));
})();

(function MultiMemorySrcDstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 17, false);
  builder.addMemory(1, 18, false);
  builder.exportMemoryAs('src_memory', 0);
  builder.exportMemoryAs('dst_memory', 1);

  builder.addFunction('copy1', makeSig([kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI32Const(1),                    // (size)
        kNumericPrefix, kExprMemoryCopy, 1, 0  // memory.copy srcmem=0 dstmem=1
      ])
      .exportFunc();

  const module = builder.instantiate();
  const src_array = new Uint8Array(module.exports.src_memory.buffer);
  const num_bytes = 1;
  const dst = kPageSize - num_bytes + 1;
  const src = kPageSize - num_bytes + 2;
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy1(dst, src));
})();

(function Memory64DstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 19, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy2', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(2),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const array = new Uint8Array(module.exports.memory.buffer);
  const num_bytes = 2;
  const dst = BigInt(0);
  const src = BigInt(kPageSize - 1);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy2(dst, src));
})();

(function Memory64CopySrcOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 20, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy33', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(33),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const array = new Uint8Array(module.exports.memory.buffer);
  const num_bytes = 33;
  const dst = BigInt(0);
  const src = BigInt(kPageSize - num_bytes + 1);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy33(dst, src));
})();

(function Memory64SrcDstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 21, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy11', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(11),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const array = new Uint8Array(module.exports.memory.buffer);
  const num_bytes = 11;
  const dst = BigInt(kPageSize - num_bytes + 1);
  const src = BigInt(kPageSize - num_bytes + 3);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy11(dst, src));
})();

(function MultiMemory64SrcOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 22, false);
  builder.addMemory64(1, 23, false);
  builder.exportMemoryAs('dst_memory', 0);
  builder.exportMemoryAs('src_memory', 1);

  builder.addFunction('copy1', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(1),                    // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 1  // memory.copy srcmem=1 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 1;
  const dst = BigInt(0);
  const src = BigInt(kPageSize);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy1(dst, src));
})();

(function MultiMemory64DstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(2, 3, false);
  builder.addMemory64(1, 4, false);
  builder.exportMemoryAs('dst_memory', 0);
  builder.exportMemoryAs('src_memory', 1);

  builder.addFunction('copy96', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(96),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 1  // memory.copy srcmem=1 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 96;
  const src = BigInt(0);
  const dst = BigInt(2 * kPageSize - num_bytes + 1);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy96(dst, src));
})();

(function MultiMemory64SrcDstOOB() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 5, false);
  builder.addMemory64(2, 6, false);
  builder.exportMemoryAs('dst_memory', 0);
  builder.exportMemoryAs('src_memory', 1);

  builder.addFunction('copy64', makeSig([kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        ...wasmI64Const(64),                   // (size)
        kNumericPrefix, kExprMemoryCopy, 0, 1  // memory.copy srcmem=1 dstmem=0
      ])
      .exportFunc();

  const module = builder.instantiate();
  const num_bytes = 64;
  const src = BigInt(2 * kPageSize - num_bytes + 10);
  const dst = BigInt(kPageSize - num_bytes + 5);
  assertTraps(kTrapMemOutOfBounds, () => module.exports.copy64(dst, src));
})();
