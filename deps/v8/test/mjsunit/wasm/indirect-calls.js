// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

var module = (function () {
  var kFuncWithBody = 9;
  var kFuncImported = 7;
  var kBodySize1 = 5;
  var kBodySize2 = 8;
  var kFuncTableSize = 8;
  var kSubOffset = 13 + kFuncWithBody + kBodySize1 + kFuncImported + kFuncWithBody + kBodySize2 + kFuncTableSize + 1;
  var kAddOffset = kSubOffset + 4;
  var kMainOffset = kAddOffset + 4;

  var ffi = new Object();
  ffi.add = (function(a, b) { return a + b | 0; });

  return _WASMEXP_.instantiateModule(bytes(
    // -- signatures
    kDeclSignatures, 2,
    2, kAstI32, kAstI32, kAstI32, // int, int -> int
    3, kAstI32, kAstI32, kAstI32, kAstI32, // int, int, int -> int
    // -- function #0 (sub)
    kDeclFunctions, 3,
    kDeclFunctionName,
    0, 0,                         // signature offset
    kSubOffset, 0, 0, 0,          // name offset
    kBodySize1, 0,                // body size
    kExprI32Sub,                  // --
    kExprGetLocal, 0,             // --
    kExprGetLocal, 1,             // --
    // -- function #1 (add)
    kDeclFunctionName | kDeclFunctionImport,
    0, 0,                         // signature offset
    kAddOffset, 0, 0, 0,          // name offset
    // -- function #2 (main)
    kDeclFunctionName | kDeclFunctionExport,
    1, 0,                         // signature offset
    kMainOffset, 0, 0, 0,         // name offset
    kBodySize2, 0,                // body size
    kExprCallIndirect, 0,
    kExprGetLocal, 0,
    kExprGetLocal, 1,
    kExprGetLocal, 2,
    // -- function table
    kDeclFunctionTable,
    3,
    0, 0,
    1, 0,
    2, 0,
    kDeclEnd,
    's', 'u', 'b', 0,              // name
    'a', 'd', 'd', 0,              // name
    'm', 'a', 'i', 'n', 0          // name
  ), ffi);
})();

// Check the module exists.
assertFalse(module === undefined);
assertFalse(module === null);
assertFalse(module === 0);
assertEquals("object", typeof module);
assertEquals("function", typeof module.main);

assertEquals(5, module.main(0, 12, 7));
assertEquals(19, module.main(1, 12, 7));

assertTraps(kTrapFuncSigMismatch, "module.main(2, 12, 33)");
assertTraps(kTrapFuncInvalid, "module.main(3, 12, 33)");
