// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-wasm-in-js-inlining
// Flags: --wasm-lazy-validation --wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_l_v).addBody([]).exportFunc();
let instance = builder.instantiate();

function g() {
  instance.exports.f();
}

%PrepareFunctionForOptimization(g);
try {
  g();
} catch (e) { }

%OptimizeFunctionOnNextCall(g);
// This should fail validation only here due to lazy validation.
assertThrows(function() { g(); }, WebAssembly.CompileError);
