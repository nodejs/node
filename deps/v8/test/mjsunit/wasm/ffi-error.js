// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function testCallFFI(ffi) {
  var builder = new WasmModuleBuilder();

  var sig_index = kSig_i_dd;
  builder.addImport("fun", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprGetLocal, 0,              // --
      kExprGetLocal, 1,              // --
      kExprCallFunction, kArity2, 0, // --
    ])    // --
    .exportFunc();

  var module = builder.instantiate(ffi);
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
