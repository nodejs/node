// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --stress-compaction

load("test/mjsunit/wasm/wasm-module-builder.js");


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


// TODO(gdeepti): Generate tests programatically for all the sizes instead of
// current implementation.
function testMemoryGrowReadWrite32() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize - 3; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize - 3; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize - 3; offset <= 4*kPageSize + 4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 3; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowReadWrite32();

function testMemoryGrowReadWrite16() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize - 1; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -2; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize - 1; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize - 2; offset <= 4*kPageSize + 4; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 2; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 1; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowReadWrite16();

function testMemoryGrowReadWrite8() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize; offset <= 4*kPageSize + 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowReadWrite8();

function testMemoryGrowZeroInitialSize() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 3; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  offset = 3*kPageSize;
  for (var i = 1; i < 4; i++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertEquals(i, growMem(1));
  }
  poke(20);
  assertEquals(20, peek());
}

testMemoryGrowZeroInitialSize();

function testMemoryGrowZeroInitialSize32() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 3; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowZeroInitialSize32();

function testMemoryGrowZeroInitialSize16() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 2; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 1; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowZeroInitialSize16();

function testMemoryGrowZeroInitialSize8() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testMemoryGrowZeroInitialSize8();

function testMemoryGrowTrapMaxPagesZeroInitialMemory() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(-1, growMem(kV8MaxPages + 1));
}

testMemoryGrowTrapMaxPagesZeroInitialMemory();

function testMemoryGrowTrapMaxPages() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 1, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(-1, growMem(kV8MaxPages));
}

testMemoryGrowTrapMaxPages();

function testMemoryGrowTrapsWithNonSmiInput() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(0, undefined, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  // The parameter of grow_memory is unsigned. Therefore -1 stands for
  // UINT32_MIN, which cannot be represented as SMI.
  assertEquals(-1, growMem(-1));
};

testMemoryGrowTrapsWithNonSmiInput();

function testMemoryGrowCurrentMemory() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  builder.addFunction("memory_size", kSig_i_v)
      .addBody([kExprMemorySize, kMemoryZero])
      .exportFunc();
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  function MemSize() { return module.exports.memory_size(); }
  assertEquals(1, MemSize());
  assertEquals(1, growMem(1));
  assertEquals(2, MemSize());
}

testMemoryGrowCurrentMemory();

function testMemoryGrowPreservesDataMemOp32() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    poke(100000 - offset);
    assertEquals(100000 - offset, peek());
  }

  assertEquals(1, growMem(3));

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    assertEquals(100000 - offset, peek());
  }
}

testMemoryGrowPreservesDataMemOp32();

function testMemoryGrowPreservesDataMemOp16() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    poke(65535 - offset);
    assertEquals(65535 - offset, peek());
  }

  assertEquals(1, growMem(3));

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    assertEquals(65535 - offset, peek());
  }
}

testMemoryGrowPreservesDataMemOp16();

function testMemoryGrowPreservesDataMemOp8() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset, val = 0;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 1); offset++, val++) {
    poke(val);
    assertEquals(val, peek());
    if (val == 255) val = 0;
  }

  assertEquals(1, growMem(3));

  val = 0;

  for(offset = 0; offset <= (kPageSize - 1); offset++, val++) {
    assertEquals(val, peek());
    if (val == 255) val = 0;
  }
}

testMemoryGrowPreservesDataMemOp8();

function testMemoryGrowOutOfBoundsOffset() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  offset = 3*kPageSize + 4;
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(1, growMem(1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(2, growMem(1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(3, growMem(1));

  for (offset = 3*kPageSize; offset <= 4*kPageSize - 4; offset++) {
    poke(0xaced);
    assertEquals(0xaced, peek());
  }

  for (offset = 4*kPageSize - 3; offset <= 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
  }
}

testMemoryGrowOutOfBoundsOffset();

function testMemoryGrowOutOfBoundsOffset2() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(16, 128, false);
  builder.addFunction("main", kSig_v_v)
      .addBody([
          kExprI32Const, 20,
          kExprI32Const, 29,
          kExprMemoryGrow, kMemoryZero,
          kExprI32StoreMem, 0, 0xFF, 0xFF, 0xFF, 0x3a
          ])
      .exportAs("main");
  var module = builder.instantiate();
  assertTraps(kTrapMemOutOfBounds, module.exports.main);
}

testMemoryGrowOutOfBoundsOffset2();

function testMemoryGrowDeclaredMaxTraps() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, 16, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(1, growMem(5));
  assertEquals(6, growMem(5));
  assertEquals(-1, growMem(6));
}

testMemoryGrowDeclaredMaxTraps();

(function testMemoryGrowInternalMaxTraps() {
  // This test checks that grow_memory does not grow past the internally
  // defined maximum memory size.
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, kSpecMaxPages, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(1, growMem(20));
  assertEquals(-1, growMem(kV8MaxPages - 20));
})();

(function testMemoryGrow4Gb() {
  var builder = genMemoryGrowBuilder();
  builder.addMemory(1, undefined, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for (offset = 0; offset <= (kPageSize - 4); offset += 4) {
    poke(100000 - offset);
    assertEquals(100000 - offset, peek());
  }

  let result = growMem(kV8MaxPages - 1);
  if (result == 1) {
    for (offset = 0; offset <= (kPageSize - 4); offset += 4) {
      assertEquals(100000 - offset, peek());
    }

    // Bounds check for large mem size.
    let kMemSize = (kV8MaxPages * kPageSize);
    let kLastValidOffset = kMemSize - 4;  // Accommodate a 4-byte read/write.
    for (offset = kMemSize - kPageSize; offset <= kLastValidOffset;
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
})();
