// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --trace-wasm-generate-compilation-hints --allow-natives-syntax
// Flags: --liftoff --wasm-tier-up --wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
builder.addFunction("main", kSig_v_v)
    .addBody([kExprNop])
    .exportAs("main");

let instance = builder.instantiate();
%WasmTierUpFunction(instance.exports.main);

%GenerateWasmCompilationHints(instance);
