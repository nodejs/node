// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function Memory32() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 2, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  const module = builder.instantiate();

  // src less than dst, non-overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 256;
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(array[i], i);
      assertEquals(array[i], array[dst+i]);
    }
  }
  // dst less than src, non-overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 256;
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(array[src+i], array[dst+i]);
    }
  }
  // src less than dst, overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 300;
    const temp = new Uint8Array(array);
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[i], array[dst+i]);
    }
  }
  // dst less than src, overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 300;
    const temp = new Uint8Array(array);
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[src+i], array[dst+i]);
    }
  }
})();

(function Memory64() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 3, false);
  builder.exportMemoryAs('memory');

  builder.addFunction('copy', makeSig([kWasmI64, kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();
  const module = builder.instantiate();

  // src less than dst, non-overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 256;
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(array[i], i);
      assertEquals(array[i], array[dst+i]);
    }
  }
  // dst less than src, non-overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 256;
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(array[src+i], array[dst+i]);
    }
  }
  // src less than dst, overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 300;
    const temp = new Uint8Array(array);
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[i], array[dst+i]);
    }
  }
  // src less than dst, overlapping.
  {
    const array = new Uint8Array(module.exports.memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 300;
    const temp = new Uint8Array(array);
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[src+i], array[dst+i]);
    }
  }
})();

(function MultiMemory32() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory(1, 4, false);
  builder.addMemory(1, 5, false);
  builder.exportMemoryAs('src_memory', 0);
  builder.exportMemoryAs('dst_memory', 1);

  builder.addFunction('copy', makeSig([kWasmI32, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryCopy, 1, 0  // memory.copy srcmem=0 dstmem=1
      ])
      .exportFunc();
  const module = builder.instantiate();

  // src less than dst, non-intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 256;
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(src_array[i], i);
      assertEquals(src_array[i], dst_array[dst+i]);
    }
  }
  // dst less than src, non-intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 256;
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(src_array[src+i], dst_array[dst+i]);
    }
  }
  // src less than dst, intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 300;
    const temp = new Uint8Array(src_array);
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[i], dst_array[dst+i]);
    }
  }
  // dst less than src, intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 300;
    const temp = new Uint8Array(src_array);
    module.exports.copy(dst, src, size);
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[src+i], dst_array[dst+i]);
    }
  }
})();

(function MultiMemory64() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 6, false);
  builder.addMemory64(1, 7, false);
  builder.exportMemoryAs('dst_memory', 0);
  builder.exportMemoryAs('src_memory', 1);

  builder.addFunction('copy', makeSig([kWasmI64, kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryCopy, 0, 1  // memory.copy srcmem=1 dstmem=0
      ])
      .exportFunc();
  const module = builder.instantiate();

  // src less than dst, non-intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 0n;
    const dst = 256;
    const size = 256;
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(src_array[i], i);
      assertEquals(src_array[i], dst_array[dst+i]);
    }
  }
  // dst less than src, non-intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 256;
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(src_array[src+i], dst_array[dst+i]);
    }
  }
  // src less than dst, intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 0;
    const dst = 256;
    const size = 300;
    const temp = new Uint8Array(src_array);
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[i], dst_array[dst+i]);
    }
  }
  // dst less than src, intersecting indices.
  {
    const src_array = new Uint8Array(module.exports.src_memory.buffer);
    const dst_array = new Uint8Array(module.exports.dst_memory.buffer);
    for (let i = 0; i < 1024; ++i) {
      src_array[i] = i;
    }
    const src = 256;
    const dst = 0;
    const size = 300;
    const temp = new Uint8Array(src_array);
    module.exports.copy(BigInt(dst), BigInt(src), BigInt(size));
    for (let i = 0; i < size; ++i) {
      assertEquals(temp[src+i], dst_array[dst+i]);
    }
  }
})();
