// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --stress-compaction

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var kMemSize = 65536;

function genModule(memory) {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  builder.addFunction("main", kSig_i_i)
    .addBody([
      // main body: while(i) { if(mem[i]) return -1; i -= 4; } return 0;
      // TODO(titzer): this manual bytecode has a copy of test-run-wasm.cc
      /**/ kExprLoop, kAstStmt,            // --
      /*  */ kExprGetLocal, 0,             // --
      /*  */ kExprIf, kAstStmt,            // --
      /*    */ kExprGetLocal, 0,           // --
      /*    */ kExprI32LoadMem, 0, 0,      // --
      /*    */ kExprIf, kAstStmt,          // --
      /*      */ kExprI8Const, 255,        // --
      /*      */ kExprReturn,              // --
      /*      */ kExprEnd,                 // --
      /*    */ kExprGetLocal, 0,           // --
      /*    */ kExprI8Const, 4,            // --
      /*    */ kExprI32Sub,                // --
      /*    */ kExprSetLocal, 0,           // --
      /*    */ kExprBr, 1,                 // --
      /*    */ kExprEnd,                   // --
      /*  */ kExprEnd,                     // --
      /**/ kExprI8Const, 0                 // --
    ])
    .exportFunc();
  var module = builder.instantiate(null, memory);
  assertTrue(module.exports.memory instanceof WebAssembly.Memory);
  if (memory != null) assertEquals(memory, module.exports.memory.buffer);
  return module;
}

function testPokeMemory() {
  var module = genModule(null);
  var buffer = module.exports.memory.buffer;
  var main = module.exports.main;
  assertEquals(kMemSize, buffer.byteLength);

  var array = new Int8Array(buffer);
  assertEquals(kMemSize, array.length);

  for (var i = 0; i < kMemSize; i++) {
    assertEquals(0, array[i]);
  }

  for (var i = 0; i < 10; i++) {
    assertEquals(0, main(kMemSize - 4));

    array[kMemSize/2 + i] = 1;
    assertEquals(0, main(kMemSize/2 - 4));
    assertEquals(-1, main(kMemSize - 4));

    array[kMemSize/2 + i] = 0;
    assertEquals(0, main(kMemSize - 4));
  }
}

testPokeMemory();

function genAndGetMain(buffer) {
  return genModule(buffer).exports.main;  // to prevent intermediates living
}

function testSurvivalAcrossGc() {
  var checker = genAndGetMain(null);
  for (var i = 0; i < 3; i++) {
    print("gc run ", i);
    assertEquals(0, checker(kMemSize - 4));
    gc();
  }
}

testSurvivalAcrossGc();
testSurvivalAcrossGc();
testSurvivalAcrossGc();
testSurvivalAcrossGc();


function testPokeOuterMemory() {
  var buffer = new ArrayBuffer(kMemSize);
  var module = genModule(buffer);
  var main = module.exports.main;
  assertEquals(kMemSize, buffer.byteLength);

  var array = new Int8Array(buffer);
  assertEquals(kMemSize, array.length);

  for (var i = 0; i < kMemSize; i++) {
    assertEquals(0, array[i]);
  }

  for (var i = 0; i < 10; i++) {
    assertEquals(0, main(kMemSize - 4));

    array[kMemSize/2 + i] = 1;
    assertEquals(0, main(kMemSize/2 - 4));
    assertEquals(-1, main(kMemSize - 4));

    array[kMemSize/2 + i] = 0;
    assertEquals(0, main(kMemSize - 4));
  }
}

testPokeOuterMemory();

function testOuterMemorySurvivalAcrossGc() {
  var buffer = new ArrayBuffer(kMemSize);
  var checker = genAndGetMain(buffer);
  for (var i = 0; i < 3; i++) {
    print("gc run ", i);
    assertEquals(0, checker(kMemSize - 4));
    gc();
  }
}

testOuterMemorySurvivalAcrossGc();
testOuterMemorySurvivalAcrossGc();
testOuterMemorySurvivalAcrossGc();
testOuterMemorySurvivalAcrossGc();


function testOOBThrows() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  builder.addFunction("geti", kSig_i_ii)
    .addBody([
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprI32LoadMem, 0, 0,
      kExprI32StoreMem, 0, 0,
      kExprGetLocal, 1,
      kExprI32LoadMem, 0, 0,
    ])
    .exportFunc();

  var module = builder.instantiate();
  var offset;

  function read() { return module.exports.geti(0, offset); }
  function write() { return module.exports.geti(offset, 0); }

  for (offset = 0; offset < 65533; offset++) {
    assertEquals(0, read());
    assertEquals(0, write());
  }


  for (offset = 65534; offset < 66536; offset++) {
    assertTraps(kTrapMemOutOfBounds, read);
    assertTraps(kTrapMemOutOfBounds, write);
  }
}

testOOBThrows();
