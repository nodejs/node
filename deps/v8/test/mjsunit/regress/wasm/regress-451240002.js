// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-wasm-in-js-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_v_v)
.addBody([
    kExprI32Const, 0,
    kExprBrTable, 0, 0,
]).exportFunc();

const instance = builder.instantiate();

function f() {
  return instance.exports.f();
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
