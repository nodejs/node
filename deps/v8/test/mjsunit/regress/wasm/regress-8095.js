// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// Prepare a special error object to throw.
var error = new Error("my error");
error.__proto__ = new Proxy(new Error(), {
  has(target, property, receiver) {
    assertUnreachable();
  }
});

// Throw it through a WebAssembly module.
var builder = new WasmModuleBuilder();
builder.addImport('mod', 'fun', kSig_v_v);
builder.addFunction("funnel", kSig_v_v)
       .addBody([kExprCallFunction, 0])
       .exportFunc();
var instance = builder.instantiate({ mod: {fun: function() { throw error }}});
assertThrows(instance.exports.funnel, Error);
