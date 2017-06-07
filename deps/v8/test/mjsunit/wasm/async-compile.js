// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --allow-natives-syntax

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

let ok_buffer = (() => {
  var builder = new WasmModuleBuilder();
  builder.addFunction("f", kSig_i_v)
    .addBody([kExprI32Const, 42])
    .exportAs("f");
  return builder.toBuffer();
})();

// The OK buffer validates and can be made into a module.
assertTrue(WebAssembly.validate(ok_buffer));
let ok_module = new WebAssembly.Module(ok_buffer);
assertTrue(ok_module instanceof WebAssembly.Module);

// The bad buffer does not validate and cannot be made into a module.
let bad_buffer = new ArrayBuffer(0);
assertFalse(WebAssembly.validate(bad_buffer));
assertThrows(() => new WebAssembly.Module(bad_buffer), WebAssembly.CompileError);

function checkModule(module) {
  assertTrue(module instanceof WebAssembly.Module);
}

function checkCompileError(ex) {
  assertTrue(ex instanceof WebAssembly.CompileError);
}

let kNumCompiles = 3;

// Three compilations of the OK module should succeed.
for (var i = 0; i < kNumCompiles; i++) {
  assertPromiseResult(WebAssembly.compile(ok_buffer), checkModule,
              (ex) => assertUnreachable);
}

// Three compilations of the bad module should fail.
for (var i = 0; i < kNumCompiles; i++) {
  assertPromiseResult(WebAssembly.compile(bad_buffer),
              (module) => assertUnreachable,
              checkCompileError);
}
