// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --wasm-lazy-validation

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");
let builder = new WasmModuleBuilder();
let f = builder.addFunction("main", kSig_v_i)
builder.setCompilationPriority(f.index,  1);
assertThrows(() => builder.instantiate(), WebAssembly.CompileError);
