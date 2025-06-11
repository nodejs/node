// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-custom-descriptors --nowasm-generic-wrappers

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_r_v).exportFunc().addBody([
  kExprRefNull, kExternRefCode,
]);

let instance = builder.instantiate();
assertNull(instance.exports.main());
