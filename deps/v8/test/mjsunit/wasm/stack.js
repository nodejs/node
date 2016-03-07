// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

function testStack(func, check) {
  var kBodySize = 2;
  var kNameFunOffset = 22 + kBodySize + 1;
  var kNameMainOffset = kNameFunOffset + 4;

  var ffi = new Object();
  ffi.fun = func;

  var data = bytes(
      // signatures
      kDeclSignatures, 1,  //  --
      0, kAstStmt,         // () -> void
      // -- foreign function
      kDeclFunctions, 2,                        //  --
      kDeclFunctionName | kDeclFunctionImport,  // --
      0, 0,                                     //  --
      kNameFunOffset, 0, 0, 0,                  // name offset
      // -- main function
      kDeclFunctionName | kDeclFunctionExport,  // --
      0, 0,                                     //  --
      kNameMainOffset, 0, 0, 0,                 // name offset
      kBodySize, 0,
      // main body
      kExprCallFunction, 0,  // --
      // names
      kDeclEnd,              //  --
      'f', 'u', 'n', 0,      //  --
      'm', 'a', 'i', 'n', 0  //  --
      );

  var module = _WASMEXP_.instantiateModule(data, ffi);

  assertEquals("function", typeof module.main);

  module.main();
  check();
}

// The stack trace contains file path, only keep "stack.js".
function stripPath(s) {
  return s.replace(/[^ (]*stack\.js/g, "stack.js");
}

var stack;
function STACK() {
  var e = new Error();
  stack = e.stack;
}

function check_STACK() {
  assertEquals(expected, stripPath(stack));
}

var expected = "Error\n" +
    // The line numbers below will change as this test gains / loses lines..
    "    at STACK (stack.js:54:11)\n" +  // --
    "    at testStack (stack.js:43:10)\n" +
    // TODO(jfb) Add WebAssembly stack here.
    "    at stack.js:69:1";

testStack(STACK, check_STACK);
