// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --expose-gc --stress-compaction

load("test/mjsunit/wasm/wasm-constants.js");

var kMemSize = 4096;

function genModule(memory) {
  var kBodySize = 27;
  var kNameMainOffset = 28 + kBodySize + 1;

  var data = bytes(
    kDeclMemory,
    12, 12, 1,                  // memory
    // -- signatures
    kDeclSignatures, 1,
    1, kAstI32, kAstI32,        // int->int
    // -- main function
    kDeclFunctions, 1,
    kDeclFunctionLocals | kDeclFunctionName | kDeclFunctionExport,
    0, 0,
    kNameMainOffset, 0, 0, 0,   // name offset
    1, 0,                       // local int32 count
    0, 0,                       // local int64 count
    0, 0,                       // local float32 count
    0, 0,                       // local float64 count
    kBodySize, 0,               // code size
    // main body: while(i) { if(mem[i]) return -1; i -= 4; } return 0;
    kExprBlock,2,
      kExprLoop,1,
        kExprIf,
          kExprGetLocal,0,
          kExprBr, 0,
            kExprIfElse,
              kExprI32LoadMem,0,kExprGetLocal,0,
              kExprBr,2, kExprI8Const, 255,
              kExprSetLocal,0,
                kExprI32Sub,kExprGetLocal,0,kExprI8Const,4,
      kExprI8Const,0,
    // names
    kDeclEnd,
    'm', 'a', 'i', 'n', 0       //  --
  );

  return _WASMEXP_.instantiateModule(data, null, memory);
}

function testPokeMemory() {
  var module = genModule(null);
  var buffer = module.memory;
  assertEquals(kMemSize, buffer.byteLength);

  var array = new Int8Array(buffer);
  assertEquals(kMemSize, array.length);

  for (var i = 0; i < kMemSize; i++) {
    assertEquals(0, array[i]);
  }

  for (var i = 0; i < 10; i++) {
    assertEquals(0, module.main(kMemSize - 4));

    array[kMemSize/2 + i] = 1;
    assertEquals(0, module.main(kMemSize/2 - 4));
    assertEquals(-1, module.main(kMemSize - 4));

    array[kMemSize/2 + i] = 0;
    assertEquals(0, module.main(kMemSize - 4));
  }
}

testPokeMemory();

function testSurvivalAcrossGc() {
  var checker = genModule(null).main;
  for (var i = 0; i < 5; i++) {
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
  assertEquals(kMemSize, buffer.byteLength);

  var array = new Int8Array(buffer);
  assertEquals(kMemSize, array.length);

  for (var i = 0; i < kMemSize; i++) {
    assertEquals(0, array[i]);
  }

  for (var i = 0; i < 10; i++) {
    assertEquals(0, module.main(kMemSize - 4));

    array[kMemSize/2 + i] = 1;
    assertEquals(0, module.main(kMemSize/2 - 4));
    assertEquals(-1, module.main(kMemSize - 4));

    array[kMemSize/2 + i] = 0;
    assertEquals(0, module.main(kMemSize - 4));
  }
}

testPokeOuterMemory();

function testOuterMemorySurvivalAcrossGc() {
  var buffer = new ArrayBuffer(kMemSize);
  var checker = genModule(buffer).main;
  for (var i = 0; i < 5; i++) {
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
  var kBodySize = 8;
  var kNameMainOffset = 29 + kBodySize + 1;

  var data = bytes(
    kDeclMemory,
    12, 12, 1,                     // memory = 4KB
    // -- signatures
    kDeclSignatures, 1,
    2, kAstI32, kAstI32, kAstI32,  // int->int
    // -- main function
    kDeclFunctions, 1,
    kDeclFunctionLocals | kDeclFunctionName | kDeclFunctionExport,
    0, 0,
    kNameMainOffset, 0, 0, 0,      // name offset
    1, 0,                          // local int32 count
    0, 0,                          // local int64 count
    0, 0,                          // local float32 count
    0, 0,                          // local float64 count
    kBodySize, 0,                  // code size
    // geti: return mem[a] = mem[b]
    kExprI32StoreMem, 0, kExprGetLocal, 0, kExprI32LoadMem, 0, kExprGetLocal, 1,
    // names
    kDeclEnd,
    'g','e','t','i', 0             //  --
  );

  var memory = null;
  var module = _WASMEXP_.instantiateModule(data, null, memory);

  var offset;

  function read() { return module.geti(0, offset); }
  function write() { return module.geti(offset, 0); }

  for (offset = 0; offset < 4092; offset++) {
    assertEquals(0, read());
    assertEquals(0, write());
  }


  for (offset = 4093; offset < 4124; offset++) {
    assertTraps(kTrapMemOutOfBounds, read);
    assertTraps(kTrapMemOutOfBounds, write);
  }
}

testOOBThrows();
