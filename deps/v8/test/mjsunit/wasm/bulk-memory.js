// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-bulk-memory

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestPassiveDataSegment() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1, 1, false);
  builder.addPassiveDataSegment([0, 1, 2]);
  builder.addPassiveDataSegment([3, 4]);

  // Should not throw.
  builder.instantiate();
})();

(function TestPassiveElementSegment() {
  const builder = new WasmModuleBuilder();
  builder.addFunction('f', kSig_v_v).addBody([]);
  builder.setTableBounds(1, 1);
  builder.addPassiveElementSegment([0, 0, 0]);
  builder.addPassiveElementSegment([0, 0]);

  // Should not throw.
  builder.instantiate();
})();

function getMemoryInit(mem, segment_data) {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("", "mem", 0);
  builder.addPassiveDataSegment(segment_data);
  builder.addFunction('init', kSig_v_iii)
      .addBody([
        kExprGetLocal, 0,  // Dest.
        kExprGetLocal, 1,  // Source.
        kExprGetLocal, 2,  // Size in bytes.
        kNumericPrefix, kExprMemoryInit,
        0,  // Data segment index.
        0,  // Memory index.
      ])
      .exportAs('init');
  return builder.instantiate({'': {mem}}).exports.init;
}

(function TestMemoryInitOutOfBoundsGrow() {
  const mem = new WebAssembly.Memory({initial: 1});
  // Create a data segment that has a length of kPageSize.
  const memoryInit = getMemoryInit(mem, new Array(kPageSize));

  mem.grow(1);

  // Works properly after grow.
  memoryInit(kPageSize, 0, 1000);

  // Traps at new boundary.
  assertTraps(
      kTrapMemOutOfBounds, () => memoryInit(kPageSize + 1, 0, kPageSize));
})();

(function TestMemoryInitOnActiveSegment() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.addPassiveDataSegment([1, 2, 3]);
  builder.addDataSegment(0, [4, 5, 6]);
  builder.addFunction('init', kSig_v_v)
      .addBody([
        kExprI32Const, 0,  // Dest.
        kExprI32Const, 0,  // Source.
        kExprI32Const, 0,  // Size in bytes.
        kNumericPrefix, kExprMemoryInit,
        1,  // Data segment index.
        0,  // Memory index.
      ])
      .exportAs('init');

  // Instantiation succeeds, because using memory.init with an active segment
  // is a trap, not a validation error.
  const instance = builder.instantiate();

  assertTraps(kTrapDataSegmentDropped, () => instance.exports.init());
})();

(function TestDataDropOnActiveSegment() {
  const builder = new WasmModuleBuilder();
  builder.addMemory(1);
  builder.addPassiveDataSegment([1, 2, 3]);
  builder.addDataSegment(0, [4, 5, 6]);
  builder.addFunction('drop', kSig_v_v)
      .addBody([
        kNumericPrefix, kExprDataDrop,
        1,  // Data segment index.
      ])
      .exportAs('drop');

  const instance = builder.instantiate();
  assertTraps(kTrapDataSegmentDropped, () => instance.exports.drop());
})();

function getMemoryCopy(mem) {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("", "mem", 0);
  builder.addFunction("copy", kSig_v_iii).addBody([
    kExprGetLocal, 0,  // Dest.
    kExprGetLocal, 1,  // Source.
    kExprGetLocal, 2,  // Size in bytes.
    kNumericPrefix, kExprMemoryCopy, 0, 0,
  ]).exportAs("copy");
  return builder.instantiate({'': {mem}}).exports.copy;
}

(function TestMemoryCopyOutOfBoundsGrow() {
  const mem = new WebAssembly.Memory({initial: 1});
  const memoryCopy = getMemoryCopy(mem);

  mem.grow(1);

  // Works properly after grow.
  memoryCopy(0, kPageSize, 1000);

  // Traps at new boundary.
  assertTraps(
      kTrapMemOutOfBounds, () => memoryCopy(0, kPageSize + 1, kPageSize));
})();

function getMemoryFill(mem) {
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory("", "mem", 0);
  builder.addFunction("fill", kSig_v_iii).addBody([
    kExprGetLocal, 0,  // Dest.
    kExprGetLocal, 1,  // Byte value.
    kExprGetLocal, 2,  // Size.
    kNumericPrefix, kExprMemoryFill, 0,
  ]).exportAs("fill");
  return builder.instantiate({'': {mem}}).exports.fill;
}

(function TestMemoryFillOutOfBoundsGrow() {
  const mem = new WebAssembly.Memory({initial: 1});
  const memoryFill = getMemoryFill(mem);
  const v = 123;

  mem.grow(1);

  // Works properly after grow.
  memoryFill(kPageSize, v, 1000);

  // Traps at new boundary.
  assertTraps(
      kTrapMemOutOfBounds, () => memoryFill(kPageSize + 1, v, kPageSize));
})();

(function TestElemDropActive() {
  const builder = new WasmModuleBuilder();
  builder.setTableBounds(5, 5);
  builder.addElementSegment(0, 0, false, [0, 0, 0]);
  builder.addFunction('drop', kSig_v_v)
      .addBody([
        kNumericPrefix, kExprElemDrop,
        0,  // Element segment index.
      ])
      .exportAs('drop');

  const instance = builder.instantiate();
  assertTraps(kTrapElemSegmentDropped, () => instance.exports.drop());
})();

(function TestLazyDataSegmentBoundsCheck() {
  const memory = new WebAssembly.Memory({initial: 1});
  const view = new Uint8Array(memory.buffer);
  const builder = new WasmModuleBuilder();
  builder.addImportedMemory('m', 'memory', 1);
  builder.addDataSegment(kPageSize - 1, [42, 42]);
  builder.addDataSegment(0, [111, 111]);

  assertEquals(0, view[kPageSize - 1]);

  // Instantiation fails, but still modifies memory.
  assertThrows(() => builder.instantiate({m: {memory}}), WebAssembly.LinkError);

  assertEquals(42, view[kPageSize - 1]);
  // The second segment is not initialized.
  assertEquals(0, view[0]);
})();

(function TestLazyElementSegmentBoundsCheck() {
  const table = new WebAssembly.Table({initial: 3, element: 'anyfunc'});
  const builder = new WasmModuleBuilder();
  builder.addImportedTable('m', 'table', 1);
  const f = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 42]);

  const tableIndex = 0;
  const isGlobal = false;
  builder.addElementSegment(tableIndex, 2, isGlobal, [f.index, f.index]);
  builder.addElementSegment(tableIndex, 0, isGlobal, [f.index, f.index]);

  assertEquals(null, table.get(0));
  assertEquals(null, table.get(1));
  assertEquals(null, table.get(2));

  // Instantiation fails, but still modifies the table.
  assertThrows(() => builder.instantiate({m: {table}}), WebAssembly.LinkError);

  // The second segment is not initialized.
  assertEquals(null, table.get(0));
  assertEquals(null, table.get(1));
  assertEquals(42, table.get(2)());
})();

(function TestLazyDataAndElementSegments() {
  const table = new WebAssembly.Table({initial: 1, element: 'anyfunc'});
  const memory = new WebAssembly.Memory({initial: 1});
  const view = new Uint8Array(memory.buffer);
  const builder = new WasmModuleBuilder();

  builder.addImportedMemory('m', 'memory', 1);
  builder.addImportedTable('m', 'table', 1);
  const f = builder.addFunction('f', kSig_i_v).addBody([kExprI32Const, 42]);

  const tableIndex = 0;
  const isGlobal = false;
  builder.addElementSegment(tableIndex, 0, isGlobal, [f.index, f.index]);
  builder.addDataSegment(0, [42]);

  // Instantiation fails, but still modifies the table. The memory is not
  // modified, since data segments are initialized after element segments.
  assertThrows(
      () => builder.instantiate({m: {memory, table}}), WebAssembly.LinkError);

  assertEquals(42, table.get(0)());
  assertEquals(0, view[0]);
})();
