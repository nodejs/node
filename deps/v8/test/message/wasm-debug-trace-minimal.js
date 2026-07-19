// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-generic-wrapper

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function js_function() {
  %DebugTraceMinimal();
}

let builder = new WasmModuleBuilder();

let js_index = builder.addImport("q", "js_function", kSig_v_v);

// f3 calls js_function
let f3_index = builder.addFunction("f3", kSig_v_v)
  .addBody([
    kExprCallFunction, js_index
  ])
  .index;

// f2 calls f3
let f2_index = builder.addFunction("f2", kSig_v_v)
  .addBody([
    kExprCallFunction, f3_index
  ])
  .index;

// f1 calls f2
builder.addFunction("f1", kSig_v_v)
  .addBody([
    kExprCallFunction, f2_index
  ])
  .exportFunc();

let instance = builder.instantiate({q: {js_function: js_function}});
instance.exports.f1();
