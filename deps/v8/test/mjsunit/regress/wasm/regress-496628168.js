// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-wasm-generate-compilation-hints
// Flags: --wasm-tier-mask-for-testing=1 --liftoff --wasm-lazy-compilation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_v_v).addBody([kExprNop]).exportAs('main');
const instance = builder.instantiate();
instance.exports.main(); // Trigger compilation
%GenerateWasmCompilationHints(instance);
