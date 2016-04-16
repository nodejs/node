// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

var kReturnValue = 117;

var kBodySize = 2;
var kNameOffset = 19 + kBodySize + 1;

var data = bytes(
  // -- memory
  kDeclMemory,
  10, 10, 1,
  // -- signatures
  kDeclSignatures, 1,
  0, kAstI32,                 // signature: void -> int
  // -- main function
  kDeclFunctions, 1,
  kDeclFunctionName | kDeclFunctionExport,
  0, 0,                       // signature index
  kNameOffset, 0, 0, 0,       // name offset
  kBodySize, 0,               // body size
  // -- body
  kExprI8Const,               // --
  kReturnValue,               // --
  kDeclEnd,
  'm', 'a', 'i', 'n', 0       // name
);

var module = _WASMEXP_.instantiateModule(data);

// Check the module exists.
assertFalse(module === undefined);
assertFalse(module === null);
assertFalse(module === 0);
assertEquals("object", typeof module);

// Check the memory is an ArrayBuffer.
var mem = module.memory;
assertFalse(mem === undefined);
assertFalse(mem === null);
assertFalse(mem === 0);
assertEquals("object", typeof mem);
assertTrue(mem instanceof ArrayBuffer);
for (var i = 0; i < 4; i++) {
  module.memory = 0;  // should be ignored
  assertEquals(mem, module.memory);
}

assertEquals(1024, module.memory.byteLength);

// Check the properties of the main function.
assertFalse(module.main === undefined);
assertFalse(module.main === null);
assertFalse(module.main === 0);
assertEquals("function", typeof module.main);

assertEquals(kReturnValue, module.main());
