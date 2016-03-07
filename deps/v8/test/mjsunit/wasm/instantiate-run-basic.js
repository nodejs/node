// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

var kReturnValue = 97;

var kBodySize = 2;
var kNameOffset = 15 + kBodySize + 1;

var data = bytes(
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

assertEquals(kReturnValue, _WASMEXP_.instantiateModule(data).main());
