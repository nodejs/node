// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-jspi

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_v_v).addBody([]).exportFunc();
const instance = builder.instantiate();
let promising = WebAssembly.promising(instance.exports.main);
this.console.profile();
promising();
