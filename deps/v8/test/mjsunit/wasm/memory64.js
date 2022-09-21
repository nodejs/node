// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-memory64

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// We use standard JavaScript doubles to represent bytes and offsets. They offer
// enough precision (53 bits) for every allowed memory size.

const GB = 1024 * 1024 * 1024;
// The current limit is 16GB. Adapt this test if this changes.
const max_num_pages = 16 * GB / kPageSize;

function BasicMemory64Tests(num_pages) {
  const num_bytes = num_pages * kPageSize;
  print(`Testing ${num_bytes} bytes (${num_pages} pages)`);

  let builder = new WasmModuleBuilder();
  builder.addMemory64(num_pages, num_pages, true);

  builder.addFunction('load', makeSig([kWasmF64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,       // local.get 0
        kExprI64UConvertF64,    // i64.uconvert_sat.f64
        kExprI32LoadMem, 0, 0,  // i32.load_mem align=1 offset=0
      ])
      .exportFunc();
  builder.addFunction('store', makeSig([kWasmF64, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,        // local.get 0
        kExprI64UConvertF64,     // i64.uconvert_sat.f64
        kExprLocalGet, 1,        // local.get 1
        kExprI32StoreMem, 0, 0,  // i32.store_mem align=1 offset=0
      ])
      .exportFunc();

  let module = builder.instantiate();
  let memory = module.exports.memory;
  let load = module.exports.load;
  let store = module.exports.store;

  assertEquals(num_bytes, memory.buffer.byteLength);
  // TODO(v8:4153): Enable for all sizes once the TypedArray size limit is
  // raised.
  const kMaxTypedArraySize = Math.pow(2, 32);
  if (num_bytes > kMaxTypedArraySize) {
    // TODO(v8:4153): Fix the error message below, if we don't decide to bump
    // the limit soon.
    assertThrows(
        () => new Int8Array(memory.buffer), RangeError,
        'Invalid typed array length: undefined');
  } else {
    let array = new Int8Array(memory.buffer);
    assertEquals(num_bytes, array.length);
  }

  assertEquals(0, load(num_bytes - 4));
  assertThrows(() => load(num_bytes - 3));

  store(num_bytes - 4, 0x12345678);
  assertEquals(0x12345678, load(num_bytes - 4));

  let kStoreOffset = 27;
  store(kStoreOffset, 11);
  assertEquals(11, load(kStoreOffset));

  // Now check 100 random positions.
  for (let i = 0; i < 100; ++i) {
    let position = Math.floor(Math.random() * num_bytes);
    let expected = 0;
    if (position == kStoreOffset) {
      expected = 11;
    } else if (num_bytes - position <= 4) {
      expected = [0x12, 0x34, 0x56, 0x78][num_bytes - position - 1];
    }
    let value = new Int8Array(memory.buffer, position, 1)[0];
    assertEquals(expected, value);
  }
}

function allowOOM(fn) {
  try {
    fn();
  } catch (e) {
    const is_oom =
        (e instanceof RangeError) && e.message.includes('Out of memory');
    if (!is_oom) throw e;
  }
}

(function TestSmallMemory() {
  print(arguments.callee.name);
  BasicMemory64Tests(4);
})();

(function Test3GBMemory() {
  print(arguments.callee.name);
  let num_pages = 3 * GB / kPageSize;
  // This test can fail if 3GB of memory cannot be allocated.
  allowOOM(() => BasicMemory64Tests(num_pages));
})();

(function Test5GBMemory() {
  print(arguments.callee.name);
  let num_pages = 5 * GB / kPageSize;
  // This test can fail if 5GB of memory cannot be allocated.
  allowOOM(() => BasicMemory64Tests(num_pages));
})();

(function TestMaxMem64Size() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(max_num_pages);

  assertTrue(WebAssembly.validate(builder.toBuffer()));
  builder.toModule();

  // This test can fail if 16GB of memory cannot be allocated.
  allowOOM(() => BasicMemory64Tests(max_num_pages));
})();

(function TestTooBigDeclaredInitial() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(max_num_pages + 1);

  assertFalse(WebAssembly.validate(builder.toBuffer()));
  assertThrows(
      () => builder.toModule(), WebAssembly.CompileError,
      'WebAssembly.Module(): initial memory size (262145 pages) is larger ' +
          'than implementation limit (262144 pages) @+12');
})();

(function TestTooBigDeclaredMaximum() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, max_num_pages + 1);

  assertFalse(WebAssembly.validate(builder.toBuffer()));
  assertThrows(
      () => builder.toModule(), WebAssembly.CompileError,
      'WebAssembly.Module(): maximum memory size (262145 pages) is larger ' +
          'than implementation limit (262144 pages) @+13');
})();

(function TestGrow64() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addMemory64(1, 10, false);

  builder.addFunction('grow', makeSig([kWasmI64], [kWasmI64]))
      .addBody([
        kExprLocalGet, 0,    // local.get 0
        kExprMemoryGrow, 0,  // memory.grow 0
      ])
      .exportFunc();

  let instance = builder.instantiate();

  assertEquals(1n, instance.exports.grow(2n));
  assertEquals(3n, instance.exports.grow(1n));
  assertEquals(-1n, instance.exports.grow(-1n));
  assertEquals(-1n, instance.exports.grow(1n << 31n));
  assertEquals(-1n, instance.exports.grow(1n << 32n));
  assertEquals(-1n, instance.exports.grow(1n << 33n));
  assertEquals(-1n, instance.exports.grow(1n << 63n));
  assertEquals(-1n, instance.exports.grow(7n));  // Above the of 10.
  assertEquals(4n, instance.exports.grow(6n));   // Just at the maximum of 10.
})();

(function TestBulkMemoryOperations() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  const kMemSizeInPages = 10;
  const kMemSize = kMemSizeInPages * kPageSize;
  builder.addMemory64(kMemSizeInPages, kMemSizeInPages);
  const kSegmentSize = 1024;
  // Build a data segment with values [0, kSegmentSize-1].
  const segment = Array.from({length: kSegmentSize}, (_, idx) => idx)
  builder.addPassiveDataSegment(segment);
  builder.exportMemoryAs('memory');

  builder.addFunction('fill', makeSig([kWasmI64, kWasmI32, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                   // local.get 0 (dst)
        kExprLocalGet, 1,                   // local.get 1 (value)
        kExprLocalGet, 2,                   // local.get 2 (size)
        kNumericPrefix, kExprMemoryFill, 0  // memory.fill mem=0
      ])
      .exportFunc();

  builder.addFunction('copy', makeSig([kWasmI64, kWasmI64, kWasmI64], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (src)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryCopy, 0, 0  // memory.copy srcmem=0 dstmem=0
      ])
      .exportFunc();

  builder.addFunction('init', makeSig([kWasmI64, kWasmI32, kWasmI32], []))
      .addBody([
        kExprLocalGet, 0,                      // local.get 0 (dst)
        kExprLocalGet, 1,                      // local.get 1 (offset)
        kExprLocalGet, 2,                      // local.get 2 (size)
        kNumericPrefix, kExprMemoryInit, 0, 0  // memory.init seg=0 mem=0
      ])
      .exportFunc();

  let instance = builder.instantiate();
  let fill = instance.exports.fill;
  let copy = instance.exports.copy;
  let init = instance.exports.init;
  // {memory(offset,size)} extracts the memory at [offset, offset+size)] into an
  // Array.
  let memory = (offset, size) => Array.from(new Uint8Array(
      instance.exports.memory.buffer.slice(offset, offset + size)));

  // Empty init (size=0).
  init(0n, 0, 0);
  assertEquals([0, 0], memory(0, 2));
  // Init memory[5..7] with [10..12].
  init(5n, 10, 3);
  assertEquals([0, 0, 10, 11, 12, 0, 0], memory(3, 7));
  // Init the end of memory ([kMemSize-2, kMemSize-1]) with [20, 21].
  init(BigInt(kMemSize-2), 20, 2);
  assertEquals([0, 0, 20, 21], memory(kMemSize - 4, 4));
  // Writing slightly OOB.
  assertTraps(kTrapMemOutOfBounds, () => init(BigInt(kMemSize-2), 20, 3));
  // Writing OOB, but the low 32-bit are in-bound.
  assertTraps(kTrapMemOutOfBounds, () => init(1n << 32n, 0, 0));
  // OOB even though size == 0.
  assertTraps(kTrapMemOutOfBounds, () => init(-1n, 0, 0));
  // More OOB.
  assertTraps(kTrapMemOutOfBounds, () => init(-1n, 0, 1));
  assertTraps(kTrapMemOutOfBounds, () => init(1n << 62n, 0, 1));
  assertTraps(kTrapMemOutOfBounds, () => init(1n << 63n, 0, 1));

  // Empty copy (size=0).
  copy(0n, 0n, 0n);
  // Copy memory[5..7] (containing [10..12]) to [3..5].
  copy(3n, 5n, 3n);
  assertEquals([0, 0, 0, 10, 11, 12, 11, 12, 0], memory(0, 9));
  // Copy to the end of memory ([kMemSize-2, kMemSize-1]).
  copy(BigInt(kMemSize-2), 3n, 2n);
  assertEquals([0, 0, 10, 11], memory(kMemSize - 4, 4));
  // Writing slightly OOB.
  assertTraps(kTrapMemOutOfBounds, () => copy(BigInt(kMemSize-2), 0n, 3n));
  // Writing OOB, but the low 32-bit are in-bound.
  assertTraps(kTrapMemOutOfBounds, () => copy(1n << 32n, 0n, 1n));
  assertTraps(kTrapMemOutOfBounds, () => copy(0n, 0n, 1n << 32n));
  // OOB even though size == 0.
  assertTraps(kTrapMemOutOfBounds, () => copy(-1n, 0n, 0n));
  // More OOB.
  assertTraps(kTrapMemOutOfBounds, () => copy(-1n, 0n, 1n));
  assertTraps(kTrapMemOutOfBounds, () => copy(1n << 62n, 0n, 1n));
  assertTraps(kTrapMemOutOfBounds, () => copy(1n << 63n, 0n, 1n));

  // Empty fill (size=0).
  fill(0n, 0, 0n);
  // Fill memory[15..17] with 3s.
  fill(15n, 3, 3n);
  assertEquals([0, 3, 3, 3, 0], memory(14, 5));
  // Fill the end of memory ([kMemSize-2, kMemSize-1]) with 7s.
  fill(BigInt(kMemSize-2), 7, 2n);
  assertEquals([0, 0, 7, 7], memory(kMemSize - 4, 4));
  // Writing slightly OOB.
  assertTraps(kTrapMemOutOfBounds, () => fill(BigInt(kMemSize-2), 0, 3n));
  // Writing OOB, but the low 32-bit are in-bound.
  assertTraps(kTrapMemOutOfBounds, () => fill(1n << 32n, 0, 1n));
  assertTraps(kTrapMemOutOfBounds, () => fill(0n, 0, 1n << 32n));
  // OOB even though size == 0.
  assertTraps(kTrapMemOutOfBounds, () => fill(-1n, 0, 0n));
  // More OOB.
  assertTraps(kTrapMemOutOfBounds, () => fill(-1n, 0, 1n));
  assertTraps(kTrapMemOutOfBounds, () => fill(1n << 62n, 0, 1n));
  assertTraps(kTrapMemOutOfBounds, () => fill(1n << 63n, 0, 1n));
})();
