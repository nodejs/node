// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

function testCallFFI(ffi) {
  var builder = new WasmModuleBuilder();

  var sig_index = kSig_i_dd;
  builder.addImport("", "fun", sig_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprGetLocal, 0,              // --
      kExprGetLocal, 1,              // --
      kExprCallFunction, 0, // --
    ])    // --
    .exportFunc();

  var module = builder.instantiate(ffi);
}

// everything is good.
(function() {
  var ffi = {"": {fun: function(a, b) { print(a, b); }}}
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


(function I64InSignatureThrows() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  builder.addFunction("function_with_invalid_signature", kSig_l_ll)
    .addBody([           // --
      kExprGetLocal, 0,  // --
      kExprGetLocal, 1,  // --
      kExprI64Sub])      // --
    .exportFunc()

  var module = builder.instantiate();

  assertThrows(function() {
      module.exports.function_with_invalid_signature(33, 88);
    }, TypeError);
})();

(function I64ParamsInSignatureThrows() {
  var builder = new WasmModuleBuilder();

  builder.addMemory(1, 1, true);
  builder.addFunction("function_with_invalid_signature", kSig_i_l)
    .addBody([
       kExprGetLocal, 0,
       kExprI32ConvertI64
     ])
    .exportFunc()

  var module = builder.instantiate();

  assertThrows(function() {
      module.exports.function_with_invalid_signature(33);
    }, TypeError);
})();

(function I64JSImportThrows() {
  var builder = new WasmModuleBuilder();
  var sig_index = builder.addType(kSig_i_i);
  var sig_i64_index = builder.addType(kSig_i_l);
  var index = builder.addImport("", "func", sig_i64_index);
  builder.addFunction("main", sig_index)
    .addBody([
      kExprGetLocal, 0,
      kExprI64SConvertI32,
      kExprCallFunction, index  // --
    ])        // --
    .exportFunc();
  var func = function() {return {};};
  var main = builder.instantiate({"": {func: func}}).exports.main;
  assertThrows(function() {
    main(13);
  }, TypeError);
})();

(function ImportI64ParamWithF64ReturnThrows() {
  // This tests that we generate correct code by using the correct return
  // register. See bug 6096.
  var builder = new WasmModuleBuilder();
  builder.addImport('', 'f', makeSig([kWasmI64], [kWasmF64]));
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprI64Const, 0, kExprCallFunction, 0, kExprDrop])
      .exportFunc();
  var instance = builder.instantiate({'': {f: i => i}});

  assertThrows(() => instance.exports.main(), TypeError);
})();

(function ImportI64Return() {
  // This tests that we generate correct code by using the correct return
  // register(s). See bug 6104.
  var builder = new WasmModuleBuilder();
  builder.addImport('', 'f', makeSig([], [kWasmI64]));
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprCallFunction, 0, kExprDrop])
      .exportFunc();
  var instance = builder.instantiate({'': {f: () => 1}});

  assertThrows(() => instance.exports.main(), TypeError);
})();

(function ImportSymbolToNumberThrows() {
  var builder = new WasmModuleBuilder();
  var index = builder.addImport("", "func", kSig_i_v);
  builder.addFunction("main", kSig_i_v)
      .addBody([kExprCallFunction, 0])
      .exportFunc();
  var func = () => Symbol();
  var main = builder.instantiate({"": {func: func}}).exports.main;
  assertThrows(() => main(), TypeError);
})();
