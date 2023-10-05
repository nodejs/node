// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-multi-memory --experimental-wasm-memory64

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function assertMemoryEquals(expected, memory) {
  assertInstanceof(memory, WebAssembly.Memory);
  assertInstanceof(expected, Uint8Array);
  const buf = new Uint8Array(memory.buffer);
  // For better output, check the first 50 bytes separately first.
  assertEquals(expected.slice(0, 50), buf.slice(0, 50));
  // Now also check the full memory content.
  assertEquals(expected, buf);
}

(function testBasicMultiMemory() {
  print(arguments.callee.name);
  // Test two memories: One 32-bit and one 64-bit, in both orders.
  for (let mem64_idx of [0, 1]) {
    const builder = new WasmModuleBuilder();
    const mem32_idx = 1 - mem64_idx;

    const mem32_pages = 1;
    const mem32_size = mem32_pages * kPageSize;
    const mem64_pages = 3;
    const mem64_size = mem64_pages * kPageSize;
    if (mem32_idx == 0) {
      builder.addMemory(mem32_pages, mem32_pages);
      builder.addMemory64(mem64_pages, mem64_pages);
    } else {
      builder.addMemory64(mem64_pages, mem64_pages);
      builder.addMemory(mem32_pages, mem32_pages);
    }
    builder.exportMemoryAs('mem32', mem32_idx);
    builder.exportMemoryAs('mem64', mem64_idx);

    builder.addFunction('load32', kSig_i_i)
        .addBody([kExprLocalGet, 0, kExprI32LoadMem, 0x40, mem32_idx, 0])
        .exportFunc();
    builder.addFunction('load64', kSig_i_l)
        .addBody([kExprLocalGet, 0, kExprI32LoadMem, 0x40, mem64_idx, 0])
        .exportFunc();

    const instance = builder.instantiate();
    const mem32_offset = 48;
    const mem64_offset = 16;
    const value = 13;
    const view32 = new DataView(instance.exports.mem32.buffer);
    const view64 = new DataView(instance.exports.mem64.buffer);
    view32.setInt32(mem32_offset, value, true);
    view64.setInt32(mem64_offset, value, true);

    const {load32, load64} = instance.exports;

    assertEquals(value, load32(mem32_offset));
    assertEquals(0, load32(mem64_offset));
    assertEquals(0, load64(BigInt(mem32_offset)));
    assertEquals(value, load64(BigInt(mem64_offset)));

    assertEquals(0, load32(mem32_size - 4));
    assertEquals(0, load64(BigInt(mem64_size - 4)));
    assertTraps(kTrapMemOutOfBounds, () => load32(mem32_size - 3));
    assertTraps(kTrapMemOutOfBounds, () => load64(BigInt(mem64_size - 3)));
  }
})();

(function testMultiMemoryInit() {
  print(arguments.callee.name);
  // Test two memories: One 32-bit and one 64-bit, in both orders.
  for (let mem64_idx of [0, 1]) {
    const builder = new WasmModuleBuilder();
    const mem32_idx = 1 - mem64_idx;

    const mem32_pages = 1;
    const mem32_size = mem32_pages * kPageSize;
    const mem64_pages = 3;
    const mem64_size = mem64_pages * kPageSize;
    if (mem32_idx == 0) {
      builder.addMemory(mem32_pages, mem32_pages);
      builder.addMemory64(mem64_pages, mem64_pages);
    } else {
      builder.addMemory64(mem64_pages, mem64_pages);
      builder.addMemory(mem32_pages, mem32_pages);
    }
    builder.exportMemoryAs('mem32', mem32_idx);
    builder.exportMemoryAs('mem64', mem64_idx);

    const data = [1, 2, 3, 4, 5];
    const seg_id = builder.addPassiveDataSegment(data);

    builder.addFunction('init32', kSig_v_iii)
        .addBody([
          kExprLocalGet, 0,  // dst
          kExprLocalGet, 1,  // offset
          kExprLocalGet, 2,  // size
          kNumericPrefix, kExprMemoryInit, seg_id, mem32_idx
        ])
        .exportFunc();
    builder.addFunction('init64', kSig_v_lii)
        .addBody([
          kExprLocalGet, 0,  // dst
          kExprLocalGet, 1,  // offset
          kExprLocalGet, 2,  // size
          kNumericPrefix, kExprMemoryInit, seg_id, mem64_idx
        ])
        .exportFunc();

    const instance = builder.instantiate();
    const {init32, init64} = instance.exports;
    const expected_mem32 = new Uint8Array(mem32_size);
    const expected_mem64 = new Uint8Array(mem64_size);

    assertMemoryEquals(expected_mem32, instance.exports.mem32);
    assertMemoryEquals(expected_mem64, instance.exports.mem64);

    init32(7, 1, 3);  // dst, (data) offset, size
    expected_mem32.set(data.slice(1, 4), 7);
    assertMemoryEquals(expected_mem32, instance.exports.mem32);
    assertMemoryEquals(expected_mem64, instance.exports.mem64);

    init64(11n, 3, 2);  // dst, (data) offset, size
    expected_mem64.set(data.slice(3, 5), 11);
    assertMemoryEquals(expected_mem32, instance.exports.mem32);
    assertMemoryEquals(expected_mem64, instance.exports.mem64);

    // Test bounds checks.
    init32(mem32_size - 1, 0, 1);  // dst, (data) offset, size
    assertTraps(kTrapMemOutOfBounds, () => init32(mem32_size - 1, 0, 2));
    init64(BigInt(mem64_size - 1), 0, 1);  // dst, (data) offset, size
    assertTraps(
        kTrapMemOutOfBounds, () => init64(BigInt(mem64_size - 1), 0, 2));
  }
})();
