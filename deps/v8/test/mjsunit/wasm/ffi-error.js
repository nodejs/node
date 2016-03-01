// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");

function testCallFFI(ffi) {
  var kBodySize = 6;
  var kNameAddOffset = 28 + kBodySize + 1;
  var kNameMainOffset = kNameAddOffset + 4;

  var data = bytes(
    kDeclMemory,
    12, 12, 1,                  // memory
    // -- signatures
    kDeclSignatures, 1,
    2, kAstI32, kAstF64, kAstF64, // (f64,f64)->int
    // -- foreign function
    kDeclFunctions, 2,
    kDeclFunctionName | kDeclFunctionImport,
    0, 0,                       // signature index
    kNameAddOffset, 0, 0, 0,    // name offset
    // -- main function
    kDeclFunctionName | kDeclFunctionExport,
    0, 0,                       // signature index
    kNameMainOffset, 0, 0, 0,   // name offset
    kBodySize, 0,
    // main body
    kExprCallFunction, 0,       // --
    kExprGetLocal, 0,           // --
    kExprGetLocal, 1,           // --
    // names
    kDeclEnd,
    'f', 'u', 'n', 0,           //  --
    'm', 'a', 'i', 'n', 0       //  --
  );

  print("instantiate FFI");
  var module = _WASMEXP_.instantiateModule(data, ffi);
}

// everything is good.
(function() {
  var ffi = new Object();
  ffi.fun = function(a, b) { print(a, b); }
  testCallFFI(ffi);
})();


// FFI object should be an object.
assertThrows(function() {
  var ffi = 0;
  testCallFFI(ffi);
});


// FFI object should have a "fun" property.
assertThrows(function() {
  var ffi = new Object();
  testCallFFI(ffi);
});


// "fun" should be a JS function.
assertThrows(function() {
  var ffi = new Object();
  ffi.fun = new Object();
  testCallFFI(ffi);
});


// "fun" should be a JS function.
assertThrows(function() {
  var ffi = new Object();
  ffi.fun = 0;
  testCallFFI(ffi);
});
