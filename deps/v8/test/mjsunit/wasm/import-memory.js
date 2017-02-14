// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestOne() {
  print("TestOne");
  let memory = new WebAssembly.Memory({initial: 1});
  assertEquals(kPageSize, memory.buffer.byteLength);
  let i32 = new Int32Array(memory.buffer);
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mine");
  builder.addFunction("main", kSig_i_v)
    .addBody([
      kExprI32Const, 0,
      kExprI32LoadMem, 0, 0])
    .exportAs("main");

  let main = builder.instantiate({mine: memory}).exports.main;
  assertEquals(0, main());

  i32[0] = 993377;

  assertEquals(993377, main());
})();

(function TestIdentity() {
  print("TestIdentity");
  let memory = new WebAssembly.Memory({initial: 1});
  let i32 = new Int32Array(memory.buffer);
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("garg");
  builder.exportMemoryAs("daggle");

  let instance = builder.instantiate({garg: memory});
  assertSame(memory, instance.exports.daggle);
})();


(function TestImportExport() {
  print("TestImportExport");
  var i1;
  {
    let builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, false);
    builder.exportMemoryAs("exported_mem");
    builder.addFunction("foo", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32LoadMem, 0, 0])
      .exportAs("foo");
    i1 = builder.instantiate();
  }

  var i2;
  {
    let builder = new WasmModuleBuilder();
    builder.addMemory(1, 1, false);
    builder.addImportedMemory("imported_mem");
    builder.addFunction("bar", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32LoadMem, 0, 0])
      .exportAs("bar");
    i2 = builder.instantiate({imported_mem: i1.exports.exported_mem});
  }

  let i32 = new Int32Array(i1.exports.exported_mem.buffer);

  for (var i = 0; i < 1e11; i = i * 3 + 5) {
    for (var j = 0; j < 10; j++) {
      var val = i + 99077 + j;
      i32[j] = val;
      assertEquals(val | 0, i1.exports.foo(j * 4));
      assertEquals(val | 0, i2.exports.bar(j * 4));
    }
  }
})();

(function ValidateBoundsCheck() {
  print("ValidateBoundsCheck");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 5});
  assertEquals(kPageSize, memory.buffer.byteLength);
  let i32 = new Int32Array(memory.buffer);
  let builder = new WasmModuleBuilder();
  // builder.addImportedMemory("mine");
  builder.addImportedMemory("mine");
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  var offset;
  let instance = builder.instantiate({mine: memory});
  function load() { return instance.exports.load(offset); }
  function store(value) { return instance.exports.store(offset, value); }

  for (offset = 0; offset < kPageSize - 3; offset+=4) {
    store(offset);
  }
  for (offset = 0; offset < kPageSize - 3; offset+=4) {
    assertEquals(offset, load());
  }
  for (offset = kPageSize - 3; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, load);
  }
})();

(function TestGrowMemoryMaxDesc() {
  print("MaximumDescriptor");
  let memory = new WebAssembly.Memory({initial: 1, maximum: 5});
  assertEquals(kPageSize, memory.buffer.byteLength);
  let i32 = new Int32Array(memory.buffer);
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mine", "", 0, 20);
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  var offset;
  let instance = builder.instantiate({mine: memory});
  function load() { return instance.exports.load(offset); }
  function store(value) { return instance.exports.store(offset, value); }

  for (var i = 1; i < 5; i++) {
    for (offset = (i - 1) * kPageSize; offset < i * kPageSize - 3; offset+=4) {
      store(offset * 2);
    }
    assertEquals(i, memory.grow(1));
    assertEquals((i + 1) * kPageSize, memory.buffer.byteLength);
  }
  for (offset = 4 * kPageSize; offset < 5 * kPageSize - 3; offset+=4) {
    store(offset * 2);
  }
  for (offset = 0; offset < 5 * kPageSize - 3; offset+=4) {
    assertEquals(offset * 2, load());
  }
  for (offset = 5 * kPageSize; offset < 5 * kPageSize + 4; offset++) {
    assertThrows(load);
  }
  assertThrows(() => memory.grow(1));
})();

(function TestGrowMemoryZeroInitialMemory() {
  print("ZeroInitialMemory");
  let memory = new WebAssembly.Memory({initial: 0});
  assertEquals(0, memory.buffer.byteLength);
  let i32 = new Int32Array(memory.buffer);
  let builder = new WasmModuleBuilder();
  builder.addImportedMemory("mine");
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  var offset;
  let instance = builder.instantiate({mine: memory});
  function load() { return instance.exports.load(offset); }
  function store(value) { return instance.exports.store(offset, value); }

  for (var i = 1; i < 5; i++) {
    assertEquals(i - 1, memory.grow(1));
    assertEquals(i * kPageSize, memory.buffer.byteLength);
    for (offset = (i - 1) * kPageSize; offset < i * kPageSize - 3; offset++) {
      store(offset * 2);
    }
  }
  for (offset = 5 * kPageSize; offset < 5 * kPageSize + 4; offset++) {
    assertThrows(load);
  }
  assertThrows(() => memory.grow(16381));
})();

(function ImportedMemoryBufferLength() {
  print("ImportedMemoryBufferLength");
  let memory = new WebAssembly.Memory({initial: 2, maximum: 10});
  assertEquals(2*kPageSize, memory.buffer.byteLength);
  let builder = new WasmModuleBuilder();
  builder.addFunction("grow", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprGrowMemory, kMemoryZero])
      .exportFunc();
  builder.addImportedMemory("mine");
  let instance = builder.instantiate({mine: memory});
  function grow(pages) { return instance.exports.grow(pages); }
  assertEquals(2, grow(3));
  assertEquals(5*kPageSize, memory.buffer.byteLength);
  assertEquals(5, grow(5));
  assertEquals(10*kPageSize, memory.buffer.byteLength);
  assertThrows(() => memory.grow(1));
})();

(function TestGrowMemoryExportedMaximum() {
  print("TestGrowMemoryExportedMaximum");
  let initial_size = 1, maximum_size = 10;
  var exp_instance;
  {
    let builder = new WasmModuleBuilder();
    builder.addMemory(initial_size, maximum_size, true);
    builder.exportMemoryAs("exported_mem");
    exp_instance = builder.instantiate();
  }
  var instance;
  {
    var builder = new WasmModuleBuilder();
    builder.addImportedMemory("imported_mem");
    builder.addFunction("mem_size", kSig_i_v)
      .addBody([kExprMemorySize, kMemoryZero])
      .exportFunc();
    builder.addFunction("grow", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprGrowMemory, kMemoryZero])
      .exportFunc();
    instance = builder.instantiate({
      imported_mem: exp_instance.exports.exported_mem});
  }
  for (var i = initial_size; i < maximum_size; i++) {
    assertEquals(i, instance.exports.grow(1));
    assertEquals((i+1), instance.exports.mem_size());
  }
  assertEquals(-1, instance.exports.grow(1));
})();
