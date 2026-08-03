// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-compaction --experimental-wasm-rab-integration
// This test does not behave predictably, since growing memory is allowed to
// fail nondeterministically.
// Flags: --no-verify-predictable

// This test is a copy of grow-memory, except the growing is done after the
// buffer is exposed as a resizable buffer.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function genMemoryGrowBuilder() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("grow_memory", kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprMemoryGrow, kMemoryZero])
      .exportFunc();
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32LoadMem, 0, 0])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32StoreMem, 0, 0,
                kExprLocalGet, 1])
      .exportFunc();
  builder.addFunction("load16", kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32LoadMem16U, 0, 0])
      .exportFunc();
  builder.addFunction("store16", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32StoreMem16, 0, 0,
                kExprLocalGet, 1])
      .exportFunc();
  builder.addFunction("load8", kSig_i_i)
      .addBody([kExprLocalGet, 0, kExprI32LoadMem8U, 0, 0])
      .exportFunc();
  builder.addFunction("store8", kSig_i_ii)
      .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32StoreMem8, 0, 0,
                kExprLocalGet, 1])
      .exportFunc();
  return builder;
}

// V8 internal memory size limit.
var kV8MaxPages = 65536;


function testMemoryGrowReadWriteBase(size, load_fn, store_fn, growMem) {
  // size is the number of bytes for load and stores.
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 100);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset;
  var load = module.exports[load_fn];
  var store = module.exports[store_fn];
  function peek() { return load(offset); }
  function poke(value) { return store(offset, value); }

  // Instead of checking every n-th offset, check the first 5.
  for(offset = 0; offset <= (4*size); offset+=size) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize - (size - 1); offset < kPageSize + size; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(module, 3));

  for (let n = 1; n <= 3; n++) {
    for (offset = n * kPageSize - 5 * size; offset <= n * kPageSize + 4 * size;
         offset += size) {
      // Check the 5 offsets to the before and after the n-th page.
      //    page n-1              page n          page n+1
      //    +---- ... ------------+---------- ... +------ ...
      //    | | | ... | | | | | | | | | | | | ... | | | | ...
      //      <+>       ^                 ^
      //       |        first offset      last offset
      //       +-> size bytes
      poke(20);
      assertEquals(20, peek());
    }
  }

  // Check the last 5 valid offsets of the last page.
  for (offset = 4*kPageSize-size-(4*size); offset <= 4*kPageSize -size; offset+=size) {
    poke(20);
    assertEquals(20, peek());
  }

  for (offset = 4*kPageSize - (size-1); offset < 4*kPageSize + size; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(module, 15));

  for (offset = 4*kPageSize - (size-1); offset <= 4*kPageSize + size; offset+=size) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - size; offset+=size) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - (size-1); offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

function growMemViaExport(module, pages) {
  module.exports.memory.toResizableBuffer();
  return module.exports.grow_memory(pages);
}

function growMemViaRAB(module, pages) {
  let buf = module.exports.memory.toResizableBuffer();
  let oldLength = buf.byteLength;
  try {
    buf.resize(buf.byteLength + pages * kPageSize);
  } catch (e) {
    if (e instanceof RangeError) return -1;
  }
  return oldLength / kPageSize;
}

(function testMemoryGrowReadWrite32() {
  print(arguments.callee.name);
  testMemoryGrowReadWriteBase(4, "load", "store", growMemViaExport);
  testMemoryGrowReadWriteBase(4, "load", "store", growMemViaRAB);
})();

(function testMemoryGrowReadWrite16() {
  print(arguments.callee.name);
  testMemoryGrowReadWriteBase(2, "load16", "store16", growMemViaExport);
  testMemoryGrowReadWriteBase(2, "load16", "store16", growMemViaRAB);
})();

(function testMemoryGrowReadWrite8() {
  print(arguments.callee.name);
  testMemoryGrowReadWriteBase(1, "load8", "store8", growMemViaExport);
  testMemoryGrowReadWriteBase(1, "load8", "store8", growMemViaRAB);
})();

function testMemoryGrowZeroInitialSize(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, 100);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(module, 1));

  // Check first 5 offsets.
  for(offset = 0; offset <= 5; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  // Check last 5 offsets.
  for(offset = kPageSize - 5*4; offset <= kPageSize - 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 3; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  offset = 3*kPageSize;
  for (var i = 1; i < 4; i++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertEquals(i, growMem(module, 1));
  }
  poke(20);
  assertEquals(20, peek());
};
testMemoryGrowZeroInitialSize(growMemViaExport);
testMemoryGrowZeroInitialSize(growMemViaRAB);

function testMemoryGrowZeroInitialSizeBase(size, load_fn, store_fn, growMem) {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, 100);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset;
  var load = module.exports[load_fn];
  var store = module.exports[store_fn];
  function peek() { return load(offset); }
  function poke(value) { return store(offset, value); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(module, 1));

  // Instead of checking every offset, check the first 5.
  for(offset = 0; offset <= 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  // Check the last 5 valid ones.
  for(offset = kPageSize - (size * 4); offset <= kPageSize - size; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - (size - 1); offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

(function testMemoryGrowZeroInitialSize32() {
  print(arguments.callee.name);
  testMemoryGrowZeroInitialSizeBase(4, "load", "store", growMemViaExport);
  testMemoryGrowZeroInitialSizeBase(4, "load", "store", growMemViaRAB);
})();

(function testMemoryGrowZeroInitialSize16() {
  print(arguments.callee.name);
  testMemoryGrowZeroInitialSizeBase(2, "load16", "store16", growMemViaExport);
  testMemoryGrowZeroInitialSizeBase(2, "load16", "store16", growMemViaRAB);
})();

(function testMemoryGrowZeroInitialSize8() {
  print(arguments.callee.name);
  testMemoryGrowZeroInitialSizeBase(1, "load8", "store8", growMemViaExport);
  testMemoryGrowZeroInitialSizeBase(1, "load8", "store8", growMemViaRAB);
})();

function testMemoryGrowTrapMaxPages(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 1);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  assertEquals(-1, growMem(module, kV8MaxPages));
};
testMemoryGrowTrapMaxPages(growMemViaExport);
testMemoryGrowTrapMaxPages(growMemViaRAB);


function testMemoryGrowCurrentMemory(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 100);
  builder.exportMemoryAs("memory");
  builder.addFunction("memory_size", kSig_i_v)
      .addBody([kExprMemorySize, kMemoryZero])
      .exportFunc();
  var module = builder.instantiate();
  function MemSize() { return module.exports.memory_size(); }
  assertEquals(1, MemSize());
  assertEquals(1, growMem(module, 1));
  assertEquals(2, MemSize());
}
testMemoryGrowCurrentMemory(growMemViaExport);
testMemoryGrowCurrentMemory(growMemViaRAB);

function testMemoryGrowPreservesDataMemOpBase(size, load_fn, store_fn, growMem) {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 100);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset;
  var load = module.exports[load_fn];
  var store = module.exports[store_fn];
  function peek() { return load(offset); }
  function poke(value) { return store(offset, value); }

  // Maximum unsigned integer of size bits.
  const max = Math.pow(2, (size * 8)) - 1;

  // Check the first 5 offsets.
  for(offset = 0; offset <= (4*size); offset+=size) {
    poke(offset % max);
    assertEquals(offset % max, peek());
  }

  // Check the last 5 valid offsets.
  for(offset = kPageSize - 5*size; offset <= (kPageSize - size); offset+=size) {
    poke(offset % max);
    assertEquals(offset % max, peek());
  }

  assertEquals(1, growMem(module, 3));

  // Check the first 5 offsets are preserved by growMem.
  for(offset = 0; offset <= (4*size); offset+=size) {
    assertEquals(offset % max, peek());
  }

  // Check the last 5 valid offsets are preserved by growMem.
  for(offset = kPageSize - 5*size; offset <= (kPageSize - size); offset+=size) {
    assertEquals(offset % max, peek());
  }
}

(function testMemoryGrowPreservesDataMemOp32() {
  print(arguments.callee.name);
  testMemoryGrowPreservesDataMemOpBase(4, "load", "store", growMemViaExport);
  testMemoryGrowPreservesDataMemOpBase(4, "load", "store", growMemViaRAB);
})();

(function testMemoryGrowPreservesDataMemOp16() {
  print(arguments.callee.name);
  testMemoryGrowPreservesDataMemOpBase(2, "load16", "store16", growMemViaExport);
  testMemoryGrowPreservesDataMemOpBase(2, "load16", "store16", growMemViaRAB);
})();

(function testMemoryGrowPreservesDataMemOp8() {
  print(arguments.callee.name);
  testMemoryGrowPreservesDataMemOpBase(1, "load8", "store8", growMemViaExport);
  testMemoryGrowPreservesDataMemOpBase(1, "load8", "store8", growMemViaRAB);
})();

function testMemoryGrowOutOfBoundsOffset(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 100);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }

  offset = 3*kPageSize + 4;
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(1, growMem(module, 1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(2, growMem(module, 1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(3, growMem(module, 1));

  for (offset = 3*kPageSize; offset <= 3*kPageSize + 4; offset++) {
    poke(0xaced);
    assertEquals(0xaced, peek());
  }

  for (offset = 4*kPageSize-8; offset <= 4*kPageSize - 4; offset++) {
    poke(0xaced);
    assertEquals(0xaced, peek());
  }

  for (offset = 4*kPageSize - 3; offset <= 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
  }
}
testMemoryGrowOutOfBoundsOffset(growMemViaExport);
testMemoryGrowOutOfBoundsOffset(growMemViaRAB);

function testMemoryGrowDeclaredMaxTraps(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 16);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  assertEquals(1, growMem(module, 5));
  assertEquals(6, growMem(module, 5));
  assertEquals(-1, growMem(module, 6));
}
testMemoryGrowDeclaredMaxTraps(growMemViaExport);
testMemoryGrowDeclaredMaxTraps(growMemViaRAB);

function testMemoryGrowInternalMaxTraps(growMem) {
  print(arguments.callee.name);
  // This test checks that grow_memory does not grow past the internally
  // defined maximum memory size.
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, kSpecMaxPages);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  assertEquals(1, growMem(module, 20));
  assertEquals(-1, growMem(module, kV8MaxPages - 20));
}
testMemoryGrowInternalMaxTraps(growMemViaExport);
testMemoryGrowInternalMaxTraps(growMemViaRAB);

function testMemoryGrow4Gb(growMem) {
  print(arguments.callee.name);
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, kV8MaxPages);
  builder.exportMemoryAs("memory");
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }

  // Check first 5 offsets.
  for (offset = 0; offset <= 4 * 4; offset += 4) {
    poke(100000 - offset);
    assertEquals(100000 - offset, peek());
  }

  // Check last 5 offsets.
  for (offset = (kPageSize - 5 * 4); offset <= (kPageSize - 4); offset += 4) {
    poke(100000 - offset);
    assertEquals(100000 - offset, peek());
  }

  let result = growMem(module, kV8MaxPages - 1);
  if (result == 1) {
    // Check first 5 offsets.
    for (offset = 0; offset <= 4 * 4; offset += 4) {
      assertEquals(100000 - offset, peek());
    }

    // Check last 5 offsets.
    for (offset = (kPageSize - 5 * 4); offset <= (kPageSize - 4); offset += 4) {
      assertEquals(100000 - offset, peek());
    }

    // Bounds check for large mem size.
    let kMemSize = (kV8MaxPages * kPageSize);
    let kLastValidOffset = kMemSize - 4;  // Accommodate a 4-byte read/write.
    // Check first 5 offsets of last page.
    for (offset = kMemSize - kPageSize; offset <= kMemSize - kPageSize + 4 * 4;
         offset += 4) {
      poke(0xaced);
      assertEquals(0xaced, peek());
    }
    for (offset = kLastValidOffset - 5 * 4; offset <= kLastValidOffset;
         offset += 4) {
      poke(0xaced);
      assertEquals(0xaced, peek());
    }

    for (offset = kLastValidOffset + 1; offset < kMemSize; offset++) {
      assertTraps(kTrapMemOutOfBounds, poke);
    }
  } else {
    // Allocating big chunks of memory can fail on gc_stress, especially on 32
    // bit platforms. When grow_memory fails, expected result is -1.
    assertEquals(-1, result);
  }
}
testMemoryGrow4Gb(growMemViaExport);
testMemoryGrow4Gb(growMemViaRAB);

function testGrowFromNearlyMaximum(growMem) {
  print(arguments.callee.name);
  // Regression test for https://crbug.com/1347668.
  const builder = genMemoryGrowBuilder();
  // The maximum needs to be >1GB, so we do not reserve everything upfront.
  const GB = 1024 * 1024 * 1024;
  const max_pages = 1 * GB / kPageSize + 10;

  builder.addMemory(0, max_pages);
  builder.exportMemoryAs("memory");
  let module;
  const is_oom = e =>
      (e instanceof RangeError) && e.message.includes('Out of memory');
  // Allow instantiation to fail with OOM.
  try {
    module = builder.instantiate();
  } catch (e) {
    if (is_oom(e)) return;
    // Everything else is a bug.
    throw e;
  }

  // First, grow close to the limit.
  // Growing can always fail if the system runs out of resources.
  let grow_result = growMem(module, max_pages - 1);
  if (grow_result == -1) return;
  assertEquals(0, grow_result);

  // Then, grow by another page (this triggered the error in
  // https://crbug.com/1347668).
  grow_result = growMem(module, 1);
  if (grow_result == -1) return;
  assertEquals(max_pages - 1, grow_result);
  assertEquals(max_pages, growMem(module, 0));
  assertEquals(-1, growMem(module, 1));  // Fails.
}
testGrowFromNearlyMaximum(growMemViaExport);
testGrowFromNearlyMaximum(growMemViaRAB);
