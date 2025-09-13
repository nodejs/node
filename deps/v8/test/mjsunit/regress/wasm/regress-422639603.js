// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-wasm-in-js-inlining

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("f", kSig_v_v)
.addBody([
    kExprTryTable, kWasmVoid, 0,
    kExprEnd,
]).exportFunc();

const instance = builder.instantiate();

function f() {
  for (const v2 in [128]) {
    instance.exports.f();
    for (let i = 0; i < 5; i++) {
      %OptimizeOsr();
    }
  }
}

%PrepareFunctionForOptimization(f);
f();
