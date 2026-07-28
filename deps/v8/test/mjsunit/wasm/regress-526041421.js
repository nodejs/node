// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-inlining-budget=1

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let sig_index = builder.addType(makeSig([], []));
let f2 = builder.addFunction("f2", sig_index).addBody([]).exportFunc();
let instance = builder.instantiate();
%WasmTierUpFunction(instance.exports.f2);
