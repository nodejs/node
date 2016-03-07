// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

function instantiate(sig, body) {
  var module = new Array();
  module = module.concat([
    // -- signatures
    kDeclSignatures, 1,
  ]);
  module = module.concat(sig);
  module = module.concat([
    // -- functions
    kDeclFunctions, 1,
    0,                 // decl flags
    0, 0,              // signature
    body.length, 0,    // body size
  ]);
  module = module.concat(body);
  module = module.concat([
    // -- declare start function
    kDeclStartFunction,
    0
  ]);

  var data = bytes.apply(this, module);
  print(module);
  print(data instanceof ArrayBuffer);
  print(data.byteLength);
  return _WASMEXP_.instantiateModule(data);
}

function assertFails(sig, body) {
  try {
    var module = instantiate(sig, body);
    print("expected failure, but passes");
    assertFalse(true);
  } catch (expected) {
    print("ok: " + expected);
  }
}

function assertVerifies(sig, body) {
  var module = instantiate(sig, body);
  assertFalse(module === undefined);
  assertFalse(module === null);
  assertFalse(module === 0);
  assertEquals("object", typeof module);
  return module;
}

assertVerifies([0, kAstStmt], [kExprNop]);
assertVerifies([0, kAstI32], [kExprI8Const, 0]);

// Arguments aren't allow to start functions.
assertFails([1, kAstI32, kAstI32], [kExprGetLocal, 0]);
assertFails([2, kAstI32, kAstI32, kAstF32], [kExprGetLocal, 0]);
assertFails([3, kAstI32, kAstI32, kAstF32, kAstF64], [kExprGetLocal, 0]);

(function testInvalidIndex() {
  var kBodySize = 1;
  var data = bytes(
    // -- signatures
    kDeclSignatures, 1,
    0, kAstStmt,
    // -- functions
    kDeclFunctions, 1,
    0,                 // decl flags
    0, 0,              // signature
    kBodySize, 0,      // body size
    kExprNop,          // body
    // -- declare start function
    kDeclStartFunction,
    1
  );

  assertThrows(function() { _WASMEXP_.instantiateModule(data); });
})();


(function testTwoStartFuncs() {
  var kBodySize = 1;
  var data = bytes(
    // -- signatures
    kDeclSignatures, 1,
    0, kAstStmt,
    // -- functions
    kDeclFunctions, 1,
    0,                 // decl flags
    0, 0,              // signature
    kBodySize, 0,      // body size
    kExprNop,          // body
    // -- declare start function
    kDeclStartFunction,
    0,
    // -- declare start function
    kDeclStartFunction,
    0
  );

  assertThrows(function() { _WASMEXP_.instantiateModule(data); });
})();


(function testRun() {
  var kBodySize = 6;

  var data = bytes(
    kDeclMemory,
    12, 12, 1,                  // memory
    // -- signatures
    kDeclSignatures, 1,
    0, kAstStmt,
    // -- start function
    kDeclFunctions, 1,
    0,                          // decl flags
    0, 0,                       // signature
    kBodySize, 0,               // code size
    // -- start body
    kExprI32StoreMem, 0, kExprI8Const, 0, kExprI8Const, 77,
    // -- declare start function
    kDeclStartFunction,
    0
  );

  var module = _WASMEXP_.instantiateModule(data);
  var memory = module.memory;
  var view = new Int8Array(memory);
  assertEquals(77, view[0]);
})();

(function testStartFFI() {
  var kBodySize = 2;
  var kNameOffset = 4 + 9 + 7 + 3;

  var data = bytes(
    // -- signatures
    kDeclSignatures, 1,
    0, kAstStmt,
    // -- imported function
    kDeclFunctions, 2,
    kDeclFunctionImport | kDeclFunctionName,     // decl flags
    0, 0,                       // signature
    kNameOffset, 0, 0, 0,
    // -- start function
    0,                          // decl flags
    0, 0,                       // signature
    kBodySize, 0,               // code size
    // -- start body
    kExprCallFunction, 0,
    // -- declare start function
    kDeclStartFunction,
    1,
    kDeclEnd,
    'f', 'o', 'o', 0
  );

  var ranned = false;
  var ffi = new Object();
  ffi.foo = function() {
    print("we ranned at stert!");
    ranned = true;
  }
  var module = _WASMEXP_.instantiateModule(data, ffi);
  var memory = module.memory;
  var view = new Int8Array(memory);
  assertTrue(ranned);
})();
