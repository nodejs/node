// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jitless --wasm-jitless --allow-natives-syntax
// Flags: --drumbrake-register-optimization --drumbrake-super-instructions
// Flags: --wasm-max-mem-pages=32769
// Save some memory on Linux; other platforms ignore this flag.
// Flags: --multi-mapped-mock-allocator

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testMultipleMemoriesOfDifferentKinds() {
  print(arguments.callee.name);

  var builder = new WasmModuleBuilder();
  const mem64_idx = 0;
  builder.addMemory64(1, 1, false);
  const mem32_idx = 1;
  builder.addMemory(1, 1, false);

  builder.addFunction("main", kSig_v_v)
    .addBody([
      ...wasmI32Const(42),
      kExprI32LoadMem, 0x40, mem32_idx, 0,
      kExprDrop
    ])
    .exportAs("main");
  () => builder.instantiate();
})();

// This test is mostly copied from atomics-memory64.js, which is disabled in
// debug builds.
(function testMem64AtomicsWaitNotify() {
  print(arguments.callee.name);
  function CreateBigSharedWasmMemory64(num_pages) {
    let builder = new WasmModuleBuilder();
    builder.addMemory64(num_pages, num_pages, true);
    builder.exportMemoryAs('memory');
    return builder.instantiate().exports.memory;
  }

  function WasmAtomicNotify(memory, offset, index, num) {
    let builder = new WasmModuleBuilder();
    let pages = memory.buffer.byteLength / kPageSize;
    builder.addImportedMemory("m", "memory", pages, pages, "shared", "memory64");
    builder.addFunction("main", makeSig([kWasmF64, kWasmI32], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprI64SConvertF64,
        kExprLocalGet, 1,
        kAtomicPrefix, kExprAtomicNotify, /* alignment */ 2, ...wasmSignedLeb64(offset, 10)])
      .exportAs("main");

    let module = new WebAssembly.Module(builder.toBuffer());
    let instance = new WebAssembly.Instance(module, { m: { memory } });
    return instance.exports.main(index, num);
  }

  function WasmI32AtomicWait(memory, offset, index, val, timeout) {
    let builder = new WasmModuleBuilder();
    let pages = memory.buffer.byteLength / kPageSize;
    builder.addImportedMemory("m", "memory", pages, pages, "shared", "memory64");
    builder.addFunction("main",
      makeSig([kWasmF64, kWasmI32, kWasmF64], [kWasmI32]))
      .addBody([
        kExprLocalGet, 0,
        kExprI64SConvertF64,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprI64SConvertF64,
        kAtomicPrefix,
        kExprI32AtomicWait, /* alignment */ 2, ...wasmSignedLeb64(offset, 10)])
      .exportAs("main");

    let module = new WebAssembly.Module(builder.toBuffer());
    let instance = new WebAssembly.Instance(module, { m: { memory } });
    return instance.exports.main(index, val, timeout);
  }

  function WasmI64AtomicWait(memory, offset, index, val_low,
    val_high, timeout) {
    let builder = new WasmModuleBuilder();
    let pages = memory.buffer.byteLength / kPageSize;
    builder.addImportedMemory("m", "memory", pages, pages, "shared", "memory64");
    // Wrapper for I64AtomicWait that takes two I32 values and combines to into
    // I64 for the instruction parameter.
    builder.addFunction("main",
      makeSig([kWasmF64, kWasmI32, kWasmI32, kWasmF64], [kWasmI32]))
      .addLocals(kWasmI64, 1)
      .addBody([
        kExprLocalGet, 1,
        kExprI64UConvertI32,
        kExprI64Const, 32,
        kExprI64Shl,
        kExprLocalGet, 2,
        kExprI64UConvertI32,
        kExprI64Ior,
        kExprLocalSet, 4, // Store the created I64 value in local
        kExprLocalGet, 0,
        kExprI64SConvertF64,
        kExprLocalGet, 4,
        kExprLocalGet, 3,
        kExprI64SConvertF64,
        kAtomicPrefix,
        kExprI64AtomicWait, /* alignment */ 3, ...wasmSignedLeb64(offset, 10)])
      .exportAs("main");

    let module = new WebAssembly.Module(builder.toBuffer());
    let instance = new WebAssembly.Instance(module, { m: { memory } });
    return instance.exports.main(index, val_high, val_low, timeout);
  }

  function TestAtomicI32Wait(pages, offset) {
    if (!%IsAtomicsWaitAllowed()) return;

    let memory = CreateBigSharedWasmMemory64(pages);
    assertEquals(kAtomicWaitNotEqual, WasmI32AtomicWait(memory, offset, 0, 42, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI32AtomicWait(memory, offset, 0, 0, 0));
    assertEquals(kAtomicWaitNotEqual, WasmI32AtomicWait(memory, 0, offset, 42, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI32AtomicWait(memory, 0, offset, 0, 0));

    let i32a = new Int32Array(memory.buffer);
    i32a[offset / 4] = 1;

    assertEquals(kAtomicWaitNotEqual, WasmI32AtomicWait(memory, offset, 0, 0, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI32AtomicWait(memory, offset, 0, 1, 0));
    assertEquals(kAtomicWaitNotEqual, WasmI32AtomicWait(memory, 0, offset, 0, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI32AtomicWait(memory, 0, offset, 1, 0));

    assertEquals(0, WasmAtomicNotify(memory, offset, /*index*/4, 1));
  }

  function TestAtomicI64Wait(pages, offset) {
    if (!%IsAtomicsWaitAllowed()) return;

    let memory = CreateBigSharedWasmMemory64(pages);
    assertEquals(kAtomicWaitNotEqual, WasmI64AtomicWait(memory, offset, 0, 42, 0, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI64AtomicWait(memory, offset, 0, 0, 0, 0));
    assertEquals(kAtomicWaitNotEqual, WasmI64AtomicWait(memory, 0, offset, 42, 0, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI64AtomicWait(memory, 0, offset, 0, 0, 0));

    let i32a = new Int32Array(memory.buffer);
    i32a[offset / 4] = 1;
    i32a[(offset / 4) + 1] = 2;

    assertEquals(kAtomicWaitNotEqual, WasmI64AtomicWait(memory, offset, 0, 2, 1, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI64AtomicWait(memory, offset, 0, 1, 2, 0));
    assertEquals(kAtomicWaitNotEqual, WasmI64AtomicWait(memory, 0, offset, 2, 1, -1));
    assertEquals(kAtomicWaitTimedOut, WasmI64AtomicWait(memory, 0, offset, 1, 2, 0));

    assertEquals(0, WasmAtomicNotify(memory, offset, /*index*/8, 1));
  }

  const OFFSET = 4000;
  const MEM_PAGES = 100;
  (function () {
    TestAtomicI32Wait(MEM_PAGES, OFFSET);
    TestAtomicI64Wait(MEM_PAGES, OFFSET);
  })();
})();

(function testLoadStoreSuperInstruction() {
  print(arguments.callee.name);

  var builder = new WasmModuleBuilder();
  builder.addMemory64(1, 1, false);
  let addR2SFunction = function (builder, name, value, makeConstInstr, loadInstr,
    storeInstr, notEqInstr) {
    builder.addFunction(name, kSig_v_v)
      .addBody([
        ...wasmI64Const(1),
        ...makeConstInstr(value),
        storeInstr, 0, 0,

        ...wasmI64Const(2),
        ...wasmI64Const(0),
        ...wasmI64Const(1),
        kExprI64Add,
        loadInstr, 0, 0,
        storeInstr, 0, 0,

        ...wasmI64Const(2),
        loadInstr, 0, 0,
        ...makeConstInstr(value),
        notEqInstr,
        kExprIf, kWasmVoid,
          kExprUnreachable,
        kExprEnd,
      ])
      .exportAs(name);
  }
  addR2SFunction(builder, "test_i32_r2s_LoadStore", 42, wasmI32Const,
    kExprI32LoadMem, kExprI32StoreMem, kExprI32Ne);
  addR2SFunction(builder, "test_f32_r2s_LoadStore", 42.0, wasmF32Const,
    kExprF32LoadMem, kExprF32StoreMem, kExprF32Ne);
  addR2SFunction(builder, "test_i64_r2s_LoadStore", 42, wasmI64Const,
    kExprI64LoadMem, kExprI64StoreMem, kExprI64Ne);
  addR2SFunction(builder, "test_f64_r2s_LoadStore", 42.0, wasmF64Const,
    kExprF64LoadMem, kExprF64StoreMem, kExprF64Ne);

  let addS2SFunction = function (builder, name, value, makeConstInstr, loadInstr,
    storeInstr, notEqInstr) {
    builder.addFunction(name, kSig_v_v)
      .addBody([
        ...wasmI64Const(1),
        ...makeConstInstr(value),
        storeInstr, 0, 0,

        ...wasmI64Const(2),
        ...wasmI64Const(1),
        loadInstr, 0, 0,
        storeInstr, 0, 0,

        ...wasmI64Const(2),
        loadInstr, 0, 0,
        ...makeConstInstr(value),
        notEqInstr,
        kExprIf, kWasmVoid,
        kExprUnreachable,
        kExprEnd,
      ])
      .exportAs(name);
  }
  addS2SFunction(builder, "test_i32_s2s_LoadStore", 42, wasmI32Const,
    kExprI32LoadMem, kExprI32StoreMem, kExprI32Ne);
  addS2SFunction(builder, "test_f32_s2s_LoadStore", 42.0, wasmF32Const,
    kExprF32LoadMem, kExprF32StoreMem, kExprF32Ne);
  addS2SFunction(builder, "test_i64_s2s_LoadStore", 42, wasmI64Const,
    kExprI64LoadMem, kExprI64StoreMem, kExprI64Ne);
  addS2SFunction(builder, "test_f64_s2s_LoadStore", 42.0, wasmF64Const,
    kExprF64LoadMem, kExprF64StoreMem, kExprF64Ne);

  let instance = builder.instantiate({});
  instance.exports.test_i32_r2s_LoadStore();
  instance.exports.test_f32_r2s_LoadStore();
  instance.exports.test_i64_r2s_LoadStore();
  instance.exports.test_f64_r2s_LoadStore();
  instance.exports.test_i32_s2s_LoadStore();
  instance.exports.test_f32_s2s_LoadStore();
  instance.exports.test_i64_s2s_LoadStore();
  instance.exports.test_f64_s2s_LoadStore();
})();

(function testLoadStoreSuperInstructionIndexWidth() {
  const builder = new WasmModuleBuilder();
  builder.addMemory64(1, 1);
  builder.exportMemoryAs("memory");
  const instructions = [
    ["i32", kExprI32LoadMem, kExprI32StoreMem, 4],
    ["i64", kExprI64LoadMem, kExprI64StoreMem, 8],
    ["f32", kExprF32LoadMem, kExprF32StoreMem, 4],
    ["f64", kExprF64LoadMem, kExprF64StoreMem, 8],
  ];
  for (const [name, load, store] of instructions) {
    builder.addFunction(`copy_${name}`, makeSig([kWasmI64], []))
        .addBody([
          ...wasmI64Const(16),
          kExprLocalGet, 0,
          ...wasmI64Const(0),
          kExprI64Add,
          load, 0, 8,
          store, 0, 8,
        ])
        .exportFunc();
  }

  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  for (const [name, , , width] of instructions) {
    const payload = [1, 2, 3, 4, 5, 6, 7, 8].slice(0, width);
    new Uint8Array(buffer, 8, width).set(payload);
    const destination = new Uint8Array(buffer, 24, width);
    const copy = instance.exports[`copy_${name}`];
    copy(0n);
    assertEquals(payload, Array.from(destination));
    for (const index of [
           0x80000000n, 0x100000000n, 0x100000020n,
           0x8000000000000020n, -1n]) {
      destination.fill(0xa5);
      assertTraps(kTrapMemOutOfBounds, () => copy(index));
      assertEquals(new Array(width).fill(0xa5), Array.from(destination));
    }
  }
})();

(function testMemory32LoadStoreIndex() {
  const builder = new WasmModuleBuilder();
  const pages = 32769;  // One page beyond the signed i32 address range.
  builder.addMemory(pages, pages);
  builder.exportMemoryAs("memory");
  const instructions = [
    ["i32", kExprI32LoadMem, kExprI32StoreMem, 4],
    ["i64", kExprI64LoadMem, kExprI64StoreMem, 8],
    ["f32", kExprF32LoadMem, kExprF32StoreMem, 4],
    ["f64", kExprF64LoadMem, kExprF64StoreMem, 8],
  ];
  for (const [name, load, store] of instructions) {
    for (const offset of [0, 8]) {
      builder.addFunction(`copy_${name}_${offset}`, kSig_v_i)
          .addBody([
            ...wasmI32Const(16),
            kExprLocalGet, 0,
            ...wasmI32Const(0),
            // Signed shift by zero preserves the index and sign-extends r0.
            kExprI32ShrS,
            load, 0, offset,
            store, 0, 8,
          ])
          .exportFunc();
    }
  }

  const instance = builder.instantiate();
  const buffer = instance.exports.memory.buffer;
  for (const [name, , , width] of instructions) {
    const payload = [1, 2, 3, 4, 5, 6, 7, 8].slice(0, width);
    const destination = new Uint8Array(buffer, 24, width);
    for (const offset of [0, 8]) {
      const copy = instance.exports[`copy_${name}_${offset}`];
      for (const index of [
             0x7ffffff8, 0x80000000, 0x80000008,
             buffer.byteLength - width - offset]) {
        new Uint8Array(buffer, index + offset, width).set(payload);
        destination.fill(0);
        copy(index);
        assertEquals(payload, Array.from(destination));
      }
      destination.fill(0xa5);
      assertTraps(kTrapMemOutOfBounds,
                  () => copy(buffer.byteLength - width - offset + 1));
      assertEquals(new Array(width).fill(0xa5), Array.from(destination));
    }
  }
})();
