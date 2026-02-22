// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
// Flags: --turboshaft-wasm-in-js-inlining --wasm-inlining-ignore-call-counts
// Flags: --turboshaft-verify-reductions

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_v_v).addBody([]).exportFunc();
const instance = builder.instantiate();
function jsToWasmCall() {
  return instance.exports.f();
}
%PrepareFunctionForOptimization(jsToWasmCall);
assertEquals(undefined, jsToWasmCall());
%OptimizeFunctionOnNextCall(jsToWasmCall);
assertEquals(undefined, jsToWasmCall());
